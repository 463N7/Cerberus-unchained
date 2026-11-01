#include "cerberus_protocol.h"
#include "esp_err.h"
#include "ble_inventory_admission.h"
#include "dual_ble_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MYNEWT_VAL(x) 1
#define pdTRUE 1
#define CERBERUS_BLE_NAME_MAX_LENGTH 32U
#define CERBERUS_BLE_UUID_MAX_LENGTH 16U
#define CERBERUS_BLE_ADVERTISEMENT_MAX_LENGTH 31U
#define portMAX_DELAY 0xffffffffU
#define BLE_HCI_ADV_RPT_EVTYPE_ADV_IND 0
#define BLE_HCI_ADV_RPT_EVTYPE_DIR_IND 1
#define BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND 3
#define BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP 4
#define BLE_HCI_ADV_CONN_MASK 1
#define BLE_HCI_ADV_SCAN_RSP_MASK 8
#define BLE_HCI_ADV_LEGACY_MASK 16
#define BLE_GAP_EXT_ADV_DATA_STATUS_COMPLETE 0
enum { BLE_GAP_EVENT_DISC=1, BLE_GAP_EVENT_EXT_DISC, BLE_GAP_EVENT_DISC_COMPLETE };
typedef struct {uint8_t val[6],type;} address_t;
struct ble_gap_disc_desc {address_t addr;int8_t rssi;uint8_t event_type;
    const uint8_t *data;uint8_t length_data;};
/* Scalar callback fields match the installed NimBLE ble_gap.h. In particular,
 * report props and delivered lengths are uint8_t, not wider invented fields. */
struct ble_gap_ext_disc_desc {uint8_t props,data_status,legacy_event_type;address_t addr;
    int8_t rssi,tx_power;uint8_t sid,prim_phy,sec_phy;uint16_t periodic_adv_itvl;
    uint8_t length_data;const uint8_t *data;address_t direct_addr;};
struct ble_gap_event {int type;struct ble_gap_disc_desc disc;struct ble_gap_ext_disc_desc ext_disc;
    struct {int reason;} disc_complete;};
struct ble_hs_adv_fields {int unused;};
#include "c5_ble_record_type.h"
typedef struct {int unused;} sentinel_watch_protocol_record_t;
static struct {struct {uint16_t identities;} status;} activity_capture;
static unsigned watch_capture;
static cerberus_ble_record_t scan_results[32];
static cerberus_ble_scan_status_payload_t scan_status;
static uint32_t scan_callback_epoch;
#include "c5_ble_dual_state.h"
static bool host_synced;
static void *scanner_mutex=(void*)1;
static unsigned held, starts, stops;
static uint64_t now_ms;
static bool other_radio_busy, cancel_fails, snapshot_busy, parse_fails;
static unsigned try_locks;
static int64_t esp_timer_get_time(void) {return (int64_t)(now_ms*1000U);}
static int xSemaphoreTake(void *m,unsigned wait)
{(void)m;if(!wait){++try_locks;if(snapshot_busy||held)return 0;}assert(!held);held=1;return 1;}
static void xSemaphoreGive(void *m) {(void)m;assert(held);held=0;}
static esp_err_t start_scan(bool companion)
{
    assert(companion && dual_attached && !held);
    if(other_radio_busy)return ESP_ERR_INVALID_STATE;
    ++starts;++scan_status.generation;dual_generation=scan_status.generation;
    ++scan_callback_epoch;scan_status.state=CERBERUS_BLE_SCAN_RUNNING;
    scan_status.stored_count=0;scan_status.error_code=0;dual_radio_active=true;
    scan_status.dropped_reports=0;
    cerberus_ble_inventory_admission_begin(&scan_inventory,
        dual_version==CERBERUS_DUAL_BLE_VERSION&&dual_mode==CERBERUS_DUAL_BLE_DISCOVERY,
        esp_timer_get_time());
    return ESP_OK;
}
static esp_err_t stop_scan(bool companion)
{
    assert(companion && dual_attached && !held);++stops;
    if(cancel_fails){dual_error=ESP_FAIL;return ESP_FAIL;}
    dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_IDLE;return ESP_OK;
}
static int ble_hs_adv_parse_fields(struct ble_hs_adv_fields *f,const void *d,unsigned n)
{(void)f;(void)d;(void)n;return parse_fails?1:0;}
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
static size_t bounded_size(size_t value,size_t limit){return value<limit?value:limit;}
#include "c5_ble_callback_impl.h"
static void finish_scan_for_epoch(int reason,uint32_t epoch)
{if(epoch&&epoch==scan_callback_epoch){dual_radio_active=false;
 scan_status.state=reason?CERBERUS_BLE_SCAN_ERROR:CERBERUS_BLE_SCAN_COMPLETE;}}
#include "c5_ble_gap_receive_impl.h"
#include "dual_ble_service_impl.h"

static void command(uint8_t cmd,const void *request,size_t n,void *reply,size_t expected)
{size_t size=0;assert(cerberus_ble_dual_command(cmd,request,n,reply,&size));assert(size==expected);assert(!held);}

static cerberus_dual_ble_snapshot_status_t snapshot_begin(cerberus_dual_ble_binding_t b,uint64_t id)
{cerberus_dual_ble_snapshot_begin_t r={.binding=b,.request_id=id};cerberus_dual_ble_snapshot_status_t out;
 command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN,&r,sizeof(r),&out,sizeof(out));return out;}
static cerberus_dual_ble_snapshot_status_t snapshot_status(cerberus_dual_ble_binding_t b,uint64_t handle)
{cerberus_dual_ble_snapshot_binding_t r={.binding=b,.handle=handle};cerberus_dual_ble_snapshot_status_t out;
 command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_STATUS,&r,sizeof(r),&out,sizeof(out));return out;}
static cerberus_dual_ble_snapshot_inventory_t snapshot_inventory(cerberus_dual_ble_binding_t b,uint64_t handle)
{cerberus_dual_ble_snapshot_binding_t r={.binding=b,.handle=handle};cerberus_dual_ble_snapshot_inventory_t out;
 command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_INVENTORY,&r,sizeof(r),&out,sizeof(out));return out;}
static cerberus_dual_ble_snapshot_chunk_t snapshot_chunk(cerberus_dual_ble_binding_t b,uint64_t handle,uint8_t index,uint8_t chunk)
{cerberus_dual_ble_snapshot_read_t r={.binding=b,.handle=handle,.index=index,.chunk=chunk};cerberus_dual_ble_snapshot_chunk_t out;
 command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ,&r,sizeof(r),&out,sizeof(out));
 assert(out.handle==handle&&out.index==index&&out.chunk==chunk);
 if(out.error){assert(!out.byte_count);for(unsigned i=0;i<10;++i)assert(!out.data[i]);}return out;}
static cerberus_dual_ble_snapshot_row_t snapshot_row(cerberus_dual_ble_binding_t b,uint64_t handle,uint8_t index)
{cerberus_dual_ble_snapshot_row_t row={0};for(uint8_t i=0;i<CERBERUS_DUAL_BLE_SNAPSHOT_CHUNKS;++i){
 cerberus_dual_ble_snapshot_chunk_t c=snapshot_chunk(b,handle,index,i);assert(!c.error);
 size_t n=i==10?2U:10U;assert(c.byte_count==n);memcpy((uint8_t*)&row+i*10U,c.data,n);
 for(size_t j=n;j<10U;++j)assert(!c.data[j]);}return row;}
static cerberus_dual_ble_snapshot_metadata_row_t snapshot_metadata_row(
    cerberus_dual_ble_binding_t b,uint64_t handle,uint8_t index)
{
    cerberus_dual_ble_snapshot_metadata_row_t row={0};
    for(uint8_t chunk=0;chunk<CERBERUS_DUAL_BLE_SNAPSHOT_METADATA_CHUNKS;++chunk){
        cerberus_dual_ble_snapshot_read_t request={b,handle,index,chunk};
        cerberus_dual_ble_snapshot_chunk_t reply;
        command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ_METADATA,&request,sizeof(request),&reply,sizeof(reply));
        assert(!reply.error&&reply.handle==handle&&reply.index==index&&reply.chunk==chunk&&reply.byte_count==10);
        memcpy((uint8_t*)&row+chunk*10U,reply.data,10);
    }
    cerberus_dual_ble_snapshot_row_t old=snapshot_row(b,handle,index);
    assert(!memcmp(&row.row,&old,sizeof(old))); /* Old READ remains102 with final8zero padding. */
    assert(cerberus_dual_ble_receive_metadata_valid(&row.receive,row.row.raw_length,row.row.event_type,row.row.raw_flags));
    return row;
}

static void callback_metadata_checks(void)
{
    _Static_assert(sizeof(((struct ble_gap_ext_disc_desc*)0)->props)==1,"SDK props width");
    _Static_assert(sizeof(((struct ble_gap_ext_disc_desc*)0)->length_data)==1,"SDK report length width");
    dual_attached=dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_IDLE;dual_error=0;now_ms=100000;
    cerberus_dual_ble_start_t start={.boot_id=dual_boot_id,.job_id=dual_job_highwater+1U,.version=2};
    cerberus_dual_ble_status_t owner;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,sizeof(start),&owner,sizeof(owner));
    assert(!owner.error_code&&dual_radio_active);
    cerberus_dual_ble_binding_t binding={dual_boot_id,start.job_id};uint64_t request=0;
    struct ble_gap_event event={.type=BLE_GAP_EVENT_DISC,
        .disc={.addr={{22,2,3,4,5,6},1},.rssi=-37,.event_type=0}};
    uint8_t payload[255]={0};const uint8_t lengths[]={0,31,32,255};
    for(unsigned extended=0;extended<2;++extended)for(unsigned n=0;n<sizeof(lengths);++n){
        uint8_t length=lengths[n];memset(payload,0,sizeof(payload));
        if(length){payload[0]=(uint8_t)(length-1U);payload[1]=0xff;payload[2]=0x4c;}
        event.type=extended?BLE_GAP_EVENT_EXT_DISC:BLE_GAP_EVENT_DISC;
        event.disc.length_data=length;event.disc.data=length?payload:NULL;
        event.ext_disc=(struct ble_gap_ext_disc_desc){.addr=event.disc.addr,.rssi=-37,
            .sid=7,.prim_phy=3,.sec_phy=1,.length_data=length,.data=length?payload:NULL};
        ++now_ms;assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));
        cerberus_dual_ble_snapshot_status_t status=snapshot_begin(binding,++request);assert(!status.error_code&&status.count==1);
        cerberus_dual_ble_snapshot_metadata_row_t frozen=snapshot_metadata_row(binding,status.handle,0);
        assert(frozen.row.raw_length==(length>31?31:length));
        if(!extended&&length>31){cerberus_dual_ble_receive_metadata_t zero={0};assert(!memcmp(&frozen.receive,&zero,sizeof(zero)));}
        else {
            assert(frozen.receive.source_kind==(extended?CERBERUS_DUAL_BLE_RECEIVE_EXTENDED:CERBERUS_DUAL_BLE_RECEIVE_LEGACY));
            assert(frozen.receive.callback_length==length&&frozen.receive.ad_validation==CERBERUS_DUAL_BLE_AD_VALID);
            assert(frozen.receive.sid==(extended?7:255)&&frozen.receive.data_status==(extended?0:255));
            assert(frozen.receive.primary_phy==(extended?3:0)&&frozen.receive.secondary_phy==(extended?1:0));
        }
        if(length>31)assert(frozen.row.raw_flags==3); /* Cut AD preview; full received AD still independently VALID. */
        else assert(!frozen.row.raw_flags);
    }
    event.type=BLE_GAP_EVENT_EXT_DISC;event.ext_disc.length_data=31;event.ext_disc.data=payload;
    memset(payload,0,sizeof(payload));payload[0]=31;payload[1]=0xff;payload[2]=0x4c;
    ++now_ms;assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));
    cerberus_dual_ble_snapshot_status_t status=snapshot_begin(binding,++request);
    cerberus_dual_ble_snapshot_metadata_row_t malformed=snapshot_metadata_row(binding,status.handle,0);
    assert(malformed.receive.ad_validation==CERBERUS_DUAL_BLE_AD_INVALID&&malformed.row.raw_flags==2);
    payload[0]=30;
    parse_fails=true;++now_ms;assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));parse_fails=false;
    status=snapshot_begin(binding,++request);
    malformed=snapshot_metadata_row(binding,status.handle,0);
    assert(malformed.receive.ad_validation==CERBERUS_DUAL_BLE_AD_VALID&&malformed.row.raw_flags==2);
    /* Full-container layout validation stays independent of NimBLE parser failure. */
    const uint8_t props[]={0x10,0x12,0x13,0x15},events[]={3,2,0,1};
    for(unsigned i=0;i<sizeof(props);++i){
        event.ext_disc.props=props[i];event.ext_disc.legacy_event_type=events[i];
        event.ext_disc.sid=255;event.ext_disc.prim_phy=1;event.ext_disc.sec_phy=0;
        ++now_ms;assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));
        status=snapshot_begin(binding,++request);
        cerberus_dual_ble_snapshot_metadata_row_t row=snapshot_metadata_row(binding,status.handle,0);
        assert(row.receive.source_kind==CERBERUS_DUAL_BLE_RECEIVE_EXTENDED&&row.receive.properties==props[i]);
        assert(row.row.event_type==events[i]&&row.receive.sid==255&&!row.receive.data_status);
    }
    /* Full source metadata unknown/invalid never invents a PHY/SID. Raw
     * admission stays separate and old immutable handle metadata stays fixed. */
    event.ext_disc.props=0;event.ext_disc.prim_phy=3;event.ext_disc.sec_phy=2;event.ext_disc.sid=15;
    ++now_ms;assert(!gap_event(&event,(void*)(uintptr_t)scan_callback_epoch));
    status=snapshot_begin(binding,++request);
    cerberus_dual_ble_snapshot_metadata_row_t painted=snapshot_metadata_row(binding,status.handle,0);
    const uint64_t painted_handle=status.handle;
    for(unsigned fault=0;fault<5;++fault){
        struct ble_gap_event bad=event;
        if(fault==0)bad.ext_disc.sid=16;
        if(fault==1)bad.ext_disc.prim_phy=2;
        if(fault==2)bad.ext_disc.sec_phy=4;
        if(fault==3)bad.ext_disc.props=0x20;
        if(fault==4){bad.ext_disc.props=0x10;bad.ext_disc.legacy_event_type=0;}
        ++now_ms;assert(!gap_event(&bad,(void*)(uintptr_t)scan_callback_epoch));
        cerberus_dual_ble_snapshot_metadata_row_t same=snapshot_metadata_row(binding,painted_handle,0);
        assert(!memcmp(&same,&painted,sizeof(same)));
    }
    status=snapshot_begin(binding,++request);
    cerberus_dual_ble_snapshot_metadata_row_t unknown=snapshot_metadata_row(binding,status.handle,0);
    cerberus_dual_ble_receive_metadata_t zero={0};assert(!memcmp(&unknown.receive,&zero,sizeof(zero)));
    /* Partial/controller-truncated, responses and stale callbacks cannot
     * replace the raw record or turn its metadata into a newer observation. */
    cerberus_ble_record_t saved=scan_results[0];
    for(unsigned fault=0;fault<5;++fault){
        struct ble_gap_event excluded=event;void *token=(void*)(uintptr_t)scan_callback_epoch;
        if(fault<2)excluded.ext_disc.data_status=(uint8_t)(fault+1U);
        if(fault==2)excluded.ext_disc.props=BLE_HCI_ADV_SCAN_RSP_MASK;
        if(fault==3)token=(void*)(uintptr_t)(scan_callback_epoch-1U);
        if(fault==4){excluded.ext_disc.props=0x18;excluded.ext_disc.legacy_event_type=4;}
        ++now_ms;assert(!gap_event(&excluded,token));assert(!memcmp(&saved,&scan_results[0],sizeof(saved)));
    }
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&binding,sizeof(binding),&owner,sizeof(owner));
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&binding,sizeof(binding),&owner,sizeof(owner));
    puts("C5 actual legacy/EXT callback metadata: SDK widths, full AD vs cut preview, legacy-PDU EXT, unknown normalization, exclusions, immutable102/110 wire passed");
}

static void snapshot_checks(void)
{
    memset(&dual_snapshot,0,sizeof(dual_snapshot));dual_snapshot.counter=dual_boot_id;
    cerberus_dual_ble_start_t start={.boot_id=dual_boot_id,.job_id=dual_job_highwater+1U,.version=2};
    cerberus_dual_ble_status_t owner;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,sizeof(start),&owner,sizeof(owner));
    assert(!owner.error_code&&owner.version==2&&(owner.flags&CERBERUS_DUAL_BLE_SNAPSHOTS));
    cerberus_dual_ble_binding_t b={dual_boot_id,start.job_id},wrong=b;
    unsigned start_before=starts,stop_before=stops;
    uint8_t raw[]={2,1,6};
    struct ble_gap_disc_desc p={.addr={{10,2,3,4,5,6},1},.rssi=-30,.event_type=0,.data=raw,.length_data=3};
    now_ms=1000;record_advertisement(&p,scan_callback_epoch,990000);
    memcpy(scan_results[0].name,"Peer",4);scan_results[0].name_length=4;
    scan_results[0].uuid_length=2;scan_results[0].uuid[0]=0xaa;scan_results[0].uuid[1]=0xfe;
    scan_results[0].manufacturer_id=0x4c;
    snapshot_busy=true;
    assert(snapshot_begin(b,1).error_code==ESP_ERR_TIMEOUT&&!dual_snapshot.valid);
    snapshot_busy=false;
    cerberus_dual_ble_snapshot_status_t first=snapshot_begin(b,1);
    assert(!first.error_code&&first.boot_id==b.boot_id&&first.handle==b.boot_id+1&&first.count==1);
    assert(first.flags==(CERBERUS_DUAL_BLE_SNAPSHOT_RUNNING|CERBERUS_DUAL_BLE_SNAPSHOT_EXTENDED));
    cerberus_dual_ble_snapshot_row_t frozen=snapshot_row(b,first.handle,0);
    assert(frozen.latest_rssi==-30&&frozen.age_ms==10&&frozen.timing_flags==3&&frozen.reports==1);
    assert(frozen.name_length==4&&!memcmp(frozen.name,"Peer",4)&&frozen.name[4]==0);
    assert(frozen.raw_length==3&&!memcmp(frozen.raw,raw,3)&&frozen.raw[3]==0);
    /* Real held-mutex admission also leaves the current handle unchanged. */
    held=1;cerberus_dual_ble_snapshot_begin_t contended={.binding=b,.request_id=2};
    cerberus_dual_ble_snapshot_status_t blocked;size_t blocked_size=0;
    assert(cerberus_ble_dual_command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN,
        &contended,sizeof(contended),&blocked,&blocked_size));
    assert(blocked_size==22&&blocked.error_code==ESP_ERR_TIMEOUT&&held==1&&dual_snapshot.handle==first.handle);held=0;
    cerberus_dual_ble_snapshot_status_t malformed;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN,&contended,sizeof(contended)-1,&malformed,22);
    assert(malformed.error_code==ESP_ERR_INVALID_ARG&&dual_snapshot.handle==first.handle);
    /* A callback can update the live row between ANY read chunks; every frozen
       field, including its first BEGIN age, must remain the original version. */
    raw[2]=7;p.rssi=-90;now_ms=1500;record_advertisement(&p,scan_callback_epoch,1490000);
    cerberus_dual_ble_snapshot_status_t retry=snapshot_begin(b,1);
    assert(!memcmp(&first,&retry,sizeof(first)));
    cerberus_dual_ble_snapshot_row_t reread=snapshot_row(b,first.handle,0);
    assert(!memcmp(&frozen,&reread,sizeof(frozen)));
    assert(snapshot_begin(b,3).error_code==ESP_ERR_INVALID_STATE);
    assert(snapshot_begin(b,0).error_code==ESP_ERR_INVALID_STATE);
    wrong.boot_id++;assert(snapshot_begin(wrong,2).error_code==ESP_ERR_NOT_FOUND);
    wrong=b;wrong.job_id++;assert(snapshot_status(wrong,first.handle).error_code==ESP_ERR_NOT_FOUND);
    assert(snapshot_chunk(wrong,first.handle,0,0).error==CERBERUS_DUAL_BLE_SNAPSHOT_STALE);
    assert(snapshot_chunk(b,first.handle,1,0).error==CERBERUS_DUAL_BLE_SNAPSHOT_RANGE);
    assert(snapshot_chunk(b,first.handle,0,11).error==CERBERUS_DUAL_BLE_SNAPSHOT_RANGE);
    snapshot_busy=true;
    assert(snapshot_status(b,first.handle).error_code==ESP_ERR_TIMEOUT);
    assert(snapshot_chunk(b,first.handle,0,0).error==CERBERUS_DUAL_BLE_SNAPSHOT_BUSY);
    snapshot_busy=false;
    uint64_t epoch=dual_epoch;dual_epoch++;
    assert(snapshot_status(b,first.handle).error_code==ESP_ERR_NOT_FOUND);dual_epoch=epoch;
    cerberus_dual_ble_snapshot_status_t second=snapshot_begin(b,2);
    assert(!second.error_code&&second.handle==first.handle+1);
    assert(snapshot_status(b,first.handle).error_code==ESP_ERR_NOT_FOUND);
    assert(snapshot_begin(b,1).error_code==ESP_ERR_INVALID_STATE);
    reread=snapshot_row(b,second.handle,0);assert(reread.latest_rssi==-90&&reread.reports==2&&reread.raw[2]==7);
    /* Peer aggregate lengths are checked before replacing a valid bank. */
    scan_results[0].name_length=33;assert(snapshot_begin(b,3).error_code==ESP_ERR_INVALID_RESPONSE);
    assert(snapshot_status(b,second.handle).error_code==ESP_OK);scan_results[0].name_length=4;
    scan_status.stored_count=33;assert(snapshot_begin(b,3).error_code==ESP_ERR_INVALID_RESPONSE);scan_status.stored_count=1;
    /* Wrap-safe callback age, unknown time/RSSI, and all32 retained rows. */
    scan_status.stored_count=32;for(unsigned i=1;i<32;++i)scan_results[i]=scan_results[0];
    now_ms=(uint64_t)UINT32_MAX+6U;scan_results[0].original_observed_ms=UINT32_MAX-4U;
    scan_results[1].original_time_valid=false;scan_results[1].latest_rssi=127;
    scan_results[2].advertisement_flags=CERBERUS_BLE_ADVERTISEMENT_MALFORMED|CERBERUS_BLE_ADVERTISEMENT_TRUNCATED;
    cerberus_dual_ble_snapshot_status_t third=snapshot_begin(b,3);assert(!third.error_code&&third.count==32);
    reread=snapshot_row(b,third.handle,0);assert(reread.age_ms==10);
    reread=snapshot_row(b,third.handle,1);assert(reread.age_ms==UINT32_MAX&&!reread.timing_flags);
    reread=snapshot_row(b,third.handle,2);assert(reread.raw_flags==3);
    reread=snapshot_row(b,third.handle,31);assert(reread.name_length==4);
    uint64_t saved_counter=dual_snapshot.counter;dual_snapshot.counter=UINT64_MAX;
    assert(snapshot_begin(b,4).error_code==ESP_ERR_INVALID_STATE);
    assert(!snapshot_begin(b,3).error_code);dual_snapshot.counter=saved_counter;
    uint64_t saved_request=dual_snapshot.request_id;dual_snapshot.request_id=UINT64_MAX;
    assert(!snapshot_begin(b,UINT64_MAX).error_code);
    assert(snapshot_begin(b,1).error_code==ESP_ERR_INVALID_STATE);dual_snapshot.request_id=saved_request;
    assert(starts==start_before&&stops==stop_before); /* Snapshot never changes RF. */
    /* A failed Stop retains the exact RF owner and cannot authorize a foreign
       release. The frozen bank remains private, while reads report its error. */
    cancel_fails=true;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&b,sizeof(b),&owner,sizeof(owner));
    assert(owner.error_code&&(owner.flags&CERBERUS_DUAL_BLE_OWNED)&&dual_attached&&dual_snapshot.valid);
    assert(snapshot_status(b,third.handle).error_code==ESP_FAIL);
    cancel_fails=false;dual_error=0;
    /* A completion is a new immutable snapshot; old handle stays RUNNING until
       replaced and never silently turns into a final bank. */
    dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_COMPLETE;
    assert(snapshot_status(b,third.handle).flags&CERBERUS_DUAL_BLE_SNAPSHOT_RUNNING);
    cerberus_dual_ble_snapshot_status_t final=snapshot_begin(b,4);
    assert(!final.error_code&&(final.flags&CERBERUS_DUAL_BLE_SNAPSHOT_COMPLETE));
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&wrong,sizeof(wrong),&owner,sizeof(owner));
    assert(snapshot_status(b,final.handle).error_code==ESP_OK);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&b,sizeof(b),&owner,sizeof(owner));
    assert(snapshot_status(b,final.handle).error_code==ESP_ERR_NOT_FOUND);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&b,sizeof(b),&owner,sizeof(owner));
    assert(!dual_attached&&!dual_snapshot.valid);
    /* A subsequent job cannot adopt old handles; completed orphan expiry
       discards a valid snapshot without affecting any active RF ownership. */
    start.job_id=dual_job_highwater+1U;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,sizeof(start),&owner,sizeof(owner));
    b.job_id=start.job_id;scan_status.stored_count=1;
    cerberus_dual_ble_snapshot_status_t orphan=snapshot_begin(b,1);assert(!orphan.error_code);
    assert(snapshot_status(b,final.handle).error_code==ESP_ERR_NOT_FOUND);
    now_ms+=CERBERUS_DUAL_BLE_ORPHAN_MS+1U;
    cerberus_dual_ble_info_t info;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);
    assert(dual_attached&&dual_radio_active&&dual_snapshot.valid);
    dual_radio_active=false;scan_status.state=CERBERUS_BLE_SCAN_COMPLETE;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);
    assert(!dual_attached&&!dual_snapshot.valid);
    assert(snapshot_status(b,orphan.handle).error_code==ESP_ERR_NOT_FOUND);
    printf("C5 snapshot memory: %zu frozen rows + %zu metadata = %zu bytes; %u try-locks.\n",
        sizeof(dual_snapshot.rows),sizeof(dual_snapshot)-sizeof(dual_snapshot.rows),sizeof(dual_snapshot),try_locks);
}
static void dense_inventory_checks(void)
{
    const uint8_t flags[]={2,1,6}, bad_width[]={2,3,0x44}, bad_suffix[]={2,1,6,0,1};
    const uint8_t duplicate_name[]={2,9,'A',2,9,'B'}, short_mfg[]={2,0xff,0x4c};
    assert(cerberus_ble_inventory_ad_valid(NULL,0));
    assert(cerberus_ble_inventory_ad_valid(flags,sizeof(flags)));
    assert(!cerberus_ble_inventory_ad_valid(NULL,1));
    assert(!cerberus_ble_inventory_ad_valid(flags,2));
    assert(!cerberus_ble_inventory_ad_valid(bad_width,sizeof(bad_width)));
    assert(!cerberus_ble_inventory_ad_valid(bad_suffix,sizeof(bad_suffix)));
    assert(!cerberus_ble_inventory_ad_valid(duplicate_name,sizeof(duplicate_name)));
    assert(!cerberus_ble_inventory_ad_valid(short_mfg,sizeof(short_mfg)));
    scan_status.state=CERBERUS_BLE_SCAN_IDLE;dual_attached=dual_radio_active=false;dual_error=0;now_ms=5000;
    cerberus_dual_ble_start_t start={.boot_id=dual_boot_id,.job_id=dual_job_highwater+1U,.version=2};
    cerberus_dual_ble_status_t owner;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,sizeof(start),&owner,sizeof(owner));
    assert(!owner.error_code&&scan_inventory.enabled);
    cerberus_dual_ble_binding_t binding={dual_boot_id,start.job_id};
    struct ble_gap_disc_desc p={.addr={{1,2,3,4,5,6},0},.rssi=-30,.event_type=0,
        .data=flags,.length_data=sizeof(flags)};
    int64_t stamp=esp_timer_get_time();
    for(unsigned i=0;i<32;++i){p.addr.val[0]=(uint8_t)(i+1);record_advertisement(&p,scan_callback_epoch,++stamp);}
    assert(scan_status.stored_count==32&&!scan_inventory.replaced&&!scan_inventory.dropped);
    memcpy(scan_results[1].name,"OLD",3);scan_results[1].name_length=3;
    scan_results[1].uuid_length=16;memset(scan_results[1].uuid,0xa5,16);scan_results[1].manufacturer_id=0x1234;
    scan_results[1].reports_seen=99;
    cerberus_dual_ble_snapshot_status_t first=snapshot_begin(binding,1);
    assert(!first.error_code&&first.count==32);
    cerberus_dual_ble_snapshot_row_t original=snapshot_row(binding,first.handle,1);
    /* A valid recent original protects slot0; malformed repeated slot1 does
     * not become more recent for replacement merely through repeated traffic. */
    p.addr.val[0]=1;record_advertisement(&p,scan_callback_epoch,++stamp);
    p.addr.val[0]=2;p.data=bad_suffix;p.length_data=sizeof(bad_suffix);
    record_advertisement(&p,scan_callback_epoch,++stamp);
    assert(scan_inventory.rank[1]<scan_inventory.rank[0]);
    p.addr.val[0]=99;p.data=flags;p.length_data=sizeof(flags);p.event_type=3;p.rssi=-100;
    record_advertisement(&p,scan_callback_epoch,++stamp);
    assert(scan_inventory.replaced==1&&!scan_inventory.dropped&&scan_status.stored_count==32);
    assert(scan_results[0].address[0]==1&&scan_results[1].address[0]==99);
    assert(!scan_results[1].name_length&&!scan_results[1].uuid_length&&!scan_results[1].flags);
    assert(scan_results[1].manufacturer_id==CERBERUS_BLE_MANUFACTURER_UNKNOWN);
    assert(scan_results[1].reports_seen==1&&scan_results[1].rssi==-100&&scan_results[1].latest_rssi==-100);
    cerberus_dual_ble_snapshot_row_t retained=snapshot_row(binding,first.handle,1);
    assert(!memcmp(&retained,&original,sizeof(original)));
    cerberus_dual_ble_snapshot_inventory_t stats=snapshot_inventory(binding,first.handle);
    assert(!stats.error_code&&stats.handle==first.handle&&stats.count==32&&stats.flags==1&&!stats.replaced&&!stats.dropped);
    cerberus_ble_record_t before[32];memcpy(before,scan_results,sizeof(before));
    p.addr.val[0]=100;p.data=bad_width;p.length_data=sizeof(bad_width);
    record_advertisement(&p,scan_callback_epoch,++stamp);
    assert(scan_inventory.dropped==1);
    p.data=flags;p.length_data=sizeof(flags);
    record_advertisement(&p,scan_callback_epoch,-1);
    record_advertisement(&p,scan_callback_epoch,stamp-1);
    assert(scan_inventory.dropped==3);
    p.addr.type=4;record_advertisement(&p,scan_callback_epoch,++stamp);p.addr.type=0;
    p.event_type=4;record_advertisement(&p,scan_callback_epoch,++stamp);p.event_type=3;
    record_advertisement(&p,scan_callback_epoch-1,++stamp);
    assert(scan_inventory.dropped==3&&scan_inventory.replaced==1&&!memcmp(before,scan_results,sizeof(before)));
    cerberus_dual_ble_snapshot_status_t second=snapshot_begin(binding,2);
    stats=snapshot_inventory(binding,second.handle);
    assert(!stats.error_code&&stats.replaced==1&&stats.dropped==3&&stats.count==32);
    assert(snapshot_inventory(binding,first.handle).error_code==ESP_ERR_NOT_FOUND);
    cerberus_dual_ble_binding_t foreign=binding;foreign.boot_id++;
    assert(snapshot_inventory(foreign,second.handle).error_code==ESP_ERR_NOT_FOUND);
    foreign=binding;foreign.job_id++;
    assert(snapshot_inventory(foreign,second.handle).error_code==ESP_ERR_NOT_FOUND);
    uint64_t epoch=dual_epoch;dual_epoch++;
    assert(snapshot_inventory(binding,second.handle).error_code==ESP_ERR_NOT_FOUND);dual_epoch=epoch;
    snapshot_busy=true;stats=snapshot_inventory(binding,second.handle);snapshot_busy=false;
    assert(stats.error_code==ESP_ERR_TIMEOUT&&!stats.replaced&&!stats.dropped&&!stats.count&&!stats.flags);
    /* Entire complete extended container validates independently of its31-byte
     * preview. The prefix remains explicitly truncated, never reassembled. */
    uint8_t extended[255]={254,0xff,0x4c,0};
    assert(cerberus_ble_inventory_ad_valid(extended,sizeof(extended)));
    assert(!cerberus_ble_inventory_ad_valid(extended,sizeof(extended)+1));
    p.data=extended;p.length_data=255;record_advertisement(&p,scan_callback_epoch,++stamp);
    assert(scan_inventory.replaced==2&&scan_results[2].address[0]==100&&scan_results[2].advertisement_length==31);
    assert(scan_results[2].advertisement_flags&CERBERUS_BLE_ADVERTISEMENT_TRUNCATED);
    scan_inventory.sequence=UINT32_MAX;p.addr.val[0]=101;p.data=flags;p.length_data=sizeof(flags);
    record_advertisement(&p,scan_callback_epoch,++stamp);assert(scan_inventory.replaced==2&&scan_inventory.dropped==4);
    scan_inventory.sequence=1000;scan_inventory.replaced=UINT32_MAX;
    record_advertisement(&p,scan_callback_epoch,++stamp);assert(scan_inventory.replaced==UINT32_MAX);
    scan_inventory.dropped=UINT32_MAX;p.addr.val[0]=102;p.data=bad_width;p.length_data=sizeof(bad_width);
    record_advertisement(&p,scan_callback_epoch,++stamp);assert(scan_inventory.dropped==UINT32_MAX);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&binding,sizeof(binding),&owner,sizeof(owner));
    assert(snapshot_inventory(binding,second.handle).error_code==ESP_ERR_NOT_FOUND);
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&binding,sizeof(binding),&owner,sizeof(owner));
    /* Legacy v1 discovery and v2 exact-address tracking keep stable first32
     * admission. A different typed address is different even with same bytes. */
    for(unsigned mode=0;mode<2;++mode){
        start.job_id=dual_job_highwater+1U;start.version=mode?2:1;start.mode=(uint8_t)mode;
        start.address[0]=mode?1:0;start.address_type=0;
        command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,sizeof(start),&owner,sizeof(owner));
        assert(!owner.error_code&&!scan_inventory.enabled&&!scan_inventory.replaced&&!scan_inventory.dropped);
        p.data=flags;p.length_data=sizeof(flags);p.event_type=0;p.addr.type=0;
        for(unsigned i=0;i<32;++i){p.addr.val[0]=(uint8_t)(i+1);record_advertisement(&p,scan_callback_epoch,++stamp);}
        p.addr.val[0]=1;p.addr.type=1;record_advertisement(&p,scan_callback_epoch,++stamp);
        assert(scan_status.stored_count==32&&!scan_inventory.replaced&&scan_inventory.dropped==1&&scan_results[0].address_type==0);
        binding.job_id=start.job_id;
        command(CERBERUS_SPI_COMMAND_DUAL_BLE_STOP,&binding,sizeof(binding),&owner,sizeof(owner));
        command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&binding,sizeof(binding),&owner,sizeof(owner));
    }
    printf("C5 dense inventory: strict complete admission, LRU ranks, immutable snapshots/stats, typed/drop/clock/saturation/legacy checks; state=%zu bytes\n",sizeof(scan_inventory));
}
int main(void)
{
    dual_boot_id=123;host_synced=true;now_ms=100;
    cerberus_dual_ble_info_t info;command(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,&info,22);
    assert(info.boot_id==123 && info.next_job_id==1 && (info.flags&CERBERUS_DUAL_BLE_READY));
    cerberus_dual_ble_start_t start={.boot_id=123,.job_id=1,.mode=1,.address={1,2,3,4,5,6},.address_type=1,.version=1};
    cerberus_dual_ble_status_t status;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&start,22,&status,22);
    assert(!status.error_code && status.epoch==1 && dual_attached && dual_radio_active && starts==1);
    cerberus_dual_ble_start_t changed_version=start;changed_version.version=2;
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_START,&changed_version,22,&status,22);assert(status.error_code&&dual_version==1);
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
    command(CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE,&b,12,&status,22);
    assert(status.error_code==ESP_ERR_NOT_FOUND&&status.version==1); /* Lost v1 RELEASE resolves. */
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
    snapshot_checks();
    dense_inventory_checks();
    callback_metadata_checks();
    puts("dual BLE C5 owner/callback: exact typed tracker, latest RSSI, capacity, idempotency, foreign ownership, orphan checks passed");
    return 0;
}
