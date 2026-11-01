#include "dual_ble_diagnostics.h"
#include "dual_ble_scan.h"
#include "s3_ble_discovery_session.h"
#include "s3_ble_tracker.h"
#include "s3_ble_host.h"
#include "c5_spi_link.h"
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static unsigned held, enters, scanner_reads, host_reads;
void draft_enter_critical(unsigned *lock){(void)lock;assert(!held);held=1;++enters;}
void draft_exit_critical(unsigned *lock){(void)lock;assert(held);held=0;}
#include "dual_ble_diagnostics.c"
static cerberus_dual_ble_scan_t storage,*dual_ble_ui_store;
static int dual_ble_ui_mode=-1;
static uint8_t dual_ble_ui_family;
static bool ble_scan_checked,ble_fox_hunt_start_pending,ble_fox_hunt_exit_pending;
static esp_err_t ble_scan_error,ble_fox_hunt_error,ble_fox_hunt_barrier_error,sample_error;
static bool ble_fox_hunt_cleanup_timed_out;
static cerberus_ble_scan_status_payload_t ble_scan_status;
static cerberus_c5_ble_result_t ble_fox_hunt_target;
static cerberus_s3_ble_track_status_t source_sample;
static struct {cerberus_ble_discovery_session_t session;} discovery,*ble_discovery_live;
enum {UI_PAGE_BLE=1,UI_BLE_MODE_FOX_HUNT=2};
static unsigned current_page,ble_mode;
bool cerberus_s3_ble_host_initialized(void){++host_reads;return true;}
bool cerberus_s3_ble_host_synced(void){++host_reads;return true;}
cerberus_s3_ble_owner_t cerberus_s3_ble_host_owner(void){++host_reads;return CERBERUS_S3_BLE_OWNER_SCANNER;}
static esp_err_t cerberus_s3_ble_scanner_track_status(cerberus_s3_ble_track_status_t *out)
{++scanner_reads;*out=source_sample;return sample_error;}
static void dual_ble_ui_get_view(cerberus_dual_ble_view_t *out){cerberus_dual_ble_view(dual_ble_ui_store,out);}
static bool dual_ble_ui_released(void){return cerberus_dual_ble_released(dual_ble_ui_store);}
#include "dual_ble_diagnostics_ui.h"
static char printed[4096];static size_t max_chunk;
static void console_printf(const char *format,...)
{
    char chunk[2048];va_list args;va_start(args,format);
    int length=vsnprintf(chunk,sizeof(chunk),format,args);va_end(args);
    assert(length>=0&&(size_t)length<768U); /* Real console formatting buffer. */
    if((size_t)length>max_chunk)max_chunk=(size_t)length;
    assert(strlen(printed)+(size_t)length<sizeof(printed));strcat(printed,chunk);
}
static cerberus_s3_ble_memory_diagnostics_t memory_snapshot;
static unsigned memory_reads;
void cerberus_s3_ble_host_memory_diagnostics(cerberus_s3_ble_memory_diagnostics_t *out)
{ assert(!held);++memory_reads;*out=memory_snapshot; }
#include "dual_ble_diagnostics_console.h"
static void check_memory_console(void)
{
    printed[0]=0;unsigned before=scanner_reads+host_reads;
    print_ble_memory();assert(memory_reads==1U && scanner_reads+host_reads==before);
    assert(strstr(printed,"phase=before_init valid=0") && strstr(printed,"phase=refused valid=0"));
    for(unsigned i=0;i<CERBERUS_S3_BLE_MEMORY_STAGE_COUNT;++i)
        memory_snapshot.samples[i]=(cerberus_s3_ble_memory_sample_t){
            .receipt_us=INT64_MAX,.sequence=UINT32_MAX,.internal_free=UINT32_MAX,
            .internal_minimum=UINT32_MAX,.psram_free=UINT32_MAX,.psram_minimum=UINT32_MAX,
            .error=INT32_MIN,.requested_owner=UINT8_MAX,.initialized=true,.synced=true,.valid=true};
    printed[0]=0;print_ble_memory();assert(memory_reads==2U && scanner_reads+host_reads==before);
    assert(strstr(printed,"phase=released valid=1") && strstr(printed,"receipt_us=9223372036854775807"));
    assert(strstr(printed,"error=-2147483648") && strstr(printed,"psram_minimum=4294967295"));
    assert(strstr(printed,"not_ble_allocation_totals") && !strstr(printed,"largest"));
}

static void check_family_scopes(void)
{
    cerberus_dual_ble_diagnostics_t out;
    dual_ble_ui_mode=CERBERUS_DUAL_BLE_FAMILY;dual_ble_ui_family=1;
    storage.start.mode=CERBERUS_DUAL_BLE_FAMILY;storage.start.reserved=2;
    storage.published_family=6;
    dual_ble_publish_diagnostics(195,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.requested_family==1&&out.c5_requested_family==2&&out.c5_published_family==6);
    printed[0]=0;unsigned reads=scanner_reads+host_reads;print_dual_ble();
    assert(scanner_reads+host_reads==reads);
    assert(strstr(printed,"dual_ble_requested_family=1")&&strstr(printed,"dual_ble_c5_requested_family=2"));
    assert(strstr(printed,"dual_ble_c5_published_family=6")&&strstr(printed,"retained_bank_not_RF_identity"));
    assert(!strstr(printed,"address=")&&!strstr(printed,"ABCDEF"));
    dual_ble_ui_mode=-1;dual_ble_ui_family=0;
    cerberus_dual_ble_stop(&storage);
    dual_ble_publish_diagnostics(196,true);cerberus_dual_ble_diagnostics_get(&out);
    assert(!out.requested_family&&out.c5_requested_family==2&&out.c5_published_family==6);
    storage.start.mode=CERBERUS_DUAL_BLE_DISCOVERY;storage.start.reserved=0;
    storage.published_family=0;
    dual_ble_publish_diagnostics(197,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(!out.requested_family&&!out.c5_requested_family&&!out.c5_published_family);
}

int main(void)
{
    cerberus_dual_ble_diagnostics_t out,zero={0};
    cerberus_dual_ble_diagnostics_get(&out);assert(!memcmp(&out,&zero,sizeof(out)));
    unsigned locks=enters;cerberus_dual_ble_diagnostics_get(NULL);assert(enters==locks);
    dual_ble_publish_diagnostics(100,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.available&&out.sequence==1&&out.published_ms==100&&out.clock_valid&&out.version==3U);
    assert(!out.allocated&&out.released&&out.requested_mode==-1&&!scanner_reads);
    unsigned reads=scanner_reads+host_reads;print_dual_ble();
    assert(scanner_reads+host_reads==reads&&!held); /* Console only copies cached data. */
    assert(strstr(printed,"dual_ble_released=1")&&!strstr(printed,"address="));
    assert(strstr(printed,"dual_ble_c5_live_updates=0")&&strstr(printed,"dual_ble_c5_has_first_result=0"));

    dual_ble_ui_store=&storage;storage.start.mode=CERBERUS_DUAL_BLE_TRACK;
    storage.start.job_id=1;storage.start.address_type=1;memcpy(storage.start.address,"ABCDEF",6);
    storage.remote.flags=CERBERUS_DUAL_BLE_ATTACHED|CERBERUS_DUAL_BLE_OWNED;
    storage.remote.state=CERBERUS_BLE_SCAN_RUNNING;storage.count=3;storage.sweeps=4;
    storage.phase=5; /* Local reading and C5 reception may overlap. */
    storage.live_updates=true;storage.snapshots=17;storage.first_result_ms=638;storage.has_first_result=true;
    storage.has_track=true;storage.track_request_ms=100;
    storage.track=(cerberus_dual_ble_track_t){.age_ms=20,.reports=5,.rssi=-71,.flags=3};
    current_page=UI_PAGE_BLE;ble_mode=UI_BLE_MODE_FOX_HUNT;dual_ble_ui_mode=1;
    memcpy(ble_fox_hunt_target.address,"ABCDEF",6);ble_fox_hunt_target.address_type=1;
    source_sample=(cerberus_s3_ble_track_status_t){.enabled=true,.seen=true,.fresh=true,
        .rssi_dbm=-44,.reports=77,.age_ms=10,.address_type=0};
    memcpy(source_sample.address,"ABCDEF",6);
    ble_fox_hunt_start_pending=true;reads=scanner_reads;
    dual_ble_publish_diagnostics(110,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_pending&&!out.s3.target_matches&&!out.c5.target_matches&&!out.s3.seen&&!out.c5.seen);
    assert(scanner_reads==reads); /* Pending target never reads the preceding S3 tracker. */
    ble_fox_hunt_barrier_error=ESP_ERR_TIMEOUT;ble_fox_hunt_error=ESP_ERR_NO_MEM;
    dual_ble_publish_diagnostics(111,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_error==ESP_ERR_TIMEOUT&&out.fox_pending&&scanner_reads==reads);
    printed[0]=0;print_dual_ble();assert(strstr(printed,"dual_ble_fox_error=263"));
    ble_fox_hunt_barrier_error=ESP_OK;
    dual_ble_publish_diagnostics(112,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_error==ESP_ERR_NO_MEM);
    ble_fox_hunt_cleanup_timed_out=true;
    dual_ble_publish_diagnostics(113,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_error==ESP_ERR_TIMEOUT&&out.s3_track_error==ESP_OK&&out.c5_error==ESP_OK);
    ble_fox_hunt_error=ESP_OK;
    dual_ble_publish_diagnostics(114,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_error==ESP_ERR_TIMEOUT);
    ble_fox_hunt_cleanup_timed_out=false;ble_fox_hunt_barrier_error=ESP_ERR_TIMEOUT;
    dual_ble_publish_diagnostics(115,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_error==ESP_ERR_TIMEOUT);
    ble_mode=0;dual_ble_publish_diagnostics(116,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(!out.fox_open&&out.fox_error==ESP_OK);
    ble_mode=UI_BLE_MODE_FOX_HUNT;ble_fox_hunt_barrier_error=ESP_OK;
    dual_ble_publish_diagnostics(117,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.fox_error==ESP_OK);
    ble_fox_hunt_start_pending=false;dual_ble_publish_diagnostics(120,false);
    cerberus_dual_ble_diagnostics_get(&out);
    assert(!out.s3.target_matches&&!out.s3.seen&&out.c5.target_matches&&out.c5.seen&&out.c5.age_ms==40);
    source_sample.address_type=1;dual_ble_publish_diagnostics(130,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.s3.target_matches&&out.s3.rssi==-44&&out.s3.reports==77);
    assert(out.c5.rssi==-71&&out.c5.reports==5&&out.c5.age_ms==50);
    assert(out.c5_live_updates&&out.c5_snapshots==17&&out.c5_sweeps==4);
    assert(out.receiving&&out.reading);
    assert(out.c5_has_first_result&&out.c5_first_result_ms==638); /* Reader latency is not sample age. */
    printed[0]=0;reads=scanner_reads+host_reads;print_dual_ble();
    assert(scanner_reads+host_reads==reads);
    assert(strstr(printed,"dual_ble_c5_snapshots=17")&&strstr(printed,"dual_ble_c5_sweeps=4"));
    assert(strstr(printed,"dual_ble_c5_first_result_ms=638")&&strstr(printed,"first_new_nonempty_bank_not_RF_time"));
    unsigned long long prior_sequence=out.sequence;
    dual_ble_publish_diagnostics(90,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.sequence==prior_sequence+1&&!out.clock_valid&&out.published_ms==130);
    assert(!out.s3.fresh&&!out.c5.fresh&&out.s3.age_ms==UINT32_MAX);
    assert(out.c5_has_first_result&&out.c5_first_result_ms==638); /* Historical latency remains separate. */
    dual_ble_publish_diagnostics(140,false);cerberus_dual_ble_diagnostics_get(&out);assert(out.clock_valid);
    ++ble_fox_hunt_target.address[0];dual_ble_publish_diagnostics(150,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(!out.s3.target_matches&&!out.c5.target_matches&&!out.s3.seen&&!out.c5.seen);
    --ble_fox_hunt_target.address[0];sample_error=ESP_ERR_INVALID_STATE;
    dual_ble_publish_diagnostics(160,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.s3_track_error==ESP_ERR_INVALID_STATE&&!out.s3.seen);
    sample_error=ESP_OK;cerberus_dual_ble_stop(&storage);storage.remote.flags=CERBERUS_DUAL_BLE_ATTACHED;
    ble_fox_hunt_exit_pending=true;dual_ble_publish_diagnostics(170,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.stopping&&out.attached&&!out.released&&!out.receiving&&out.fox_stopping);
    storage.start.job_id=0;dual_ble_ui_mode=-1;dual_ble_publish_diagnostics(180,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.released&&!out.c5.target_matches&&!out.c5.seen);
    /* Old released discovery metadata cannot replace the current Fox scan state. */
    ble_discovery_live=&discovery;discovery.session.have_observation=true;
    discovery.session.observation.state=CERBERUS_BLE_SCAN_ERROR;
    ble_scan_checked=true;ble_scan_status.state=CERBERUS_BLE_SCAN_RUNNING;
    dual_ble_publish_diagnostics(190,false);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.s3_scan_cached&&out.s3_state==CERBERUS_BLE_SCAN_RUNNING);
    printed[0]=0;reads=scanner_reads+host_reads;print_dual_ble();assert(scanner_reads+host_reads==reads);
    assert(strstr(printed,"dual_ble_s3_rssi=-44")&&strstr(printed,"independent_received_reports"));
    check_family_scopes();
    cerberus_dual_ble_diagnostics_t input=out;input.c5_sweeps=123;
    input.c5_snapshots=UINT32_MAX;input.c5_first_result_ms=UINT32_MAX;input.c5_has_first_result=true;
    input.c5_inventory_known=input.c5_inventory_replacement_enabled=true;
    input.c5_inventory_replaced=UINT32_MAX;input.c5_inventory_dropped=123456;
    cerberus_dual_ble_diagnostics_publish(&input,200);input.c5_sweeps=999;
    cerberus_dual_ble_diagnostics_get(&out);assert(out.c5_sweeps==123);
    assert(out.c5_snapshots==UINT32_MAX&&out.c5_first_result_ms==UINT32_MAX);
    assert(out.c5_inventory_known&&out.c5_inventory_replacement_enabled&&
        out.c5_inventory_replaced==UINT32_MAX&&out.c5_inventory_dropped==123456);
    printed[0]=0;reads=scanner_reads+host_reads;print_dual_ble();assert(scanner_reads+host_reads==reads);
    assert(strstr(printed,"dual_ble_c5_snapshots=4294967295")&&strstr(printed,"dual_ble_c5_first_result_ms=4294967295"));
    assert(strstr(printed,"dual_ble_c5_inventory_replaced=4294967295")&&
        strstr(printed,"dual_ble_c5_inventory_dropped=123456")&&
        strstr(printed,"last_published_bank_not_unique_devices_or_RF_loss"));
    published.sequence=UINT64_MAX-1;cerberus_dual_ble_diagnostics_publish(&input,201);
    cerberus_dual_ble_diagnostics_get(&out);assert(out.sequence==UINT64_MAX&&!out.sequence_saturated);
    cerberus_dual_ble_diagnostics_publish(NULL,-1);cerberus_dual_ble_diagnostics_get(&out);
    assert(out.sequence==UINT64_MAX&&out.sequence_saturated&&!out.available&&!out.clock_valid&&out.published_ms==201);
    check_memory_console();
    printf("Production BLE diagnostics passed: snapshot=%zu bytes, max console chunk=%zu bytes; identity/pending/release/clock/getter isolation\n",sizeof(out),max_chunk);
    return 0;
}
