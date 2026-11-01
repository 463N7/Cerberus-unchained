#undef NDEBUG
#include "cerberus_protocol.h"
#include "wifi_channel_survey_metrics.h"
#include "esp_err.h"
#include "display.h"
#include "ui_widgets.h"
#include <assert.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; assert(x); } while (0)
#define portMAX_DELAY 0
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
#define WIFI_PROMIS_FILTER_MASK_MGMT 1U
#define WIFI_PROMIS_FILTER_MASK_CTRL 2U
#define WIFI_PROMIS_FILTER_MASK_DATA 4U
#define WIFI_BAND_2G 1
#define WIFI_BAND_5G 2
#define WIFI_SECOND_CHAN_NONE 0
#define WIFI_PKT_MISC 3
typedef unsigned wifi_promiscuous_pkt_type_t;
typedef struct { unsigned filter_mask; } wifi_promiscuous_filter_t;
typedef struct { struct { unsigned rx_state,sig_len,channel; int8_t rssi; } rx_ctrl; } wifi_promiscuous_pkt_t;
static int mutex,task,mux;
static void *scanner_mutex=&mutex,*channel_survey_task_handle=&task;
static bool locked,critical,scanner_ready=true,station_reserved;
static cerberus_wifi_scan_status_payload_t scan_status;
static cerberus_wifi_connection_status_payload_t connection_status;
static cerberus_wifi_defense_status_payload_t defense_status;
static cerberus_ble_scan_status_payload_t ble_status;
static cerberus_ble_connection_status_payload_t ble_connection;
static atomic_bool mac_radio_reserved,csi_radio_reserved,remote_id_radio_reserved,
    capture_radio_reserved,packet_monitor_radio_reserved;
/* Same production storage types, with no duplicate result bank. */
static cerberus_wifi_channel_survey_status_payload_t channel_survey_status;
static cerberus_wifi_channel_survey_result_payload_t channel_survey_results[CERBERUS_WIFI_CHANNEL_SURVEY_MAX_RESULTS];
static uint8_t channel_survey_channels[CERBERUS_WIFI_CHANNEL_SURVEY_MAX_RESULTS];
static int64_t channel_survey_started_us,clock_us;
static atomic_bool channel_survey_cancelled,channel_survey_radio_reserved;
#define channel_survey_mux mux
static bool channel_survey_receiving;
static uint8_t channel_survey_receive_channel;
static uint32_t channel_survey_packet_count;
static int64_t channel_survey_rssi_sum;
static int8_t channel_survey_strongest_rssi;
static unsigned notifications,enable_calls,disable_calls,channel_calls,delay_calls,worker_notifications;
static unsigned cancel_after_delay;
static int filter_error,enable_error,disable_error,channel_error;
static bool sdk_promiscuous,worker_running,run_worker_during_wait,run_worker_on_notify,hold_worker;
static jmp_buf worker_exit;
static void run_worker(void);
static void cerberus_wifi_scanner_get_status(cerberus_wifi_scan_status_payload_t *out);
static void xSemaphoreTake(void *m,unsigned t){(void)t;CHECK(m==scanner_mutex&&!locked);locked=true;}
static void xSemaphoreGive(void *m){CHECK(m==scanner_mutex&&locked);locked=false;}
#define portENTER_CRITICAL(m) do { CHECK((m)==&mux&&!critical);critical=true; } while(0)
#define portEXIT_CRITICAL(m) do { CHECK((m)==&mux&&critical);critical=false; } while(0)
static int64_t esp_timer_get_time(void){return clock_us;}
static bool cerberus_c5_test_running(void){return false;}
static bool cerberus_ieee802154_scanner_is_running(void){return false;}
static bool cerberus_wifi_defense_survey_is_running(void){return defense_status.state==CERBERUS_WIFI_DEFENSE_RUNNING;}
static bool cerberus_wifi_station_running(void){return station_reserved;}
static void cerberus_ble_scanner_get_status(cerberus_ble_scan_status_payload_t *out){*out=ble_status;}
static void cerberus_ble_connection_get_status(cerberus_ble_connection_status_payload_t *out){*out=ble_connection;}
static void cerberus_wifi_connection_get_status(cerberus_wifi_connection_status_payload_t *out){*out=connection_status;}
static void xTaskNotifyGive(void *t){CHECK(t==channel_survey_task_handle);++notifications;if(run_worker_on_notify)run_worker();}
static void ulTaskNotifyTake(int clear,unsigned timeout)
{(void)timeout;CHECK(clear==pdTRUE);if(worker_notifications++)longjmp(worker_exit,1);}
static int esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t *f)
{CHECK(f->filter_mask==7U);return filter_error;}
static int esp_wifi_set_promiscuous(bool on)
{CHECK(!locked);if(on){++enable_calls;if(!enable_error)sdk_promiscuous=true;return enable_error;}
 ++disable_calls;if(!disable_error)sdk_promiscuous=false;return disable_error;}
static int esp_wifi_set_band(int band){CHECK(band==1||band==2);return channel_error;}
static int esp_wifi_set_channel(uint8_t channel,int second)
{CHECK(channel&&second==0);++channel_calls;return channel_error;}
static void vTaskDelay(unsigned ms)
{
    CHECK(!locked);clock_us+=(int64_t)ms*1000;
    if(worker_running){
        ++delay_calls;
        cerberus_wifi_scan_status_payload_t projected;cerberus_wifi_scanner_get_status(&projected);
        CHECK(projected.state==CERBERUS_WIFI_SCAN_RUNNING);
        if(cancel_after_delay&&delay_calls>=cancel_after_delay)
            atomic_store(&channel_survey_cancelled,true);
    }else if(run_worker_during_wait&&!hold_worker)run_worker();
}
#include "survey_admission.h"
#include "wifi_channel_survey_service.h"
static void run_worker(void)
{
    CHECK(!worker_running);worker_running=true;worker_notifications=0;
    if(!setjmp(worker_exit))channel_survey_task(NULL);
    worker_running=false;CHECK(!locked&&!critical);
}

/* Actual main dispatch and S3 transport validators, with only frame exchange
 * and link mutex boundaries replaced. A lost reply can occur after mutation. */
static uint8_t response_bytes[22],response_command,response_sequence;
static size_t response_size;
static void prepare_frame_response(cerberus_spi_command_t command,uint8_t sequence,const void *data,size_t size)
{CHECK(size<=sizeof(response_bytes));memcpy(response_bytes,data,size);response_size=size;response_command=command;response_sequence=sequence;}
typedef struct { uint8_t command,sequence,payload_length,payload[22]; } test_request_t;
static void dispatch(const test_request_t *request)
{switch(request->command){
#include "survey_dispatch.h"
default:CHECK(false);}}
static int link_mutex_object;
static void *link_mutex=&link_mutex_object;
static bool link_locked;
static uint8_t drop_after_command,fail_before_command;
static unsigned starts,stops,status_queries,reads;
static int take_link_mutex(unsigned timeout){CHECK(timeout==250&&!link_locked);link_locked=true;return pdTRUE;}
static void give_link_mutex(void){CHECK(link_locked);link_locked=false;}
static esp_err_t exchange_v2_locked(cerberus_spi_command_t command,const void *request,size_t length,void *out,size_t size)
{
    CHECK(link_locked&&length<=22);if(fail_before_command==command){fail_before_command=0;return ESP_ERR_TIMEOUT;}
    if(command==CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_START)++starts;
    if(command==CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_STOP)++stops;
    if(command==CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_STATUS)++status_queries;
    if(command==CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_RESULT)++reads;
    test_request_t q={.command=command,.sequence=17,.payload_length=(uint8_t)length};
    if(length)memcpy(q.payload,request,length);response_size=0;dispatch(&q);
    if(drop_after_command==command){drop_after_command=0;return ESP_ERR_TIMEOUT;}
    CHECK(response_command==command&&response_sequence==17&&response_size==size);
    memcpy(out,response_bytes,size);return ESP_OK;
}
typedef struct { uint32_t firmware_version; } cerberus_c5_status_t;
static cerberus_c5_status_t c5_status={.firmware_version=71};
static esp_err_t c5_spi_link_get_status(cerberus_c5_status_t *out){*out=c5_status;return ESP_OK;}
#include "survey_link.h"
#define UI_WIFI_VISIBLE_ROWS 4
enum { UI_WIFI_MODE_MENU,UI_WIFI_MODE_CHANNELS_24,UI_WIFI_MODE_CHANNELS_5 };
enum { UI_PAGE_WIFI,UI_PAGE_OTHER,UI_PAGE_BLE,UI_PAGE_IEEE802154 };
static unsigned wifi_mode,current_page,draws;
static bool screen_asleep;
#define ESP_ERROR_CHECK(x) CHECK((x)==ESP_OK)
static esp_err_t draw_detail_page(void){++draws;return ESP_OK;}
#include "survey_ui.h"
#define UI_HEADER_HEIGHT 48U
#define ESP_RETURN_ON_ERROR(x,tag,message) do { esp_err_t e=(x);if(e!=ESP_OK)return e; } while(0)
static char drawn[4096];
static uint16_t pixels[480][320];
#include "display_font.h"
static void text(uint16_t x,uint16_t y,const char *s)
{CHECK(x<320&&y<480&&strlen(drawn)+strlen(s)+2<sizeof(drawn));strcat(drawn,s);strcat(drawn,"\n");}
static const char *esp_err_to_name(esp_err_t e){return e==ESP_FAIL?"ESP_FAIL":e==ESP_ERR_TIMEOUT?"ESP_ERR_TIMEOUT":"ESP_ERR_INVALID_STATE";}
esp_err_t cerberus_display_fill_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c)
{CHECK(x+w<=320&&y+h<=480);for(unsigned iy=y;iy<y+h;++iy)for(unsigned ix=x;ix<x+w;++ix)pixels[iy][ix]=c;return ESP_OK;}
esp_err_t cerberus_display_draw_text(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale)
{
    CHECK((scale==1||scale==2)&&x+strlen(s)*6*scale<=320&&y+7*scale<=480);text(x,y,s);
    for(size_t i=0;s[i];++i){const uint8_t *font=font5x7((unsigned char)s[i]);
        for(unsigned cy=0;cy<7;++cy)for(unsigned cx=0;cx<6;++cx)
            for(unsigned sy=0;sy<scale;++sy)for(unsigned sx=0;sx<scale;++sx)
                pixels[y+cy*scale+sy][x+(i*6+cx)*scale+sx]=cx<5&&(font[cy]&(1U<<(4-cx)))?fg:bg;
    }
    return ESP_OK;
}
#include "survey_ui_layout.h"
#include "survey_ui_draw.h"
static void draw_expect(const char *state,const char *button)
{drawn[0]=0;unsigned before=status_queries;CHECK(draw_wifi_channel_survey_live()==ESP_OK);
 CHECK(strstr(drawn,state)&&strstr(drawn,button)&&status_queries==before);}
static void ui_tick(uint32_t now_ms){
#include "survey_ui_tick.h"
}
static void ui_back(void){
#include "survey_ui_back.h"
}

static void reset(void)
{
    CHECK(!locked&&!critical&&!link_locked&&!worker_running);
    memset(&scan_status,0,sizeof(scan_status));memset(&connection_status,0,sizeof(connection_status));
    memset(&ble_status,0,sizeof(ble_status));memset(&ble_connection,0,sizeof(ble_connection));
    memset(&defense_status,0,sizeof(defense_status));
    memset(&channel_survey_status,0,sizeof(channel_survey_status));
    channel_survey_status.dwell_ms=100;memset(channel_survey_results,0,sizeof(channel_survey_results));
    atomic_store(&channel_survey_cancelled,false);atomic_store(&channel_survey_radio_reserved,false);
    scanner_ready=true;station_reserved=false;notifications=enable_calls=disable_calls=channel_calls=delay_calls=0;
    filter_error=enable_error=disable_error=channel_error=0;sdk_promiscuous=false;
    cancel_after_delay=11;run_worker_during_wait=true;run_worker_on_notify=false;hold_worker=false;clock_us=1000000;
    starts=stops=status_queries=reads=draws=0;fail_before_command=drop_after_command=0;
    memset(&wifi_channel_survey_status,0,sizeof(wifi_channel_survey_status));
    memset(wifi_channel_survey_rows,0,sizeof(wifi_channel_survey_rows));
    wifi_channel_survey_attempted=wifi_channel_survey_claimed=wifi_channel_survey_cleanup_requested=false;
    wifi_channel_survey_cleanup_polled=false;wifi_channel_survey_claim_generation=wifi_channel_survey_claim_band=0;
    wifi_channel_survey_cleanup_last_poll=wifi_channel_survey_last_poll=0;wifi_channel_survey_error=ESP_ERR_INVALID_STATE;
    wifi_channel_survey_page=0;wifi_mode=UI_WIFI_MODE_CHANNELS_24;current_page=UI_PAGE_WIFI;screen_asleep=false;
    c5_status.firmware_version=CERBERUS_WIFI_CHANNEL_SURVEY_CLEANUP_MIN_FIRMWARE;
}
static void native_lifecycle(void)
{
    reset();CHECK(cerberus_wifi_channel_survey_start(1)==ESP_OK&&notifications==1);
    CHECK(ble_other_radio_active()&&!atomic_load(&channel_survey_radio_reserved));
    CHECK(cerberus_wifi_channel_survey_preempt()==ESP_OK);
    CHECK(!enable_calls&&!ble_other_radio_active()&&channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_IDLE);
    CHECK(cerberus_wifi_channel_survey_start(2)==ESP_OK);disable_error=ESP_FAIL;run_worker();
    CHECK(enable_calls==1&&channel_calls>=2&&sdk_promiscuous);
    CHECK(channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_ERROR&&atomic_load(&channel_survey_radio_reserved));
    CHECK(ble_other_radio_active()&&cerberus_wifi_channel_survey_start(1)==ESP_ERR_INVALID_STATE);
    unsigned before=disable_calls;CHECK(cerberus_wifi_channel_survey_preempt()==ESP_FAIL&&disable_calls==before+1);
    CHECK(ble_other_radio_active()&&sdk_promiscuous);
    disable_error=0;CHECK(cerberus_wifi_channel_survey_preempt()==ESP_OK);
    CHECK(!ble_other_radio_active()&&!sdk_promiscuous&&!atomic_load(&channel_survey_radio_reserved));
    CHECK(cerberus_wifi_channel_survey_start(1)==ESP_OK);filter_error=ESP_FAIL;run_worker();
    CHECK(channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_ERROR&&!ble_other_radio_active());
    CHECK(cerberus_wifi_channel_survey_preempt()==ESP_OK); /* No held cleanup remains. */
    CHECK(channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_IDLE&&!channel_survey_status.error_code);
    reset();CHECK(cerberus_wifi_channel_survey_start(1)==ESP_OK);hold_worker=true;
    int64_t before_time=clock_us;CHECK(cerberus_wifi_channel_survey_preempt()==ESP_ERR_TIMEOUT);
    CHECK(clock_us-before_time==1000000&&ble_other_radio_active());hold_worker=false;
    CHECK(cerberus_wifi_channel_survey_preempt()==ESP_OK&&!ble_other_radio_active());
    /* Actual receive callback remains bounded to the current channel. */
    wifi_promiscuous_pkt_t packet={.rx_ctrl={.sig_len=24,.channel=6,.rssi=-40}};
    channel_survey_receiving=true;channel_survey_receive_channel=6;channel_survey_packet_count=0;
    channel_survey_receive(&packet,0);CHECK(channel_survey_packet_count==1);
    packet.rx_ctrl.channel=11;channel_survey_receive(&packet,0);CHECK(channel_survey_packet_count==1);
    channel_survey_receiving=false;
}
static void ui_failed_cleanup_and_hidden(void)
{
    reset();wifi_channel_survey_begin_ui();CHECK(starts==1&&wifi_channel_survey_claimed);
    disable_error=ESP_FAIL;run_worker();clock_us+=700000;ui_tick((uint32_t)(clock_us/1000));
    CHECK(wifi_channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_ERROR);
    draw_expect("ERROR","STOP");
    ui_back();CHECK(wifi_mode==UI_WIFI_MODE_CHANNELS_24&&wifi_channel_survey_claimed&&stops==1);
    CHECK(wifi_channel_survey_cleanup_requested&&ble_other_radio_active());
    draw_expect("STOPPING","RETRY STOP");
    unsigned before=stops;clock_us+=699000;ui_tick((uint32_t)(clock_us/1000));CHECK(stops==before);
    clock_us+=1000;ui_tick((uint32_t)(clock_us/1000));CHECK(stops==before+1);
    CHECK(starts==1&&wifi_channel_survey_claimed);
    disable_error=0;clock_us+=700000;ui_tick((uint32_t)(clock_us/1000));
    CHECK(!wifi_channel_survey_claimed&&!ble_other_radio_active()&&starts==1);
    draw_expect("PAUSED","AP LIST");
    ui_back();CHECK(wifi_mode==UI_WIFI_MODE_MENU&&starts==1);
    wifi_mode=UI_WIFI_MODE_CHANNELS_5;wifi_channel_survey_begin_ui();CHECK(starts==2);
    screen_asleep=true;ui_tick((uint32_t)(clock_us/1000));CHECK(!wifi_channel_survey_claimed&&!ble_other_radio_active());
    screen_asleep=false;ui_tick((uint32_t)(clock_us/1000)+700);CHECK(starts==2); /* Waking is not Start. */
    wifi_channel_survey_begin_ui();CHECK(starts==3);
    current_page=UI_PAGE_OTHER;clock_us+=700000;ui_tick((uint32_t)(clock_us/1000));
    CHECK(!wifi_channel_survey_claimed&&starts==3);
}
static void ui_uncertain_and_replacement(void)
{
    reset();drop_after_command=CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_START;
    wifi_channel_survey_begin_ui();CHECK(wifi_channel_survey_error==ESP_ERR_TIMEOUT&&wifi_channel_survey_claimed);
    CHECK(ble_other_radio_active()&&starts==1);
    fail_before_command=CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_STATUS;
    CHECK(!wifi_channel_survey_stop_ui()&&wifi_channel_survey_claimed&&!stops);
    CHECK(wifi_channel_survey_stop_ui()&&!wifi_channel_survey_claimed&&stops==1);
    CHECK(!ble_other_radio_active());
    reset();wifi_channel_survey_begin_ui();CHECK(wifi_channel_survey_claimed);
    CHECK(cerberus_wifi_channel_survey_preempt()==ESP_OK);CHECK(cerberus_wifi_channel_survey_start(2)==ESP_OK);
    unsigned before=stops;CHECK(wifi_channel_survey_stop_ui());
    CHECK(stops==before&&!wifi_channel_survey_claimed&&ble_other_radio_active());
    CHECK(channel_survey_status.band==2&&channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_RUNNING);
    /* A fresh page must not adopt an already running foreign survey. */
    wifi_channel_survey_begin_ui();CHECK(starts==1&&!wifi_channel_survey_claimed);
    CHECK(cerberus_wifi_channel_survey_preempt()==ESP_OK);wifi_channel_survey_begin_ui();CHECK(starts==2&&wifi_channel_survey_claimed);
    hold_worker=true;CHECK(!wifi_channel_survey_stop_ui());
    wifi_channel_survey_begin_ui();CHECK(starts==2&&wifi_channel_survey_claimed); /* repeated Stop */
    hold_worker=false;CHECK(wifi_channel_survey_stop_ui());
    channel_survey_status.error_code=ESP_FAIL; /* IDLE with historical error is released. */
    CHECK(wifi_channel_survey_stop_ui());
    reset();wifi_channel_survey_begin_ui();filter_error=ESP_FAIL;run_worker();
    clock_us+=700000;ui_tick((uint32_t)(clock_us/1000));
    CHECK(wifi_channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_ERROR&&wifi_channel_survey_claimed);
    wifi_channel_survey_begin_ui();CHECK(!wifi_channel_survey_claimed&&stops==1&&starts==1);
    CHECK(channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_IDLE);
    wifi_channel_survey_begin_ui();CHECK(starts==2&&wifi_channel_survey_claimed);
}
static void ui_lost_ack_and_long_cleanup(void)
{
    reset();channel_survey_status.generation=255;
    wifi_channel_survey_begin_ui();CHECK(wifi_channel_survey_claim_generation==1&&channel_survey_status.generation==1);
    drop_after_command=CERBERUS_SPI_COMMAND_WIFI_CHANNEL_SURVEY_STOP;
    CHECK(!wifi_channel_survey_stop_ui()&&wifi_channel_survey_claimed);
    CHECK(channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_IDLE&&!ble_other_radio_active());
    CHECK(wifi_channel_survey_stop_ui()&&!wifi_channel_survey_claimed&&stops==1);
    reset();run_worker_on_notify=true;filter_error=ESP_FAIL;
    wifi_channel_survey_begin_ui(); /* SDK worker fails before Start status is assembled. */
    CHECK(starts==1&&wifi_channel_survey_claimed&&wifi_channel_survey_error==ESP_FAIL);
    CHECK(channel_survey_status.state==CERBERUS_WIFI_CHANNEL_SURVEY_ERROR);
    CHECK(wifi_channel_survey_stop_ui()&&!wifi_channel_survey_claimed&&stops==1);
    reset();wifi_channel_survey_begin_ui();hold_worker=true;
    current_page=UI_PAGE_OTHER;
    int64_t begin=clock_us;
    for(unsigned retry=0;retry<12;++retry){
        clock_us+=700000;ui_tick((uint32_t)(clock_us/1000));
        CHECK(wifi_channel_survey_claimed&&wifi_channel_survey_cleanup_requested);
        CHECK(starts==1&&ble_other_radio_active());
    }
    CHECK(clock_us-begin>18000000&&stops==12);
    hold_worker=false;clock_us+=700000;ui_tick((uint32_t)(clock_us/1000));
    CHECK(!wifi_channel_survey_claimed&&!ble_other_radio_active()&&starts==1);
    current_page=UI_PAGE_WIFI;ui_tick((uint32_t)(clock_us/1000)+700);CHECK(starts==1);
}
static void old_peer_gate(void)
{
    const unsigned versions[]={0,CERBERUS_WIFI_CHANNEL_SURVEY_MIN_FIRMWARE,
        CERBERUS_WIFI_CHANNEL_SURVEY_CLEANUP_MIN_FIRMWARE-1U};
    for(unsigned i=0;i<sizeof(versions)/sizeof(versions[0]);++i){
        reset();c5_status.firmware_version=versions[i];wifi_channel_survey_begin_ui();
        CHECK(wifi_channel_survey_error==ESP_ERR_NOT_SUPPORTED&&!wifi_channel_survey_claimed);
        CHECK(!starts&&!stops&&!status_queries);draw_expect("C5 FIRMWARE 69 REQUIRED","AP LIST");
        ui_tick((uint32_t)(clock_us/1000));ui_back();CHECK(!starts&&!stops&&!status_queries);
    }
}
static void previews(const char *prefix)
{
    static const char *names[]={"running","held-error","released"};
    reset();wifi_channel_survey_begin_ui();
    wifi_channel_survey_status.total_sweeps=7;wifi_channel_survey_status.total_packets=314;
    for(unsigned i=0;i<4;++i)wifi_channel_survey_rows[i]=(cerberus_wifi_channel_survey_result_payload_t){
        .channel=(uint8_t)(1+i*3),.seen=1,.rate_per_sec=(uint16_t)(12+i*4),.average_rssi=-65,
        .total_packets=70+i,.last_packets=(uint16_t)(4+i),.strongest_rssi=-42};
    for(unsigned state=0;state<3;++state){
        if(state==1){wifi_channel_survey_cleanup_requested=true;wifi_channel_survey_error=ESP_FAIL;}
        if(state==2){wifi_channel_survey_claimed=false;wifi_channel_survey_status.state=CERBERUS_WIFI_CHANNEL_SURVEY_IDLE;wifi_channel_survey_error=ESP_OK;}
        drawn[0]=0;CHECK(cerberus_display_fill_rect(0,0,320,480,UI_BACKGROUND)==ESP_OK);
        CHECK(draw_header("CHANNEL SURVEY")==ESP_OK);CHECK(draw_wifi_channel_survey_live()==ESP_OK);
        CHECK(draw_small_button(8,430,304,42,"BACK")==ESP_OK);
        char path[1024];snprintf(path,sizeof(path),"%s-%s.ppm",prefix,names[state]);
        FILE *file=fopen(path,"wb");CHECK(file);CHECK(fprintf(file,"P6\n320 480\n255\n")>0);
        for(unsigned y=0;y<480;++y)for(unsigned x=0;x<320;++x){uint16_t p=pixels[y][x];
            const uint8_t rgb[]={(uint8_t)(((p>>11)&31)*255/31),(uint8_t)(((p>>5)&63)*255/63),(uint8_t)((p&31)*255/31)};
            if(fwrite(rgb,1,3,file)!=3)abort();
        }
        CHECK(fclose(file)==0);
    }
}
int main(int argc,char **argv)
{
    CHECK(argc==2);
    native_lifecycle();ui_failed_cleanup_and_hidden();ui_uncertain_and_replacement();ui_lost_ack_and_long_cleanup();old_peer_gate();
    previews(argv[1]);
    printf("Channel Survey lifecycle: %u checks; actual worker/admission/SPI transport/UI cleanup, queued/dwell/disable failure/retry, uncertain Start, observed replacement, Back/sleep/hidden and reentry passed.\n",checks);
    return 0;
}
