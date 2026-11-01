/* Wire both production ends together. Only RF/controller completion, mutex,
 * clock, packet parser and transport failure are mocked by the owner shim. */
#define main c5_owner_fixture_main
#include "dual_ble_owner_check.c"
#undef main
#include "dual_ble_scan.h"
#include "c5_ble_legacy_getters.h"

typedef struct {
    bool legacy, lose_begin, mutated, extended, old_metadata, bad_fence;
    uint64_t rf_started_ms, first_begin_ms, fallback_handle;
    unsigned begin_calls, reads, releases, frames, metadata_reads, old_reads, unsupported_at;
} integration_t;

static uint64_t integration_clock(void *context) { (void)context; return now_ms; }
static void integration_report(int8_t rssi, uint8_t payload)
{
    uint8_t raw[]={2,1,payload};
    struct ble_gap_disc_desc packet={.addr={{1,2,3,4,5,6},1},.rssi=rssi,
        .event_type=0,.data=raw,.length_data=sizeof(raw)};
    struct ble_gap_event event={.type=BLE_GAP_EVENT_DISC,.disc=packet};
    /* Actual callback obtains its own clock; fixtures do not substitute an
     * invented source time for the receive metadata path. */
    assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));
}
static void integration_extended_report(uint8_t address,uint8_t type,int8_t rssi,uint8_t sid)
{
    uint8_t payload[255]={254,0xff,0x4c,0};
    struct ble_gap_event event={.type=BLE_GAP_EVENT_EXT_DISC,.ext_disc={
        .addr={{address,2,3,4,5,6},type},.rssi=rssi,.sid=sid,.prim_phy=3,.sec_phy=1,
        .length_data=sizeof(payload),.data=payload}};
    assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));
}

static esp_err_t integration_exchange(void *context,uint8_t command_id,const void *request,
    size_t request_size,void *response,size_t response_size)
{
    integration_t *i=context;++i->frames;
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ_METADATA){
        ++i->metadata_reads;
        if(i->old_metadata&&i->metadata_reads==i->unsupported_at){
            const cerberus_dual_ble_snapshot_read_t *r=request;
            i->fallback_handle=r->handle;now_ms+=8U;return ESP_ERR_NOT_SUPPORTED;
        }
    }
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ){
        ++i->old_reads;
        if(i->old_metadata){const cerberus_dual_ble_snapshot_read_t *r=request;
            assert(r->handle==i->fallback_handle);}
    }
    if(command_id==CERBERUS_SPI_COMMAND_BLE_SCAN_RESULT){
        assert(i->legacy&&request_size==sizeof(cerberus_ble_scan_result_request_t));
        const cerberus_ble_scan_result_request_t *r=request;
        esp_err_t result=cerberus_ble_scanner_get_result(r->generation,r->index,response);now_ms+=8U;return result;
    }
    if(command_id==CERBERUS_SPI_COMMAND_BLE_SCAN_DATA){
        assert(i->legacy&&request_size==sizeof(cerberus_ble_scan_data_request_t));
        const cerberus_ble_scan_data_request_t *r=request;
        esp_err_t result=cerberus_ble_scanner_get_data_chunk(r->generation,r->index,r->kind,r->chunk,response);now_ms+=8U;return result;
    }
    if(command_id>=CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN&&
       command_id<=CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_STATUS)assert(!i->legacy);
    unsigned old_starts=starts;size_t size=0;
    bool handled=cerberus_ble_dual_command(command_id,request,request_size,response,&size);
    assert(handled&&size==response_size&&!held);
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_INFO&&i->legacy){
        /* Compatibility shim presents the old INFO capability/version only;
         * accepted v1 START/status and completed-bank getters remain actual C5. */
        cerberus_dual_ble_info_t *info=response;info->version=1;
        info->flags&=(uint8_t)~CERBERUS_DUAL_BLE_SNAPSHOTS;
    }
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_START&&starts!=old_starts){
        i->rf_started_ms=now_ms;
        if(i->extended)integration_extended_report(1,1,-30,7);else integration_report(-30,6);
        memcpy(scan_results[0].name,"REAL",4);scan_results[0].name_length=4;
        scan_results[0].uuid_length=2;scan_results[0].uuid[0]=0xaa;scan_results[0].uuid[1]=0xfe;
        scan_results[0].manufacturer_id=0x004c;
    }
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN){
        ++i->begin_calls;if(!i->first_begin_ms)i->first_begin_ms=now_ms;
        if(i->lose_begin){i->lose_begin=false;now_ms+=8U;return ESP_ERR_TIMEOUT;}
    }
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ||
        command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ_METADATA){
        ++i->reads;
        /* This update occurs AFTER the first real chunk reply, while remaining
         * chunks are still in flight. No mixture may reach the S3 publisher. */
        if(!i->mutated){
            if(i->extended||scan_status.stored_count==32)integration_extended_report(1,1,-90,12);
            else integration_report(-90,7);
            i->mutated=true;
        }
    }
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_STATUS&&i->bad_fence)
        ++((cerberus_dual_ble_snapshot_status_t*)response)->count;
    if(command_id==CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE)++i->releases;
    now_ms+=8U; /* Exercise the real eight-ms tick budget with one in-flight exchange. */
    return ESP_OK;
}

static void integration_reset(integration_t *i,cerberus_dual_ble_scan_t *s,bool legacy)
{
    assert(!held);memset(i,0,sizeof(*i));i->legacy=legacy;
    memset(scan_results,0,sizeof(scan_results));memset(&scan_status,0,sizeof(scan_status));
    memset(&dual_snapshot,0,sizeof(dual_snapshot));
    dual_boot_id=UINT64_C(0x123456789abcdef);dual_snapshot.counter=dual_boot_id;
    dual_epoch=dual_epoch_counter=0;dual_job_id=dual_job_highwater=dual_last_contact_ms=0;
    dual_attached=dual_radio_active=dual_track_seen=false;dual_error=0;
    scan_callback_epoch=0;starts=stops=0;other_radio_busy=cancel_fails=snapshot_busy=false;
    host_synced=true;parse_fails=false;now_ms=1000;
    cerberus_dual_ble_ops_t ops={.context=i,.exchange=integration_exchange,.now_ms=integration_clock};
    cerberus_dual_ble_init(s,&ops);
    assert(cerberus_dual_ble_begin(s,CERBERUS_DUAL_BLE_DISCOVERY,NULL,0)==ESP_OK);
}

static void integration_tick(integration_t *i,cerberus_dual_ble_scan_t *s)
{
    now_ms+=20;
    if(i->rf_started_ms&&dual_radio_active&&now_ms-i->rf_started_ms>=8000U){
        /* Mock the existing eight-second controller completion only. */
        dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_COMPLETE;
    }
    unsigned frames=i->frames;cerberus_dual_ble_tick(s,now_ms);
    assert(i->frames-frames<=1U); /* Each mocked transport costs the full8ms tick budget. */
    assert(!s->error&&!held);
}

int main(void)
{
    integration_t i;cerberus_dual_ble_scan_t s;
    integration_reset(&i,&s,false);i.lose_begin=true;
    for(unsigned tick=0;tick<300U&&!s.snapshots;++tick)integration_tick(&i,&s);
    assert(s.snapshots==1&&s.count==1&&s.sweeps==0&&dual_radio_active&&starts==1&&!stops);
    assert(s.has_first_result&&s.first_result_ms<8000U&&s.live_updates&&i.begin_calls>=2&&i.mutated);
    cerberus_dual_ble_row_t row;assert(cerberus_dual_ble_row(&s,0,&row));
    cerberus_dual_ble_provenance_t source;
    assert(cerberus_dual_ble_copy_row(&s,0,&row,&source));
    assert(source.boot_id==dual_boot_id&&source.epoch==dual_epoch&&source.generation==scan_status.generation&&source.index==0);
    assert(row.rssi==-30&&row.reports==1&&row.raw_length==3&&row.raw[2]==6);
    assert(row.receive.source_kind==CERBERUS_DUAL_BLE_RECEIVE_LEGACY&&row.receive.data_status==255);
    assert(!strcmp(row.name,"REAL")&&row.uuid_length==2&&row.manufacturer_id==0x004c);
    assert(row.request_started_ms==i.first_begin_ms);
    assert(scan_results[0].latest_rssi==-90&&scan_results[0].advertisement[2]==7);
    for(unsigned tick=0;tick<500U&&!s.sweeps;++tick)integration_tick(&i,&s);
    assert(s.sweeps==1&&now_ms-i.rf_started_ms>=8000U&&s.snapshots>1);
    assert(cerberus_dual_ble_row(&s,0,&row)&&row.rssi==-90&&row.raw[2]==7&&row.reports==2);
    /* Prevent automatic next sweep only after final publication: release must
       still pass through the actual owner handler and settle locally. */
    s.enabled=false;
    for(unsigned tick=0;tick<20U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
    assert(cerberus_dual_ble_released(&s)&&!dual_attached&&!dual_snapshot.valid&&i.releases&&starts==1&&!stops);
    assert(cerberus_dual_ble_copy_row(&s,0,&row,&source));
    assert(source.boot_id==s.published_boot_id&&source.epoch==s.published_epoch&&!s.remote.epoch&&!s.start.job_id);

    integration_reset(&i,&s,false);
    for(unsigned tick=0;tick<100U&&!i.reads;++tick)integration_tick(&i,&s);
    assert(i.reads&&dual_radio_active&&!s.snapshots);
    cerberus_dual_ble_stop(&s);
    for(unsigned tick=0;tick<50U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
    assert(!dual_radio_active&&!dual_attached&&!dual_snapshot.valid&&stops==1&&!s.snapshots);

    integration_reset(&i,&s,true);
    for(unsigned tick=0;tick<300U&&(!i.rf_started_ms||now_ms-i.rf_started_ms<6000U);++tick)integration_tick(&i,&s);
    assert(dual_radio_active&&!s.snapshots&&!s.sweeps&&!i.begin_calls&&!i.reads);
    for(unsigned tick=0;tick<250U&&!s.sweeps;++tick)integration_tick(&i,&s);
    assert(s.sweeps==1&&s.count==1&&!s.live_updates&&!i.begin_calls&&!i.reads);
    assert(cerberus_dual_ble_row(&s,0,&row)&&row.rssi==-30&&!strcmp(row.name,"REAL")&&row.raw[2]==6);
    cerberus_dual_ble_receive_metadata_t no_metadata={0};
    assert(!memcmp(&row.receive,&no_metadata,sizeof(no_metadata))); /* v1 has no metadata claim. */
    assert(cerberus_dual_ble_copy_row(&s,0,&row,&source));
    assert(source.boot_id==dual_boot_id&&source.epoch==dual_epoch&&source.generation==scan_status.generation&&source.index==0);
    s.enabled=false;
    for(unsigned tick=0;tick<20U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
    assert(cerberus_dual_ble_released(&s)&&!dual_attached&&starts==1&&!stops);
    /* Dense reports run through the actual C5 callback/admission policy and
     * frozen wire commands. A later replacement during the download cannot
     * alter either the published rows or their published counter cohort. */
    integration_reset(&i,&s,false);
    for(unsigned tick=0;tick<20U&&!i.rf_started_ms;++tick)integration_tick(&i,&s);
    uint8_t raw[]={2,1,6};
    struct ble_gap_disc_desc dense={.addr={{2,2,3,4,5,6},1},.rssi=-70,.event_type=0,
        .data=raw,.length_data=sizeof(raw)};
    for(unsigned address=2;address<=33;++address){
        dense.addr.val[0]=(uint8_t)address;++now_ms;
        record_advertisement(&dense,scan_callback_epoch,(int64_t)now_ms*1000LL);
    }
    assert(scan_status.stored_count==32&&scan_inventory.replaced==1);
    for(unsigned tick=0;tick<700U&&!s.snapshots;++tick)integration_tick(&i,&s);
    assert(s.snapshots==1&&s.count==32&&s.inventory_known&&s.inventory_replacement_enabled);
    assert(s.inventory_replaced==1&&!s.inventory_dropped&&scan_inventory.replaced==2);
    cerberus_dual_ble_row_t painted;cerberus_dual_ble_provenance_t painted_source;
    assert(cerberus_dual_ble_copy_row(&s,1,&painted,&painted_source)&&painted.address[0]==2);
    for(unsigned tick=0;tick<700U&&s.snapshots<2;++tick)integration_tick(&i,&s);
    assert(s.snapshots==2&&s.inventory_known&&s.inventory_replaced==2);
    assert(cerberus_dual_ble_copy_row(&s,1,&row,&source)&&row.address[0]==1);
    assert(painted.address[0]==2&&painted_source.boot_id==source.boot_id&&painted_source.epoch==source.epoch);
    assert(painted.receive.source_kind==CERBERUS_DUAL_BLE_RECEIVE_LEGACY&&painted.receive.callback_length==3);
    assert(row.receive.source_kind==CERBERUS_DUAL_BLE_RECEIVE_EXTENDED&&row.receive.sid==12&&row.receive.callback_length==255);
    cerberus_dual_ble_stop(&s);
    for(unsigned tick=0;tick<50U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
    assert(cerberus_dual_ble_released(&s)&&!dual_attached);
    /* Real EXT_DISC normalization -> immutable110 wire -> S3, with typed
     * selection surviving newer bytes/SID and a different typed identity. */
    integration_reset(&i,&s,false);i.extended=true;i.lose_begin=true;
    for(unsigned tick=0;tick<200U&&!s.snapshots;++tick)integration_tick(&i,&s);
    assert(s.snapshots==1&&starts==1&&i.metadata_reads==11&&!i.old_reads&&i.begin_calls>=2);
    assert(cerberus_dual_ble_copy_row(&s,0,&painted,&painted_source));
    assert(painted.address_type==1&&painted.receive.source_kind==CERBERUS_DUAL_BLE_RECEIVE_EXTENDED);
    assert(painted.receive.sid==7&&painted.receive.primary_phy==3&&painted.receive.secondary_phy==1);
    assert(painted.receive.callback_length==255&&painted.receive.ad_validation==CERBERUS_DUAL_BLE_AD_VALID);
    assert(painted.raw_length==31&&painted.raw_flags==3&&painted.rssi==-30);
    assert(painted.request_started_ms==i.first_begin_ms&&scan_results[0].receive.sid==12);
    integration_extended_report(1,0,-50,2); /* Same bytes, different address type. */
    for(unsigned tick=0;tick<250U&&s.snapshots<2;++tick)integration_tick(&i,&s);
    assert(s.count==2&&cerberus_dual_ble_row(&s,0,&row)&&row.receive.sid==12&&row.rssi==-90);
    assert(cerberus_dual_ble_row(&s,1,&row)&&row.address_type==0&&row.receive.sid==2);
    assert(painted.address_type==1&&painted.receive.sid==7&&painted.rssi==-30);
    cerberus_dual_ble_stop(&s);
    for(unsigned tick=0;tick<50U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
    assert(cerberus_dual_ble_released(&s)&&!dual_attached);
    /* Unsupported opt-in metadata, both immediately and after partial reads,
     * rereads the SAME frozen handle fromrow0. No new BEGIN/job/RF scan. */
    for(unsigned unsupported=1;unsupported<=3;unsupported+=2){
        integration_reset(&i,&s,false);i.extended=true;i.old_metadata=true;i.unsupported_at=unsupported;
        for(unsigned tick=0;tick<250U&&!s.snapshots;++tick)integration_tick(&i,&s);
        assert(s.snapshots==1&&starts==1&&i.begin_calls==1&&i.metadata_reads==unsupported&&i.old_reads==11);
        assert(cerberus_dual_ble_row(&s,0,&row)&&row.rssi==-30&&row.request_started_ms==i.first_begin_ms);
        assert(!memcmp(&row.receive,&no_metadata,sizeof(no_metadata))&&!s.metadata_read_enabled);
        assert(dual_snapshot.handle==i.fallback_handle);
        cerberus_dual_ble_stop(&s);
        for(unsigned tick=0;tick<50U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
        assert(cerberus_dual_ble_released(&s)&&starts==1&&stops==1);
    }
    for(unsigned stop_at=1;stop_at<=11;stop_at+=10){
        integration_reset(&i,&s,false);i.extended=true;
        for(unsigned tick=0;tick<200U&&i.metadata_reads<stop_at;++tick)integration_tick(&i,&s);
        assert(!s.snapshots&&i.metadata_reads==stop_at);
        cerberus_dual_ble_stop(&s);
        for(unsigned tick=0;tick<50U&&!cerberus_dual_ble_released(&s);++tick)integration_tick(&i,&s);
        assert(cerberus_dual_ble_released(&s)&&!s.snapshots&&!dual_snapshot.valid&&starts==1&&stops==1);
    }
    integration_reset(&i,&s,false);i.extended=true;i.bad_fence=true;
    for(unsigned tick=0;tick<250U&&!s.error;++tick){now_ms+=20;cerberus_dual_ble_tick(&s,now_ms);}
    assert(s.error&&!s.snapshots&&!s.count);
    i.bad_fence=false;cerberus_dual_ble_stop(&s);
    for(unsigned tick=0;tick<50U&&!cerberus_dual_ble_released(&s);++tick){now_ms+=20;cerberus_dual_ble_tick(&s,now_ms);}
    assert(cerberus_dual_ble_released(&s)&&!dual_attached&&!s.snapshots);
    puts("Actual C5 service/callback + S3 coordinator: live immutable readout, lost-BEGIN age, final 8s sweep, Stop and v1 completed-bank fallback passed");
    return 0;
}
