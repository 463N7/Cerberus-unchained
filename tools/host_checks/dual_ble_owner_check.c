#include "cerberus_protocol.h"
#include "esp_err.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MYNEWT_VAL(x) 1
#define portMAX_DELAY 0xffffffffU
#define BLE_HCI_ADV_RPT_EVTYPE_ADV_IND 0
#define BLE_HCI_ADV_RPT_EVTYPE_DIR_IND 1
#define BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP 4
typedef struct {uint8_t val[6],type;} address_t;
struct ble_gap_disc_desc {address_t addr;int8_t rssi;uint8_t event_type;
    const uint8_t *data;uint8_t length_data;};
struct ble_hs_adv_fields {int unused;};
typedef struct {uint8_t address[6],address_type;int8_t rssi;uint16_t manufacturer_id;
    uint8_t flags;uint16_t reports_seen;uint32_t original_observed_ms;
    int8_t latest_rssi;bool original_time_valid;} cerberus_ble_record_t;
typedef struct {int unused;} sentinel_watch_protocol_record_t;
static struct {struct {uint16_t identities;} status;} activity_capture;
static unsigned watch_capture;
static cerberus_ble_record_t scan_results[32];
static cerberus_ble_scan_status_payload_t scan_status;
static uint32_t scan_callback_epoch;
static uint64_t dual_boot_id,dual_epoch_counter,dual_epoch;
static uint32_t dual_job_id,dual_job_highwater,dual_last_contact_ms,dual_track_ms,dual_track_reports;
static int32_t dual_error;
static uint8_t dual_mode,dual_generation,dual_address[6],dual_address_type;
static int8_t dual_track_rssi;
static bool dual_attached,dual_radio_active,dual_track_seen,host_synced;
static void *scanner_mutex=(void*)1;
static unsigned held, starts, stops;
static uint64_t now_ms;
static bool other_radio_busy, cancel_fails;
static int64_t esp_timer_get_time(void) {return (int64_t)(now_ms*1000U);}
static void xSemaphoreTake(void *m,unsigned wait) {(void)m;(void)wait;assert(!held);held=1;}
static void xSemaphoreGive(void *m) {(void)m;assert(held);held=0;}
static esp_err_t start_scan(bool companion)
{
    assert(companion && dual_attached && !held);
    if(other_radio_busy)return ESP_ERR_INVALID_STATE;
    ++starts;++scan_status.generation;dual_generation=scan_status.generation;
    ++scan_callback_epoch;scan_status.state=CERBERUS_BLE_SCAN_RUNNING;
    scan_status.stored_count=0;scan_status.error_code=0;dual_radio_active=true;
    return ESP_OK;
}
static esp_err_t stop_scan(bool companion)
{
    assert(companion && dual_attached && !held);++stops;
    if(cancel_fails){dual_error=ESP_FAIL;return ESP_FAIL;}
    dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_IDLE;return ESP_OK;
}
static int ble_hs_adv_parse_fields(struct ble_hs_adv_fields *f,const void *d,unsigned n)
{(void)f;(void)d;(void)n;return 0;}
static void cerberus_ble_activity_observe(void *a,const uint8_t *b,uint8_t t,const void *d,unsigned n,bool valid,uint32_t now)
{(void)a;(void)b;(void)t;(void)d;(void)n;(void)valid;(void)now;}
static bool sentinel_watch_protocol_decode_ble(const void *d,unsigned n,void *p)
{(void)d;(void)n;(void)p;return false;}
static void cerberus_watch_capture_observe_timed(void *a,const void *b,const uint8_t *c,uint8_t t,int8_t r,uint8_t x,uint64_t now)
{(void)a;(void)b;(void)c;(void)t;(void)r;(void)x;(void)now;}
static void cerberus_watch_capture_observe(void *a,const void *b,const uint8_t *c,uint8_t t,int8_t r,uint8_t x)
{(void)a;(void)b;(void)c;(void)t;(void)r;(void)x;}
static int find_record(const address_t *a)
{for(unsigned i=0;i<scan_status.stored_count;++i)if(scan_results[i].address_type==a->type && !memcmp(scan_results[i].address,a->val,6))return (int)i;return -1;}
static void copy_advertisement_fields(cerberus_ble_record_t *r,const struct ble_hs_adv_fields *f)
{(void)r;(void)f;}
static void copy_raw_advertisement(cerberus_ble_record_t *r,const struct ble_gap_disc_desc *d,bool parsed)
{(void)d;(void)parsed;if(r->reports_seen<UINT16_MAX)++r->reports_seen;}
#include "c5_ble_callback_impl.h"
#include "dual_ble_service_impl.h"

static void command(uint8_t cmd,const void *request,size_t n,void *reply,size_t expected)
{size_t size=0;assert(cerberus_ble_dual_command(cmd,request,n,reply,&size));assert(size==expected);assert(!held);}
int main(void)
{
    dual_boot_id=123;host_synced=true;now_ms=100;
    cerberus_dual_ble_info_t info;command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);
    assert(info.boot_id==123 && info.next_job_id==1 && (info.flags&CERBERUS_DUAL_BLE_READY));
    cerberus_dual_ble_start_t start={.boot_id=123,.job_id=1,.mode=1,.address={1,2,3,4,5,6},.address_type=1,.version=1};
    cerberus_dual_ble_status_t status;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,22,&status,22);
    assert(!status.error_code && status.epoch==1 && dual_attached && dual_radio_active && starts==1);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,22,&status,22);assert(starts==1);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);assert(info.flags&CERBERUS_DUAL_BLE_ATTACHED);
    cerberus_dual_ble_start_t other=start;other.address_type=0;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&other,22,&status,22);assert(status.error_code && starts==1);
    cerberus_dual_ble_binding_t b={123,1},foreign={123,2};
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&foreign,12,&status,22);assert(status.error_code==ESP_ERR_NOT_FOUND && !stops);
    struct ble_gap_disc_desc packet={.addr={{1,2,3,4,5,6},1},.rssi=-35,.event_type=0};
    record_advertisement(&packet,scan_callback_epoch,100000);
    packet.rssi=-90;record_advertisement(&packet,scan_callback_epoch,150000);
    assert(dual_track_seen && dual_track_rssi==-90 && dual_track_reports==2 && scan_results[0].rssi==-35);
    assert(scan_results[0].latest_rssi==-90 && scan_results[0].original_observed_ms==150);
    packet.addr.type=0;record_advertisement(&packet,scan_callback_epoch,170000);
    assert(dual_track_reports==2 && scan_status.stored_count==2);
    packet.addr.type=1;packet.rssi=127;record_advertisement(&packet,scan_callback_epoch,180000);
    assert(dual_track_reports==2 && dual_track_ms==150);
    packet.rssi=-60;packet.event_type=4;record_advertisement(&packet,scan_callback_epoch,190000);
    assert(dual_track_reports==2); /* Scan responses never masquerade as original ADV. */
    packet.event_type=0;record_advertisement(&packet,scan_callback_epoch-1,190000);assert(dual_track_reports==2);
    scan_status.stored_count=32;memset(scan_results,0,sizeof(scan_results));
    record_advertisement(&packet,scan_callback_epoch,200000);assert(dual_track_reports==3 && dual_track_rssi==-60);
    now_ms=250;cerberus_dual_ble_track_t track;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_TRACK_STATUS,&b,12,&track,22);
    assert(track.age_ms==50 && track.reports==3 && track.rssi==-60 && track.flags==3);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&b,12,&status,22);assert(status.error_code && dual_attached);
    cancel_fails=true;command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&b,12,&status,22);
    assert(status.error_code && (status.flags&CERBERUS_DUAL_BLE_OWNED) && dual_attached);
    cancel_fails=false;command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&b,12,&status,22);
    assert(!(status.flags&CERBERUS_DUAL_BLE_OWNED) && dual_attached);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&b,12,&status,22);assert(!dual_attached && !status.flags);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,22,&status,22);assert(status.error_code && starts==1 && !dual_attached);
    other=start;other.boot_id=456;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&other,22,&status,22);assert(status.error_code==ESP_ERR_NOT_FOUND && starts==1);
    other=start;other.job_id=2;other.mode=0;memset(other.address,0,6);other.address_type=0;
    other_radio_busy=true;command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&other,22,&status,22);
    assert(status.error_code && !dual_attached && starts==1);other_radio_busy=false;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);assert(info.next_job_id==3);
    other.job_id=info.next_job_id;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&other,22,&status,22);assert(starts==2 && dual_attached);
    now_ms+=13000;command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);assert(dual_attached); /* Running RF is never assumed gone. */
    dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_COMPLETE;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);assert(!dual_attached); /* Completed orphan expires. */
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&other,22,&status,22);assert(status.error_code && !dual_attached && starts==2);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,22,&status,22);assert(status.error_code && !dual_attached && starts==2);
    puts("dual BLE C5 owner/callback: exact typed tracker, latest RSSI, capacity, idempotency, foreign ownership, orphan checks passed");
    return 0;
}
