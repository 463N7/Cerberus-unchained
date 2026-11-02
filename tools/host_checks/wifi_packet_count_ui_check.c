#define main controller_suite
#include "wifi_packet_count_scan_check.c"
#undef main
#include "count_framebuffer.h"
#include "ui_widgets.h"
#include "wifi_packet_count_record.h"
#include "count_modes.h"
#define UI_BACKGROUND 0x0042
#define UI_PANEL 0x08A4
#define UI_MUTED 0x7CB5
#define UI_TEXT 0xE77F
#define UI_BAD 0xF471
#define MALLOC_CAP_SPIRAM 4U
#define MALLOC_CAP_8BIT 2U
#define TAG "count-ui"
#define ESP_ERROR_CHECK(x) CHECK((x)==ESP_OK)
enum {UI_PAGE_WIFI,UI_PAGE_OTHER,SENTINEL_SCAN_NONE};
static unsigned current_page=UI_PAGE_WIFI;
static ui_wifi_mode_t wifi_mode=UI_WIFI_MODE_DUAL;
static unsigned live_session,live_ble_session;
static bool oom,masked,prior_released=true,ordinary_busy;
static unsigned allocations,frees,ordinary_stops,record_starts,record_finishes,record_releases;
static struct {unsigned firmware_version;} c5_status={74};
typedef struct {bool cleanup;} cerberus_dual_wifi_status_t;
static void cerberus_dual_wifi_scan_get_status(cerberus_dual_wifi_status_t *s){s->cleanup=ordinary_busy;}
static bool dual_wifi_ui_busy(const cerberus_dual_wifi_status_t *s){return s->cleanup;}
static bool dual_wifi_live_released(void){return prior_released;}
static void dual_wifi_live_cancel_handoff(void){}
static void dual_wifi_ui_leave(void){++ordinary_stops;}
static void wifi_relation_cancel(void){}
static void sentinel_scan_session_select(unsigned *session,unsigned kind){*session=kind;}
static void dual_wifi_live_open(bool start){CHECK(!start);wifi_mode=UI_WIFI_MODE_DUAL;}
static int64_t esp_timer_get_time(void){return fake.clock;}
static void *heap_caps_calloc(size_t n,size_t size,unsigned caps)
{CHECK(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(oom)return NULL;++allocations;return calloc(n,size);}
static void heap_caps_free(void *p){if(p){++frees;free(p);}}
static bool privacy_addresses_masked(void){return masked;}
static const char *esp_err_to_name(esp_err_t e){return e==ESP_ERR_NO_MEM?"ESP_ERR_NO_MEM":e==ESP_ERR_INVALID_STATE?"ESP_ERR_INVALID_STATE":"ESP_FAIL";}
static void cerberus_privacy_format_wifi_identifier(const uint8_t *a,bool hide,char *out,size_t n)
{if(hide)snprintf(out,n,"**:**:**:**:**:**");else snprintf(out,n,"%02X:%02X:%02X:%02X:%02X:%02X",a[0],a[1],a[2],a[3],a[4],a[5]);}
static const cerberus_ui_widget_theme_t theme={UI_BACKGROUND,UI_PANEL,UI_TEXT,UI_TEXT,UI_MUTED};
#define UI_WIDGET_THEME (&theme)
static esp_err_t draw_centered(uint16_t y,const char *text,uint16_t color,uint8_t scale)
{CHECK(strlen(text)*6U*scale<=320U);return cerberus_display_draw_text((320U-strlen(text)*6U*scale)/2U,y,text,color,UI_BACKGROUND,scale);}
static esp_err_t draw_header(const char *text){return draw_centered(20,text,UI_TEXT,2);}
static esp_err_t draw_detail_page(void);
esp_err_t cerberus_s3_wifi_packet_count_exchange(uint8_t c,const void*q,size_t qn,void*r,size_t rn)
{return exchange(&fake,0,c,q,qn,r,rn);}
static esp_err_t c5_spi_link_wifi_packet_count_exchange(uint8_t c,const void*q,size_t qn,void*r,size_t rn)
{return exchange(&fake,1,c,q,qn,r,rn);}
static wifi_packet_count_record_status_t record;
static bool recorder_ready,recorder_fail,recorder_missing;
esp_err_t wifi_packet_count_record_start(const wifi_packet_count_transport_t *t,uint64_t *job)
{CHECK(t&&t->context==NULL);++record_starts;record=(wifi_packet_count_record_status_t){.job=91,.busy=true};*job=91;return ESP_OK;}
bool wifi_packet_count_record_status(uint64_t job,wifi_packet_count_record_status_t *out)
{CHECK(job==91);if(recorder_missing){memset(out,0,sizeof(*out));return false;}record.ready=recorder_ready;record.error=recorder_fail?ESP_FAIL:ESP_OK;*out=record;return true;}
esp_err_t wifi_packet_count_record_bind(uint64_t job,unsigned source,uint64_t session)
{CHECK(job==91);record.source[source].bound=true;record.source[source].session=session;return record_bind(&fake,source,session);}
bool wifi_packet_count_record_drained(uint64_t job,unsigned source,uint64_t session)
{CHECK(job==91);bool done=record_drained(&fake,source,session);if(done){record.source[source].bound=false;record.source[source].drained=true;}return done;}
esp_err_t wifi_packet_count_record_finish(uint64_t job)
{CHECK(job==91);++record_finishes;CHECK(!record.source[0].bound&&!record.source[1].bound);record.busy=false;return ESP_OK;}
esp_err_t wifi_packet_count_record_release(uint64_t job)
{CHECK(job==91&&!record.busy);++record_releases;return ESP_OK;}
#include "wifi_packet_count_ui.h"
static esp_err_t draw_detail_page(void)
{drawn_size=0;drawn[0]=0;return wifi_mode==UI_WIFI_MODE_PACKET_COUNT?wifi_packet_count_draw():ESP_OK;}

/* Extracted route bodies consume actual copied AP/station values. */
typedef struct {struct {uint8_t bssid[6],channel;char ssid[33];} ap;uint8_t source;uint32_t source_generation;} cerberus_dual_wifi_row_t;
static cerberus_dual_wifi_row_t route_row;
static esp_err_t route_error;
static void dual_wifi_ui_set_error(esp_err_t e){route_error=e;}
static esp_err_t dual_wifi_ui_copy_selected(cerberus_dual_wifi_row_t *out){*out=route_row;return ESP_OK;}
typedef struct {uint8_t station[6],bssid[6],channel;} cerberus_wifi_relation_row_t;
typedef struct {struct {struct {cerberus_wifi_relation_row_t rows[1];} bank;bool valid;uint8_t count;} selected;
    uint64_t selected_publication;uint8_t detail_index;bool detail;char message[49];} wifi_relation_ui_store_t;
static wifi_relation_ui_store_t relation_store;
#include "count_routes.h"
static void ui_ticks(unsigned n,bool visible)
{while(n--){fake.clock+=20000;fake.calls=0;wifi_packet_count_tick((uint64_t)fake.clock/1000U,visible,false);CHECK(fake.calls<=4U);}}
static void ui_reset(void)
{
    if(wifi_packet_count_ui){free(wifi_packet_count_ui);wifi_packet_count_ui=NULL;}
    reset();memset(&record,0,sizeof(record));oom=masked=ordinary_busy=recorder_ready=recorder_fail=recorder_missing=false;
    prior_released=true;wifi_mode=UI_WIFI_MODE_DUAL;current_page=UI_PAGE_WIFI;
    route_row=(cerberus_dual_wifi_row_t){.ap={.bssid={2,0,0,0,0,1},.channel=1,.ssid="TEST AP"},.source=1,.source_generation=44};
    relation_store=(wifi_relation_ui_store_t){.detail=true,.selected={.valid=true,.count=1,
        .bank={.rows={{.station={2,0,0,0,0,2},.bssid={2,0,0,0,0,1},.channel=6}}}},.selected_publication=93};
}
static void ppm(const char *folder,const char *name)
{
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",folder,name);FILE*f=fopen(path,"wb");CHECK(f);
    fprintf(f,"P6\n320 480\n255\n");for(unsigned i=0;i<320U*480U;++i){uint16_t p=screen[i];
        uint8_t v[]={(uint8_t)((p>>11)*255/31),(uint8_t)(((p>>5)&63)*255/63),(uint8_t)((p&31)*255/31)};CHECK(fwrite(v,1,3,f)==3);}CHECK(!fclose(f));
}
static void route_and_lifecycle(const char *folder)
{
    ui_reset();oom=true;unsigned stopped=ordinary_stops;route_ap();CHECK(!wifi_packet_count_ui&&ordinary_stops==stopped&&wifi_mode==UI_WIFI_MODE_DUAL&&route_error==ESP_ERR_NO_MEM);
    oom=false;route_ap();CHECK(wifi_mode==UI_WIFI_MODE_PACKET_COUNT&&wifi_packet_count_ui->scan.count==1U&&!fake.calls);
    route_ap();CHECK(wifi_packet_count_ui->scan.count==1U);route_station(100,390);CHECK(wifi_packet_count_ui->scan.count==2U);
    CHECK(!memcmp(wifi_packet_count_ui->scan.selected[1].target.mac,relation_store.selected.bank.rows[0].station,6));
    CHECK(draw_detail_page()==ESP_OK&&strstr(drawn,"TOTAL --"));ppm(folder,"count-ready");
    prior_released=false;wifi_count_start_stop();ui_ticks(30,true);CHECK(!fake.source[0].begin_calls&&!fake.source[1].begin_calls&&!wifi_packet_count_ui_busy());
    wifi_count_start_stop();CHECK(!wifi_packet_count_ui->start_pending);prior_released=true;
    wifi_count_start_stop();ui_ticks(300,true);CHECK(fake.source[0].starts==1&&fake.source[1].starts==1);
    CHECK(draw_detail_page()==ESP_OK&&strstr(drawn,"RECEIVING"));ppm(folder,"count-running");
    wifi_packet_count_tap(100,210);CHECK(wifi_packet_count_ui->detail);
    uint64_t frozen=wifi_packet_count_ui->selected.total.observations;
    observe(&fake.source[0],fake.clock);ui_ticks(200,true);CHECK(wifi_packet_count_ui->selected.total.observations==frozen);
    CHECK(draw_detail_page()==ESP_OK);ppm(folder,"count-detail");
    masked=true;CHECK(draw_detail_page()==ESP_OK&&!strstr(drawn,"02:00:00:00:00:01"));ppm(folder,"count-masked");masked=false;
    wifi_packet_count_back();CHECK(!wifi_packet_count_ui->detail);
    fake.source[0].hold_stop=true;wifi_packet_count_back();ui_ticks(800,true);CHECK(wifi_packet_count_ui&&wifi_packet_count_ui->closing&&!fake.source[0].releases);
    fake.source[0].hold_stop=false;ui_ticks(500,true);CHECK(!wifi_packet_count_ui&&wifi_mode==UI_WIFI_MODE_OBSERVED_CLIENTS);
    ui_reset();route_ap();wifi_count_start_stop();ui_ticks(100,true);current_page=UI_PAGE_OTHER;ui_ticks(200,false);CHECK(!wifi_packet_count_ui&&!fake.source[1].session);
}
static void recording_ui(const char *folder)
{
    ui_reset();route_ap();wifi_packet_count_ui->record_enabled=true;unsigned starts=record_starts;
    wifi_count_start_stop();ui_ticks(20,true);CHECK(record_starts==starts+1U&&!fake.source[1].begin_calls&&wifi_packet_count_ui->start_pending);
    wifi_count_start_stop();ui_ticks(5,true);CHECK(!wifi_packet_count_ui->job&&!fake.source[1].begin_calls);
    recorder_ready=true;wifi_count_start_stop();ui_ticks(160,true);CHECK(fake.source[1].starts==1&&fake.binds==1);
    fake.source[1].drain_ready=false;wifi_count_start_stop();ui_ticks(200,true);CHECK(wifi_packet_count_ui->job&&!fake.source[1].releases);
    fake.source[1].drain_ready=true;ui_ticks(200,true);CHECK(!wifi_packet_count_ui->job&&wifi_packet_count_ui->scan.frozen);
    wifi_packet_count_ui->recording.source[0].written=UINT32_MAX;wifi_packet_count_ui->recording.source[1].written=UINT32_MAX;
    wifi_packet_count_ui->recording.source[0].discarded=UINT32_MAX;wifi_packet_count_ui->recording.source[1].discarded=UINT32_MAX;
    CHECK(draw_detail_page()==ESP_OK);ppm(folder,"count-recorded");
    wifi_packet_count_back();ui_ticks(5,true);CHECK(!wifi_packet_count_ui);
    ui_reset();route_ap();wifi_packet_count_ui->record_enabled=true;recorder_ready=true;
    wifi_count_start_stop();ui_ticks(150,true);CHECK(fake.source[1].starts==1U);
    unsigned finishes=record_finishes,releases=record_releases;
    recorder_missing=true;ui_ticks(200,true);
    CHECK(!wifi_packet_count_ui->job&&wifi_packet_count_ui->job_retired&&wifi_packet_count_ui->error==ESP_ERR_NOT_FOUND);
    CHECK(!fake.source[1].session&&record_finishes==finishes&&record_releases==releases);
    wifi_packet_count_back();ui_ticks(5,true);CHECK(!wifi_packet_count_ui);
    ui_reset();route_ap();wifi_packet_count_ui->record_enabled=true;recorder_fail=true;
    wifi_count_start_stop();ui_ticks(20,true);CHECK(!fake.source[1].begin_calls&&!wifi_packet_count_ui->job&&wifi_packet_count_ui->error==ESP_FAIL);
}
static void painting(void)
{
    ui_reset();route_ap();CHECK(draw_detail_page()==ESP_OK&&wifi_packet_count_ui->painted_count==1U);
    wifi_packet_count_tap(150,390);CHECK(wifi_packet_count_ui->picking&&wifi_mode==UI_WIFI_MODE_DUAL);
    wifi_packet_count_tick((uint64_t)fake.clock/1000U,false,true);CHECK(wifi_packet_count_ui);
    wifi_packet_count_tick((uint64_t)fake.clock/1000U,false,false);CHECK(!wifi_packet_count_ui);
    route_ap();CHECK(draw_detail_page()==ESP_OK);
    fail_after=calls+2U;CHECK(wifi_packet_count_draw()==ESP_FAIL&&!wifi_packet_count_ui->painted_count);fail_after=0;
    wifi_packet_count_tap(30,205);CHECK(!wifi_packet_count_ui->detail);
    for(unsigned i=2;i<=16U;++i){wifi_packet_count_selection_t v=scan.selected[0];v.target.mac[5]=(uint8_t)i;CHECK(wifi_packet_count_scan_add(&wifi_packet_count_ui->scan,&v)==ESP_OK);}
    wifi_packet_count_ui->page=3;CHECK(draw_detail_page()==ESP_OK&&wifi_packet_count_ui->painted_count==4U);
    wifi_packet_count_tap(30,325);CHECK(wifi_packet_count_ui->selected.index==15U);
    wifi_packet_count_tap(30,390);CHECK(wifi_packet_count_ui->scan.count==15U&&!wifi_packet_count_ui->detail);
    wifi_packet_count_tap(250,390);CHECK(!wifi_packet_count_ui->scan.count);
}
int main(int argc,char **argv)
{CHECK(argc==2);controller_suite();route_and_lifecycle(argv[1]);recording_ui(argv[1]);painting();
 printf("Packet Count actual UI: %u checks PASS; UI %zu bytes\n",checks,sizeof(wifi_packet_count_ui_store_t));return 0;}
