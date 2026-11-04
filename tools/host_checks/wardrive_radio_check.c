#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
enum {ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=257,ESP_ERR_INVALID_ARG,ESP_ERR_INVALID_STATE,
    ESP_ERR_TIMEOUT,ESP_ERR_NOT_SUPPORTED,ESP_ERR_NOT_FINISHED};
static unsigned checks,depth,allocations;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %u: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) do{(void)x;CHECK(!depth);++depth;}while(0)
#define portEXIT_CRITICAL(x) do{(void)x;CHECK(depth==1);--depth;}while(0)
#ifndef FIXTURE_TICK_MS
#define FIXTURE_TICK_MS 1U
#endif
#define pdMS_TO_TICKS(x) ((x)/FIXTURE_TICK_MS)
#define pdPASS 1
#ifndef FIXTURE_EXT_ADV
#define FIXTURE_EXT_ADV 0
#endif
#define MYNEWT_VAL(x) FIXTURE_EXT_ADV
enum {MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_SPIRAM=4,
    WIFI_SECOND_CHAN_NONE=0,WIFI_STORAGE_RAM=1,WIFI_MODE_NULL=0,WIFI_PROMIS_FILTER_MASK_MGMT=1,
    BLE_HS_FOREVER=0,BLE_HS_EALREADY=99,BLE_GAP_EVENT_DISC=1,BLE_GAP_EVENT_EXT_DISC=2};
typedef enum {WIFI_PKT_MGMT=0,WIFI_PKT_DATA=1} wifi_promiscuous_pkt_type_t;
typedef struct {unsigned sig_len,rx_state,channel;int rssi;} rx_control_t;
typedef struct {rx_control_t rx_ctrl;uint8_t payload[256];} wifi_promiscuous_pkt_t;
typedef struct {unsigned filter_mask;} wifi_promiscuous_filter_t;
typedef struct {int static_rx_buf_num,dynamic_rx_buf_num,dynamic_tx_buf_num,rx_ba_win,
    ampdu_rx_enable,ampdu_tx_enable,amsdu_tx_enable;} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
struct ble_gap_disc_params{uint16_t itvl,window;uint8_t passive,filter_duplicates;};
struct ble_gap_ext_disc_params{uint16_t itvl,window;uint8_t passive;};
struct ble_gap_event{int type;struct {uint8_t *data;size_t length_data;
    struct {uint8_t val[6],type;}addr;int rssi;}disc;
    struct {uint8_t *data;size_t length_data;struct {uint8_t val[6],type;}addr;int rssi,data_status;}ext_disc;};
static uint64_t now_us=1000000;
static int64_t esp_timer_get_time(void){return (int64_t)now_us;}
static bool fail_alloc,fail_task,driver_rx,ble_rx,drop_contact,drain,prepare_fail;
static unsigned cleanup_failures,channel,radio_stops;
static uint32_t stop_at;
static void (*worker_fn)(void *);
static int (*gap_fn)(struct ble_gap_event *,void *);
static void *gap_arg;
static void *heap_caps_calloc(size_t n,size_t size,unsigned caps)
{CHECK(!depth&&(caps==3||caps==6));if(fail_alloc)return NULL;void *p=calloc(n,size);CHECK(p);++allocations;return p;}
static void heap_caps_free(void *p){CHECK(!depth);if(p){CHECK(allocations);--allocations;free(p);}}
static int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle)
{CHECK(!depth&&!strcmp(name,"wd_radio")&&stack==4096&&!arg&&priority==4&&!handle);if(fail_task)return 0;worker_fn=fn;return pdPASS;}
static void vTaskDelete(void *unused){CHECK(!depth&&!unused);++radio_stops;}
static void vTaskDelay(unsigned ticks);
#if CONFIG_IDF_TARGET_ESP32C5
typedef int wifi_band_mode_t;
typedef int wifi_second_chan_t;
enum {WIFI_BAND_MODE_AUTO=0,WIFI_BAND_MODE_2G_ONLY=1,WIFI_BAND_2G=2,WIFI_BAND_5G=5};
static wifi_band_mode_t current_mode=WIFI_BAND_MODE_2G_ONLY;
static unsigned current_band=WIFI_BAND_2G,current_filter=9;
static esp_err_t esp_wifi_get_band_mode(wifi_band_mode_t *mode){CHECK(!depth);*mode=current_mode;return ESP_OK;}
static esp_err_t esp_wifi_set_band_mode(wifi_band_mode_t mode){CHECK(!depth&&!ble_rx);current_mode=mode;return ESP_OK;}
static esp_err_t esp_wifi_get_channel(uint8_t *ch,wifi_second_chan_t *second){CHECK(!depth);*ch=6;*second=WIFI_SECOND_CHAN_NONE;return ESP_OK;}
static esp_err_t esp_wifi_set_band(unsigned band){CHECK(!depth&&!ble_rx&&current_mode==WIFI_BAND_MODE_AUTO);current_band=band;return ESP_OK;}
static esp_err_t esp_wifi_get_promiscuous_filter(wifi_promiscuous_filter_t *filter){CHECK(!depth);filter->filter_mask=current_filter;return ESP_OK;}
#endif
static esp_err_t esp_wifi_set_channel(unsigned value,int secondary)
{CHECK(!depth&&secondary==WIFI_SECOND_CHAN_NONE&&!ble_rx);
#if CONFIG_IDF_TARGET_ESP32C5
 CHECK(current_band==(value<=14?WIFI_BAND_2G:WIFI_BAND_5G));
#endif
 channel=value;return ESP_OK;}
static esp_err_t esp_wifi_set_promiscuous(bool enabled)
{CHECK(!depth);if(!enabled&&cleanup_failures){--cleanup_failures;return ESP_FAIL;}CHECK(!enabled||!ble_rx);driver_rx=enabled;return ESP_OK;}
static esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t *f)
{CHECK(!depth&&(f->filter_mask==WIFI_PROMIS_FILTER_MASK_MGMT||f->filter_mask==9));
 if(prepare_fail){prepare_fail=false;return ESP_FAIL;}
#if CONFIG_IDF_TARGET_ESP32C5
 current_filter=f->filter_mask;
#endif
 return ESP_OK;}
static int ble_hs_id_infer_auto(int privacy,uint8_t *type){CHECK(!depth&&!privacy);*type=0;return 0;}
#if CONFIG_IDF_TARGET_ESP32C5 && FIXTURE_EXT_ADV
static int ble_gap_ext_disc(unsigned type,int duration,int period,int duplicates,int filter,int limited,
    const struct ble_gap_ext_disc_params *one_m,const struct ble_gap_ext_disc_params *coded,
    int (*fn)(struct ble_gap_event *,void *),void *arg)
{CHECK(!depth&&!driver_rx&&!ble_rx&&!type&&!duration&&!period&&!duplicates&&!filter&&!limited);
 CHECK(one_m&&coded&&one_m->passive&&coded->passive&&one_m->itvl==160&&coded->itvl==160&&one_m->window==80&&coded->window==80);
 gap_fn=fn;gap_arg=arg;ble_rx=true;return 0;}
#else
static int ble_gap_disc(unsigned type,int duration,const struct ble_gap_disc_params *p,
    int (*fn)(struct ble_gap_event *,void *),void *arg)
{CHECK(!depth&&!driver_rx&&!ble_rx&&!type&&duration==BLE_HS_FOREVER&&p->passive&&!p->filter_duplicates&&p->itvl==p->window);gap_fn=fn;gap_arg=arg;ble_rx=true;return 0;}
#endif
static int ble_gap_disc_cancel(void)
{CHECK(!depth);if(cleanup_failures){--cleanup_failures;return 1;}ble_rx=false;return 0;}
static bool ble_gap_disc_active(void){CHECK(!depth);return ble_rx;}
#if CONFIG_IDF_TARGET_ESP32S3
#include "s3_wifi_owner.h"
#include "s3_ble_host.h"
static bool wifi_owned,ble_owned,wifi_initialized,wifi_started;
cerberus_s3_ble_owner_t cerberus_s3_ble_host_owner(void){return ble_owned?CERBERUS_S3_BLE_OWNER_WARDRIVE:CERBERUS_S3_BLE_OWNER_NONE;}
esp_err_t cerberus_s3_wifi_claim(cerberus_s3_wifi_owner_t owner)
{CHECK(!depth&&owner==CERBERUS_S3_WIFI_WARDRIVE&&!wifi_owned);wifi_owned=true;return ESP_OK;}
esp_err_t cerberus_s3_wifi_release(cerberus_s3_wifi_owner_t owner)
{CHECK(!depth&&owner==CERBERUS_S3_WIFI_WARDRIVE&&wifi_owned&&!wifi_initialized&&!ble_owned);wifi_owned=false;return ESP_OK;}
esp_err_t cerberus_s3_wifi_prepare(cerberus_s3_wifi_owner_t owner)
{CHECK(!depth&&wifi_owned&&owner==CERBERUS_S3_WIFI_WARDRIVE);return ESP_OK;}
esp_err_t cerberus_s3_ble_host_claim(cerberus_s3_ble_owner_t owner,cerberus_s3_ble_host_lease_t *lease)
{CHECK(!depth&&wifi_owned&&!ble_owned&&owner==CERBERUS_S3_BLE_OWNER_WARDRIVE);ble_owned=true;lease->token=1;return ESP_OK;}
bool cerberus_s3_ble_host_lease_valid(const cerberus_s3_ble_host_lease_t *lease){return lease->token&&ble_owned;}
esp_err_t cerberus_s3_ble_host_release(const cerberus_s3_ble_host_lease_t *lease)
{CHECK(!depth&&lease->token&&ble_owned&&!ble_rx);ble_owned=false;return ESP_OK;}
static esp_err_t esp_wifi_init(const wifi_init_config_t *init)
{CHECK(!depth&&!wifi_initialized&&init->static_rx_buf_num==4&&init->dynamic_tx_buf_num==2);wifi_initialized=true;return ESP_OK;}
static esp_err_t esp_wifi_set_storage(int mode){CHECK(!depth&&mode==WIFI_STORAGE_RAM);return ESP_OK;}
static esp_err_t esp_wifi_set_mode(int mode){CHECK(!depth&&mode==WIFI_MODE_NULL);return ESP_OK;}
static esp_err_t esp_wifi_start(void){CHECK(!depth&&wifi_initialized);wifi_started=true;return ESP_OK;}
static esp_err_t esp_wifi_set_promiscuous_rx_cb(void (*fn)(void *,wifi_promiscuous_pkt_type_t))
{CHECK(!depth);(void)fn;return ESP_OK;}
static esp_err_t esp_wifi_stop(void){CHECK(!depth&&!driver_rx);wifi_started=false;return ESP_OK;}
static esp_err_t esp_wifi_deinit(void){CHECK(!depth&&!wifi_started);wifi_initialized=false;return ESP_OK;}
#else
static bool cerberus_wifi_wardrive_ready(void){return true;}
static bool cerberus_ble_wardrive_ready(void){return true;}
#endif
#include "../../shared/wardrive_radio.c"
static cerberus_wd_request_t query;
static void vTaskDelay(unsigned ticks)
{
    CHECK(!depth&&ticks);now_us+=(uint64_t)ticks*FIXTURE_TICK_MS*1000U;CHECK(now_us<UINT64_C(1000000000));
    CHECK(cerberus_wardrive_radio_held()); /* Cleanup failure must retain ownership. */
    cerberus_wd_status_t status;
    if(!drop_contact)CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_STATUS,&query,&status)==ESP_OK);
    if(driver_rx){
        wifi_promiscuous_pkt_t frame={.rx_ctrl={.sig_len=43,.channel=channel,.rssi=-45}};
        frame.payload[0]=0x80;memset(frame.payload+4,255,6);frame.payload[10]=2;frame.payload[16]=4;
        frame.payload[36]=0;frame.payload[37]=1;frame.payload[38]='X';
        cerberus_wardrive_radio_wifi_rx(&frame,WIFI_PKT_MGMT);
    }
    if(ble_rx){
        uint8_t ad[]={2,9,'B'};struct ble_gap_event event={.type=BLE_GAP_EVENT_DISC,
            .disc={.data=ad,.length_data=3,.addr={.val={1,2,3,4,5,6}},.rssi=-50}};
        CHECK(gap_fn(&event,gap_arg)==0);
#if FIXTURE_EXT_ADV
        event.type=BLE_GAP_EVENT_EXT_DISC;event.ext_disc.data=ad;event.ext_disc.length_data=3;
        event.ext_disc.addr.val[0]=2;event.ext_disc.rssi=-51;event.ext_disc.data_status=1;
        uint32_t reports=wd_status.ble_reports;CHECK(gap_fn(&event,gap_arg)==0&&wd_status.ble_reports==reports);
        event.ext_disc.data_status=0;CHECK(gap_fn(&event,gap_arg)==0);
#endif
    }
    if(drain&&!drop_contact){
        cerberus_wd_record_t row;CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_READ,&query,&row)==ESP_OK);
        if(row.sequence){CHECK(row.sequence>query.ack);query.ack=row.sequence;}
    }
    if(!drop_contact&&wd_now()>=stop_at)CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_STOP,&query,&status)==ESP_OK);
}
static void scenario(uint8_t mode,bool overflow,bool lost_host,bool fail_prepare,bool fail_create)
{
    static uint64_t session=100;query=(cerberus_wd_request_t){.session=++session,.mode=mode};
    drain=!overflow;drop_contact=lost_host;prepare_fail=fail_prepare;fail_task=fail_create;
    bool fail_cleanup=cleanup_failures!=0;
    stop_at=wd_now()+7000;cerberus_wd_status_t status;
    esp_err_t e=cerberus_wardrive_radio_exchange(CERBERUS_WD_START,&query,&status);
    CHECK(e==(fail_create?ESP_ERR_NO_MEM:ESP_OK));
    if(!fail_create){CHECK(worker_fn);worker_fn(NULL);worker_fn=NULL;}
    CHECK(!cerberus_wardrive_radio_held()&&!driver_rx&&!ble_rx);
    CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_STATUS,&query,&status)==ESP_OK);
    CHECK(status.state==CERBERUS_WD_STOPPED&&!status.held);
    if(lost_host)CHECK(status.error==ESP_ERR_TIMEOUT);
    else if(fail_prepare)CHECK(status.error==ESP_FAIL);
    else if(fail_create)CHECK(status.error==ESP_ERR_NO_MEM);
    else if(fail_cleanup)CHECK(status.error==ESP_FAIL&&cleanup_failures==0);
    else {
        CHECK(status.error==ESP_OK);
        if(mode==CERBERUS_WD_BLE)CHECK(!status.wifi_reports&&status.ble_reports>1);
        else CHECK(status.wifi_reports>1&&status.wifi_sweeps>0);
#if CONFIG_IDF_TARGET_ESP32S3
        if(mode==CERBERUS_WD_COMBINED)CHECK(status.ble_reports>1);
#endif
        if(overflow)CHECK(status.dropped>0&&status.pending==WD_QUEUE_CAPACITY);
    }
    cerberus_wd_request_t stale=query;stale.session+=1000;
    CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_RELEASE,&stale,&status)==ESP_ERR_INVALID_STATE);
    CHECK(allocations==1);
    if(overflow){CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_RELEASE,&query,&status)==ESP_ERR_INVALID_STATE);}
    query.discard=1;CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_RELEASE,&query,&status)==ESP_OK);
    CHECK(!allocations);CHECK(cerberus_wardrive_radio_exchange(CERBERUS_WD_START,&query,&status)==ESP_ERR_INVALID_STATE);
#if CONFIG_IDF_TARGET_ESP32S3
    CHECK(!wifi_owned&&!ble_owned&&!wifi_initialized&&!wifi_started);
#else
    CHECK(!wd_wifi_saved&&current_mode==WIFI_BAND_MODE_2G_ONLY&&current_band==WIFI_BAND_2G&&current_filter==9);
#endif
}
int main(void)
{
    scenario(CERBERUS_WD_WIFI,false,false,false,false);
    scenario(CERBERUS_WD_BLE,false,false,false,false);
    scenario(CERBERUS_WD_COMBINED,false,false,false,false);
    scenario(CERBERUS_WD_WIFI,true,false,false,false);
    scenario(CERBERUS_WD_BLE,true,true,false,false);
    scenario(CERBERUS_WD_WIFI,false,false,true,false);
    scenario(CERBERUS_WD_WIFI,false,false,false,true);
    cleanup_failures=2;scenario(CERBERUS_WD_WIFI,false,false,false,false);
    cleanup_failures=2;scenario(CERBERUS_WD_BLE,false,false,false,false);
    printf("Wardrive native receiver: %u checks; modes, repeats, lease loss, overflow, failed setup and exact cleanup passed\n",checks);
    return 0;
}
