#include "dual_ble_scan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t now, boot, epoch;
    uint32_t job;
    unsigned starts, stops, releases, frames, track_reads;
    uint8_t count, generation, state, mode;
    bool attached, owned, lose_start, fail_stop, corrupt_fence, corrupt_data;
    bool unsupported, reset, busy;
} peer_t;
static uint64_t clock_ms(void *ctx) { return ((peer_t *)ctx)->now; }
static cerberus_dual_ble_status_t status(peer_t *p)
{
    return (cerberus_dual_ble_status_t){ .epoch=p->epoch, .job_id=p->job,
        .state=p->state, .generation=p->generation, .count=p->count,
        .flags=(p->attached ? CERBERUS_DUAL_BLE_ATTACHED : 0) |
            (p->owned ? CERBERUS_DUAL_BLE_OWNED : 0), .version=1 };
}
static esp_err_t exchange(void *ctx, uint8_t cmd, const void *request,
    size_t request_size, void *reply, size_t reply_size)
{
    peer_t *p=ctx; ++p->frames; ++p->now;
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_INFO) {
        assert(!request_size && reply_size==sizeof(cerberus_dual_ble_info_t));
        if (p->unsupported) return ESP_ERR_NOT_SUPPORTED;
        cerberus_dual_ble_info_t r={.boot_id=p->boot,.next_job_id=p->starts+1U,.version=1,.capacity=32,.raw_bytes=31,
            .flags=CERBERUS_DUAL_BLE_READY | CERBERUS_DUAL_BLE_EXTENDED |
                (p->attached || p->busy ? CERBERUS_DUAL_BLE_ATTACHED : 0)};
        memcpy(reply,&r,sizeof(r)); return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_START) {
        assert(request_size==22 && reply_size==22);
        const cerberus_dual_ble_start_t *r=request;
        assert(cerberus_dual_ble_start_valid(r)); assert(r->boot_id==p->boot);
        if (!p->attached) { ++p->starts; ++p->epoch; ++p->generation;
            p->attached=true; p->owned=true; p->state=CERBERUS_BLE_SCAN_RUNNING;
            p->job=r->job_id; p->mode=r->mode; }
        assert(p->job==r->job_id);
        if (p->lose_start) return ESP_ERR_TIMEOUT;
        cerberus_dual_ble_status_t result=status(p); memcpy(reply,&result,sizeof(result)); return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_BLE_SCAN_RESULT) {
        const cerberus_ble_scan_result_request_t *r=request;
        assert(r->generation==p->generation && r->index<p->count && !p->owned);
        cerberus_ble_scan_result_payload_t result={.generation=r->generation,.index=r->index,
            .address={1,2,3,4,5,6}, .address_type=r->index, .name_length=4,
            .advertisement_length=3,.advertisement_event_type=0,.reports_seen=7,.rssi=-20};
        memcpy(reply,&result,sizeof(result)); return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_BLE_SCAN_DATA) {
        const cerberus_ble_scan_data_request_t *r=request;
        cerberus_ble_scan_data_payload_t result={.generation=r->generation,.index=r->index,
            .kind=r->kind,.chunk=r->chunk};
        if (r->kind==CERBERUS_BLE_DATA_NAME) { result.byte_count=4; memcpy(result.bytes,"TEST",4); }
        else { assert(r->kind==CERBERUS_BLE_DATA_RAW_ADVERTISEMENT); result.byte_count=3;
            result.bytes[0]=2;result.bytes[1]=1;result.bytes[2]=6; }
        if (p->corrupt_data) ++result.generation;
        memcpy(reply,&result,sizeof(result)); return ESP_OK;
    }
    const cerberus_dual_ble_binding_t *b=request;
    assert(request_size>=sizeof(*b));
    if (p->reset || b->boot_id!=p->boot || b->job_id!=p->job || !p->attached) {
        cerberus_dual_ble_status_t absent={.job_id=b->job_id,.state=CERBERUS_BLE_SCAN_ERROR,
            .error_code=ESP_ERR_NOT_FOUND,.version=1};
        memcpy(reply,&absent,sizeof(absent)); p->attached=false; p->owned=false; return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_ROW_TIME) {
        const cerberus_dual_ble_row_request_t *r=request;
        cerberus_dual_ble_row_time_t stamp={.epoch=p->epoch,.job_id=p->job,.age_ms=7000,
            .index=r->index,.generation=p->generation,.latest_rssi=-80,.flags=3,.reports=7};
        memcpy(reply,&stamp,sizeof(stamp)); return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_TRACK_STATUS) {
        ++p->track_reads;
        cerberus_dual_ble_track_t result={.epoch=p->epoch,.job_id=p->job,.age_ms=40,
            .reports=p->track_reads,.rssi=-75,.flags=3};
        memcpy(reply,&result,sizeof(result)); return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_STOP) {
        ++p->stops;
        if (p->fail_stop) return ESP_ERR_TIMEOUT;
        p->owned=false; p->state=CERBERUS_BLE_SCAN_IDLE;
    } else if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE) {
        ++p->releases; assert(!p->owned); p->attached=false;p->state=CERBERUS_BLE_SCAN_IDLE;
    } else assert(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_STATUS);
    cerberus_dual_ble_status_t r=status(p);
    if (p->corrupt_fence && !p->owned && cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_STATUS) ++r.epoch;
    memcpy(reply,&r,sizeof(r)); return ESP_OK;
}
static void init(cerberus_dual_ble_scan_t *s, peer_t *p)
{
    memset(p,0,sizeof(*p));p->boot=123;p->now=100;p->count=2;
    cerberus_dual_ble_ops_t ops={p,exchange,clock_ms};cerberus_dual_ble_init(s,&ops);
}
static void tick(cerberus_dual_ble_scan_t *s, peer_t *p)
{ unsigned before=p->frames;cerberus_dual_ble_tick(s,p->now);assert(p->frames-before<=4);p->now+=100; }
static void start(cerberus_dual_ble_scan_t *s, peer_t *p)
{ assert(cerberus_dual_ble_begin(s,0,NULL,0)==ESP_OK);tick(s,p);assert(p->starts==1); }
static void completed(peer_t *p)
{ p->owned=false;p->state=CERBERUS_BLE_SCAN_COMPLETE; }
static void drain(cerberus_dual_ble_scan_t *s, peer_t *p)
{
    cerberus_dual_ble_stop(s);
    for (unsigned i=0;i<40 && !cerberus_dual_ble_released(s);++i)tick(s,p);
    assert(cerberus_dual_ble_released(s));assert(!p->owned && !p->attached);
}
int main(void)
{
    cerberus_dual_ble_scan_t s;peer_t p;
    init(&s,&p);start(&s,&p);completed(&p);
    for (unsigned i=0;i<50 && !s.sweeps;++i)tick(&s,&p);
    assert(s.sweeps==1 && s.count==2 && s.rows[0].address_type==0 && s.rows[1].address_type==1);
    assert(!memcmp(s.rows[0].address,s.rows[1].address,6)); /* Same bytes, distinct typed identities. */
    assert(s.rows[0].rssi==-80 && !strcmp(s.rows[0].name,"TEST")); /* Latest callback, not -20 peak. */
    assert(s.rows[0].raw_length==3 && s.rows[0].raw[2]==6 && s.rows[0].reports==7);
    assert(cerberus_dual_ble_age_upper(s.rows[0].callback_age_ms,s.rows[0].request_started_ms,p.now)>=7000);
    drain(&s,&p);
    cerberus_dual_ble_row_t saved=s.rows[0];
    assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);tick(&s,&p);completed(&p);p.corrupt_data=true;
    for(unsigned i=0;i<40 && !s.error;++i)tick(&s,&p);
    assert(s.error==ESP_ERR_INVALID_RESPONSE && s.count==2 && !memcmp(&saved,&s.rows[0],sizeof(saved)));
    drain(&s,&p);

    init(&s,&p);start(&s,&p);completed(&p);p.corrupt_fence=true;
    for(unsigned i=0;i<20 && !s.error;++i)tick(&s,&p);
    assert(s.error==ESP_ERR_INVALID_RESPONSE && !s.count);p.corrupt_fence=false;drain(&s,&p);

    init(&s,&p);p.lose_start=true;p.fail_stop=true;assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);
    for(unsigned i=0;i<30 && !s.error;++i)tick(&s,&p);
    assert(s.error==ESP_ERR_TIMEOUT && p.starts==1 && !s.count);
    p.fail_stop=true;for(unsigned i=0;i<8;++i)tick(&s,&p);
    assert(!cerberus_dual_ble_released(&s) && p.owned && !p.releases);
    p.fail_stop=false;drain(&s,&p); /* Lost START replies still authorize only bound cleanup. */

    init(&s,&p);start(&s,&p);p.reset=true;
    for(unsigned i=0;i<8;++i)tick(&s,&p);
    assert(cerberus_dual_ble_released(&s) && s.error && !s.enabled);
    init(&s,&p);p.busy=true;assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);tick(&s,&p);
    assert(s.error==ESP_ERR_INVALID_STATE && !p.starts && !p.stops);
    init(&s,&p);p.unsupported=true;assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);tick(&s,&p);
    assert(s.error==ESP_ERR_NOT_SUPPORTED && !p.starts && !p.stops);

    init(&s,&p);p.count=0;start(&s,&p);completed(&p);
    for(unsigned i=0;i<20 && !s.sweeps;++i)tick(&s,&p);
    assert(s.sweeps==1 && !s.count);drain(&s,&p);

    const uint8_t target[6]={1,2,3,4,5,6};
    init(&s,&p);assert(cerberus_dual_ble_begin(&s,1,target,3)==ESP_OK);
    for(unsigned i=0;i<8 && !s.has_track;++i)tick(&s,&p);
    cerberus_dual_ble_track_view_t v;cerberus_dual_ble_track_view(&s,p.now,&v);
    assert(v.seen && v.fresh && v.rssi==-75 && v.age_upper_ms>=40);
    cerberus_dual_ble_track_view(&s,p.now+5001,&v);assert(v.seen && !v.fresh);
    cerberus_dual_ble_track_view(&s,s.track_request_ms-1,&v);assert(v.age_upper_ms==UINT32_MAX && !v.fresh);
    drain(&s,&p);
    assert(cerberus_dual_ble_age_upper(UINT32_MAX-1,10,12)==UINT32_MAX);
    assert(cerberus_dual_ble_age_upper(2,10,9)==UINT32_MAX);
    assert(cerberus_dual_ble_age_upper(2,10,12)==4);
    printf("dual BLE reader: owner/replay/fence/lost-reply/age/typed-row checks passed; store=%zu bytes\n",sizeof(s));
    return 0;
}
