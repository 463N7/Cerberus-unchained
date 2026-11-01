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
    uint8_t version;
    bool snapshot_capability;
    uint8_t status_add_flags, status_remove_flags;
    unsigned cost_ms, snapshot_begins, snapshot_reads, snapshot_fences, lose_begin_replies;
    uint64_t request_id, snapshot_clock_ms, last_begin_ms, handle_counter, last_fence_ms;
    unsigned complete_fences;
    cerberus_dual_ble_snapshot_status_t snapshot;
    cerberus_dual_ble_snapshot_row_t frozen[CERBERUS_DUAL_BLE_CAPACITY];
    unsigned fault, busy_begin, busy_read;
    bool unknown_age;
} peer_t;
enum {
    BAD_BEGIN_BOOT=1, BAD_BEGIN_ZERO_HANDLE, BAD_BEGIN_COUNT, BAD_BEGIN_FLAGS,
    BAD_BEGIN_UNKNOWN_FLAG, BAD_BEGIN_EXTENDED,
    BAD_READ_HANDLE, BAD_READ_INDEX, BAD_READ_CHUNK, BAD_READ_LENGTH,
    BAD_READ_ERROR, BAD_READ_ERROR_BODY, BAD_READ_TAIL,
    BAD_ROW_TYPE, BAD_ROW_NAME_LENGTH, BAD_ROW_UUID_LENGTH, BAD_ROW_RAW_LENGTH,
    BAD_ROW_EVENT, BAD_ROW_PEER_FLAGS, BAD_ROW_RAW_FLAGS, BAD_ROW_TIME_FLAGS,
    BAD_ROW_RSSI, BAD_ROW_NAME_PADDING, BAD_ROW_UUID_PADDING, BAD_ROW_RAW_PADDING,
    BAD_ROW_DUPLICATE, BAD_FENCE_HANDLE, BAD_FENCE_BOOT, BAD_FENCE_COUNT, BAD_FENCE_FLAGS,
    FAULT_END
};
static uint64_t clock_ms(void *ctx) { return ((peer_t *)ctx)->now; }
static cerberus_dual_ble_status_t status(peer_t *p)
{
    return (cerberus_dual_ble_status_t){ .epoch=p->epoch, .job_id=p->job,
        .state=p->state, .generation=p->generation, .count=p->count,
        .flags=((p->attached ? CERBERUS_DUAL_BLE_ATTACHED : 0) |
            (p->owned ? CERBERUS_DUAL_BLE_OWNED : 0) |
            (p->attached ? CERBERUS_DUAL_BLE_EXTENDED : 0) |
            (p->attached && p->snapshot_capability ? CERBERUS_DUAL_BLE_SNAPSHOTS : 0) |
            p->status_add_flags) & (uint8_t)~p->status_remove_flags, .version=p->version };
}
static void snapshot_freeze(peer_t *p)
{
    p->snapshot=(cerberus_dual_ble_snapshot_status_t){.boot_id=p->boot,.handle=++p->handle_counter,
        .count=p->count,.flags=(p->owned?CERBERUS_DUAL_BLE_SNAPSHOT_RUNNING:CERBERUS_DUAL_BLE_SNAPSHOT_COMPLETE)|
            CERBERUS_DUAL_BLE_SNAPSHOT_EXTENDED};
    p->snapshot_clock_ms=p->now;
    memset(p->frozen,0,sizeof(p->frozen));
    for (uint8_t i=0;i<p->count;++i) {
        cerberus_dual_ble_snapshot_row_t *r=&p->frozen[i];
        memcpy(r->address,(uint8_t[]){1,2,3,4,5,(uint8_t)(i/4U)},6);
        r->address_type=i%4U;r->latest_rssi=-85;r->name_length=4;memcpy(r->name,"SNAP",4);
        r->uuid_length=2;r->uuid[0]=0x0d;r->uuid[1]=0x18;r->manufacturer_id=0x004c;
        r->peer_flags=CERBERUS_BLE_RESULT_CONNECTABLE;r->raw_length=3;memcpy(r->raw,(uint8_t[]){2,1,6},3);
        r->reports=100U+p->snapshot_begins;r->age_ms=p->unknown_age?UINT32_MAX:17;
        r->timing_flags=p->unknown_age?0:3;
    }
    cerberus_dual_ble_snapshot_row_t *r=&p->frozen[0];
    switch(p->fault) {
    case BAD_ROW_TYPE:r->address_type=4;break;
    case BAD_ROW_NAME_LENGTH:r->name_length=33;break;
    case BAD_ROW_UUID_LENGTH:r->uuid_length=3;break;
    case BAD_ROW_RAW_LENGTH:r->raw_length=32;break;
    case BAD_ROW_EVENT:r->event_type=5;break;
    case BAD_ROW_PEER_FLAGS:r->peer_flags=0x80;break;
    case BAD_ROW_RAW_FLAGS:r->raw_flags=0x80;break;
    case BAD_ROW_TIME_FLAGS:r->timing_flags=0x80;break;
    case BAD_ROW_RSSI:r->latest_rssi=21;break;
    case BAD_ROW_NAME_PADDING:r->name[31]=1;break;
    case BAD_ROW_UUID_PADDING:r->uuid[15]=1;break;
    case BAD_ROW_RAW_PADDING:r->raw[30]=1;break;
    case BAD_ROW_DUPLICATE:p->frozen[1]=*r;break;
    default:break;
    }
}
static esp_err_t snapshot_exchange(peer_t *p,uint8_t cmd,const void *request,
    size_t request_size,void *reply,size_t reply_size)
{
    assert(p->version==2 && p->snapshot_capability && reply_size==22);
    const cerberus_dual_ble_binding_t *b=request;
    assert(b->boot_id==p->boot && b->job_id==p->job && p->attached);
    if(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_READ) {
        assert(request_size==22);++p->snapshot_reads;
        const cerberus_dual_ble_snapshot_read_t *r=request;
        assert(r->handle==p->snapshot.handle&&r->index<p->snapshot.count&&r->chunk<11);
        cerberus_dual_ble_snapshot_chunk_t out={.handle=r->handle,.index=r->index,.chunk=r->chunk};
        if(p->busy_read) {--p->busy_read;out.error=CERBERUS_DUAL_BLE_SNAPSHOT_BUSY;}
        else {
            size_t offset=r->chunk*10U, amount=sizeof(p->frozen[0])-offset;
            if(amount>10U)amount=10U;out.byte_count=(uint8_t)amount;
            memcpy(out.data,(const uint8_t *)&p->frozen[r->index]+offset,amount);
            switch(p->fault) {
            case BAD_READ_HANDLE:--out.handle;break;
            case BAD_READ_INDEX:++out.index;break;
            case BAD_READ_CHUNK:++out.chunk;break;
            case BAD_READ_LENGTH:--out.byte_count;break;
            case BAD_READ_ERROR:out.error=5;break;
            case BAD_READ_ERROR_BODY:out.error=CERBERUS_DUAL_BLE_SNAPSHOT_BUSY;break;
            case BAD_READ_TAIL:if(r->chunk==10)out.data[9]=1;break;
            default:break;
            }
        }
        memcpy(reply,&out,sizeof(out));return ESP_OK;
    }
    cerberus_dual_ble_snapshot_status_t out;
    if(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN) {
        assert(request_size==20);++p->snapshot_begins;
        const cerberus_dual_ble_snapshot_begin_t *r=request;
        if(p->busy_begin) {
            --p->busy_begin;out=(cerberus_dual_ble_snapshot_status_t){.boot_id=p->boot,.error_code=ESP_ERR_TIMEOUT};
            memcpy(reply,&out,sizeof(out));return ESP_OK;
        }
        if(r->request_id!=p->request_id) {
            assert(r->request_id==p->request_id+1U);p->request_id=r->request_id;p->last_begin_ms=p->now;
            snapshot_freeze(p);
        }
        if(p->lose_begin_replies) {--p->lose_begin_replies;return ESP_ERR_TIMEOUT;}
        out=p->snapshot;
        switch(p->fault) {
        case BAD_BEGIN_BOOT:++out.boot_id;break;
        case BAD_BEGIN_ZERO_HANDLE:out.handle=0;break;
        case BAD_BEGIN_COUNT:out.count=33;break;
        case BAD_BEGIN_FLAGS:out.flags|=3;break;
        case BAD_BEGIN_UNKNOWN_FLAG:out.flags|=0x80;break;
        case BAD_BEGIN_EXTENDED:out.flags&=~CERBERUS_DUAL_BLE_SNAPSHOT_EXTENDED;break;
        default:break;
        }
    } else {
        assert(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_STATUS&&request_size==20);++p->snapshot_fences;
        const cerberus_dual_ble_snapshot_binding_t *r=request;assert(r->handle==p->snapshot.handle);
        out=p->snapshot;
        p->last_fence_ms=p->now;
        if(out.flags&CERBERUS_DUAL_BLE_SNAPSHOT_COMPLETE)++p->complete_fences;
        switch(p->fault) {
        case BAD_FENCE_HANDLE:++out.handle;break;
        case BAD_FENCE_BOOT:++out.boot_id;break;
        case BAD_FENCE_COUNT:++out.count;break;
        case BAD_FENCE_FLAGS:out.flags^=3;break;
        default:break;
        }
    }
    memcpy(reply,&out,sizeof(out));return ESP_OK;
}
static esp_err_t exchange(void *ctx, uint8_t cmd, const void *request,
    size_t request_size, void *reply, size_t reply_size)
{
    peer_t *p=ctx; ++p->frames; p->now+=p->cost_ms;
    if(cmd>=CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_BEGIN&&cmd<=CERBERUS_SPI_COMMAND_DUAL_BLE_SNAPSHOT_STATUS)
        return snapshot_exchange(p,cmd,request,request_size,reply,reply_size);
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_INFO) {
        assert(!request_size && reply_size==sizeof(cerberus_dual_ble_info_t));
        if (p->unsupported) return ESP_ERR_NOT_SUPPORTED;
        cerberus_dual_ble_info_t r={.boot_id=p->boot,.next_job_id=p->starts+1U,.version=p->version,.capacity=32,.raw_bytes=31,
            .flags=CERBERUS_DUAL_BLE_READY | CERBERUS_DUAL_BLE_EXTENDED |
                (p->snapshot_capability?CERBERUS_DUAL_BLE_SNAPSHOTS:0) |
                (p->attached || p->busy ? CERBERUS_DUAL_BLE_ATTACHED : 0)};
        memcpy(reply,&r,sizeof(r)); return ESP_OK;
    }
    if (cmd == CERBERUS_SPI_COMMAND_DUAL_BLE_START) {
        assert(request_size==22 && reply_size==22);
        const cerberus_dual_ble_start_t *r=request;
        assert(cerberus_dual_ble_start_valid(r)); assert(r->boot_id==p->boot);
        if (!p->attached) { ++p->starts; ++p->epoch; ++p->generation;
            p->attached=true; p->owned=true; p->state=CERBERUS_BLE_SCAN_RUNNING;
            p->job=r->job_id; p->mode=r->mode;p->request_id=0;memset(&p->snapshot,0,sizeof(p->snapshot)); }
        assert(r->version==p->version);
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
            .error_code=ESP_ERR_NOT_FOUND,.version=p->version};
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
    memset(p,0,sizeof(*p));p->boot=123;p->now=100;p->count=2;p->version=1;p->cost_ms=1;
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
static void init_live(cerberus_dual_ble_scan_t *s,peer_t *p)
{init(s,p);p->version=2;p->snapshot_capability=true;}
static unsigned fast_tick(cerberus_dual_ble_scan_t *s,peer_t *p)
{
    unsigned before=p->frames;cerberus_dual_ble_tick(s,p->now);
    unsigned used=p->frames-before;assert(used<=4);p->now+=20;return used;
}
static void await_snapshots(cerberus_dual_ble_scan_t *s,peer_t *p,unsigned count)
{
    for(unsigned i=0;i<2000&&s->snapshots<count&&!s->error;++i)fast_tick(s,p);
    assert(!s->error&&s->snapshots==count);
}
static void check_live_snapshots(void)
{
    cerberus_dual_ble_scan_t s;peer_t p;
    init_live(&s,&p);p.count=32;
    s.count=1;s.rows[0].rssi=-101; /* Retained previous discovery is not first results. */
    start(&s,&p);assert(!s.has_first_result&&!s.snapshots&&s.count==1&&s.live_updates);
    unsigned peak_calls=0;
    for(unsigned i=0;i<1000&&!s.snapshots;++i) {
        unsigned used=fast_tick(&s,&p);if(used>peak_calls)peak_calls=used;
        if(!s.snapshots)assert(s.count==1&&s.rows[0].rssi==-101&&!s.has_first_result);
    }
    assert(s.snapshots==1&&s.count==32&&s.sweeps==0&&p.owned&&p.attached&&peak_calls==4);
    assert(p.snapshot_reads==32U*11U&&p.snapshot_fences==1&&p.snapshot_begins==1);
    assert(s.has_first_result&&s.first_result_ms<8000&&s.first_result_ms>500);
    assert(s.first_result_ms==p.last_fence_ms-s.discovery_started_ms);
    assert(s.rows[0].rssi==-85&&!strcmp(s.rows[0].name,"SNAP")&&s.rows[0].raw[2]==6);
    assert(s.rows[0].reports==101&&s.rows[0].manufacturer_id==0x004c&&s.rows[0].uuid_length==2);
    for(unsigned i=0;i<32;++i) {
        assert(s.rows[i].request_started_ms==s.snapshot_request_started_ms);
        assert(cerberus_dual_ble_age_upper(s.rows[i].callback_age_ms,s.rows[i].request_started_ms,p.now)>=17);
    }
    uint64_t due=s.next_snapshot_ms;
    unsigned begins=p.snapshot_begins;
    while(p.now+4<due) {fast_tick(&s,&p);assert(p.snapshot_begins==begins);}
    await_snapshots(&s,&p,2);assert(p.last_begin_ms>=due&&!s.sweeps&&p.owned);
    uint32_t first=s.first_result_ms;uint64_t anchor=s.rows[0].request_started_ms;
    /* RF can finish halfway through a frozen RUNNING download. That snapshot
       stays coherent; a distinct COMPLETE snapshot is mandatory before release. */
    for(unsigned i=0;i<1000&&p.request_id<3;++i)fast_tick(&s,&p);
    assert(p.request_id==3);completed(&p);
    await_snapshots(&s,&p,3);assert(!s.sweeps&&s.first_result_ms==first&&s.rows[0].request_started_ms>anchor);
    for(unsigned i=0;i<1000&&!s.sweeps;++i)fast_tick(&s,&p);
    assert(s.sweeps==1&&s.snapshots==4&&p.complete_fences==1);
    assert(s.first_result_ms==first);drain(&s,&p);

    /* Lost BEGIN replies retain one frozen bank and FIRST request anchor. */
    init_live(&s,&p);p.lose_begin_replies=2;start(&s,&p);await_snapshots(&s,&p,1);
    assert(p.snapshot_begins==3&&p.handle_counter==1&&p.request_id==1);
    assert(s.snapshot_request_started_ms+1==p.snapshot_clock_ms);
    assert(s.rows[0].request_started_ms+200<=p.now);
    assert(s.rows[0].reports==101);drain(&s,&p);

    /* Busy operations end this tick and retry the same bound operation. */
    init_live(&s,&p);p.busy_begin=1;p.busy_read=1;start(&s,&p);
    await_snapshots(&s,&p,1);assert(p.snapshot_begins==2&&p.handle_counter==1);drain(&s,&p);
    init_live(&s,&p);start(&s,&p);p.cost_ms=9;
    for(unsigned i=0;i<300&&!s.snapshots;++i)assert(fast_tick(&s,&p)<=1);
    assert(s.snapshots==1);drain(&s,&p);

    /* Empty is a publication, not first results; new nonempty publication
       is measured from public begin even across an internal release/rearm. */
    init_live(&s,&p);p.count=0;start(&s,&p);await_snapshots(&s,&p,1);
    assert(!s.has_first_result&&!s.first_result_ms&&!s.count&&!s.sweeps);
    completed(&p);for(unsigned i=0;i<50&&!s.sweeps;++i)fast_tick(&s,&p);
    assert(s.sweeps==1&&s.snapshots==2&&!s.has_first_result);
    p.count=1;await_snapshots(&s,&p,3);assert(s.has_first_result&&s.first_result_ms>500&&s.sweeps==1);
    completed(&p);for(unsigned i=0;i<50&&s.sweeps<2;++i)fast_tick(&s,&p);
    assert(s.sweeps==2&&s.snapshots==4);
    first=s.first_result_ms;drain(&s,&p);
    assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);
    assert(!s.has_first_result&&!s.first_result_ms&&!s.snapshots&&!s.sweeps&&s.count==1);
    await_snapshots(&s,&p,1);assert(s.has_first_result&&s.first_result_ms<first);drain(&s,&p);

    init_live(&s,&p);p.unknown_age=true;start(&s,&p);await_snapshots(&s,&p,1);
    assert(!s.rows[0].timing_flags&&s.rows[0].callback_age_ms==UINT32_MAX);
    assert(cerberus_dual_ble_age_upper(s.rows[0].callback_age_ms,s.rows[0].request_started_ms,p.now)==UINT32_MAX);
    drain(&s,&p);
    init_live(&s,&p);s.ops.now_ms=NULL;start(&s,&p);await_snapshots(&s,&p,1);
    assert(!s.has_first_result&&!s.first_result_ms);drain(&s,&p);
    init_live(&s,&p);start(&s,&p);p.now+=(uint64_t)UINT32_MAX+1U;
    await_snapshots(&s,&p,1);assert(!s.has_first_result&&!s.first_result_ms&&!s.discovery_clock_valid);
    await_snapshots(&s,&p,2);assert(!s.has_first_result&&!s.first_result_ms);drain(&s,&p);

    cerberus_dual_ble_view_t view;cerberus_dual_ble_view(&s,&view);
    assert(view.snapshots==s.snapshots&&!view.has_first_result&&view.live_updates);
    puts("dual BLE live snapshots: early atomic banks, 32x11 bounded transfer, retry anchor, coalescing, final sweep and first-result metrics passed");
}
static void check_snapshot_faults_and_stop(void)
{
    cerberus_dual_ble_scan_t s;peer_t p;
    for(unsigned fault=1;fault<FAULT_END;++fault) {
        init_live(&s,&p);p.fault=fault;s.count=1;s.rows[0].rssi=-111;
        cerberus_dual_ble_row_t saved=s.rows[0];start(&s,&p);
        for(unsigned i=0;i<200&&!s.error;++i)fast_tick(&s,&p);
        assert(s.error&&s.count==1&&!memcmp(&saved,&s.rows[0],sizeof(saved))&&!s.snapshots&&!s.has_first_result);
        drain(&s,&p);
    }
    /* Stop at every encountered phase/chunk keeps the displayed bank unchanged
       and goes directly through scoped STOP/RELEASE, never another download. */
    init_live(&s,&p);p.count=3;s.count=1;s.rows[0].rssi=-100;
    assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);
    uint32_t phases=0;
    for(unsigned i=0;i<300&&!s.sweeps;++i) {
        phases|=1U<<s.phase;
        cerberus_dual_ble_scan_t stopped=s;peer_t other=p;stopped.ops.context=&other;
        cerberus_dual_ble_row_t saved[32];memcpy(saved,stopped.rows,sizeof(saved));
        unsigned reads=other.snapshot_reads,begins=other.snapshot_begins,fences=other.snapshot_fences;
        drain(&stopped,&other);
        assert(other.snapshot_reads==reads&&other.snapshot_begins==begins&&other.snapshot_fences==fences);
        assert(!memcmp(saved,stopped.rows,sizeof(saved)));
        /* One call per tick exposes BEGIN/READ/FENCE/RELEASE individually. */
        p.cost_ms=9;fast_tick(&s,&p);
        if(s.snapshots)completed(&p);
    }
    assert(s.sweeps==1&&s.snapshots>=2);
    assert((phases&(1U<<11))&&(phases&(1U<<12))&&(phases&(1U<<13))); /* All snapshot phases. */
    drain(&s,&p);

    init_live(&s,&p);start(&s,&p);s.snapshot_request_id=UINT64_MAX;
    for(unsigned i=0;i<40&&!s.error;++i)fast_tick(&s,&p);
    assert(s.error==ESP_ERR_INVALID_STATE&&!p.snapshot_begins);drain(&s,&p);
    /* Negotiated capability and selected status version must agree, including
       the real C5 v2 SNAPSHOTS bit. A legacy peer may not smuggle that bit in. */
    init_live(&s,&p);p.status_remove_flags=CERBERUS_DUAL_BLE_SNAPSHOTS;start(&s,&p);
    assert(s.error==ESP_ERR_INVALID_RESPONSE&&!s.snapshots);p.status_remove_flags=0;drain(&s,&p);
    init(&s,&p);p.status_add_flags=CERBERUS_DUAL_BLE_SNAPSHOTS;start(&s,&p);
    assert(s.error==ESP_ERR_INVALID_RESPONSE&&!s.live_updates);p.status_add_flags=0;drain(&s,&p);
    init(&s,&p);p.snapshot_capability=true;
    assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);tick(&s,&p);
    assert(s.error==ESP_ERR_INVALID_RESPONSE&&!p.starts);
    /* An old READ from a preceding bank cannot replace retained rows,
       even with the same row/chunk position. Transport tokens are not proof. */
    init_live(&s,&p);start(&s,&p);await_snapshots(&s,&p,1);
    cerberus_dual_ble_row_t saved=s.rows[0];p.fault=BAD_READ_HANDLE;
    for(unsigned i=0;i<100&&!s.error;++i)fast_tick(&s,&p);
    assert(s.error==ESP_ERR_INVALID_RESPONSE&&s.snapshots==1&&!memcmp(&saved,&s.rows[0],sizeof(saved)));
    drain(&s,&p);
    puts("dual BLE snapshot validation: 29 malformed/stale/body/fence cases, Stop in every transfer phase and exhausted counter passed");
}
static void assert_provenance(const cerberus_dual_ble_provenance_t *p,
    uint64_t boot, uint64_t epoch, uint8_t generation, uint8_t index)
{
    assert(p->boot_id==boot && p->epoch==epoch && p->generation==generation && p->index==index);
}
static void check_published_provenance(void)
{
    for(unsigned version=1;version<=2;++version) {
        cerberus_dual_ble_scan_t s;peer_t p;init(&s,&p);
        p.version=(uint8_t)version;p.snapshot_capability=version==2;
        p.boot=UINT64_C(0x1234567812345678);p.epoch=UINT64_C(0x100000000);p.generation=255;
        start(&s,&p);
        const uint64_t old_boot=p.boot, old_epoch=p.epoch;
        const uint8_t old_generation=p.generation;
        assert(old_generation==0); /* Wrapped byte is valid, not the identity fence. */
        if(version==1)completed(&p);
        await_snapshots(&s,&p,1);
        cerberus_dual_ble_row_t selected, current;
        cerberus_dual_ble_provenance_t selected_source, current_source;
        unsigned frames=p.frames;
        assert(cerberus_dual_ble_copy_row(&s,1,&selected,&selected_source));
        assert(p.frames==frames);
        assert_provenance(&selected_source,old_boot,old_epoch,old_generation,1);
        drain(&s,&p);
        assert(cerberus_dual_ble_copy_row(&s,1,&current,&current_source));
        assert(!memcmp(&selected,&current,sizeof(selected)));
        assert_provenance(&current_source,old_boot,old_epoch,old_generation,1);

        /* A new boot/job changes mutable START/status immediately. The retained
         * bank's provenance must remain old, including during partial transfer. */
        p.boot+=UINT64_C(0x100000000);p.epoch+=UINT64_C(0x100000000);
        assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);
        tick(&s,&p);
        assert(s.start.boot_id==p.boot && s.start.boot_id!=old_boot);
        assert(cerberus_dual_ble_copy_row(&s,1,&current,&current_source));
        assert_provenance(&current_source,old_boot,old_epoch,old_generation,1);
        if(version==1)completed(&p);
        p.cost_ms=9;
        for(unsigned i=0;i<200&&!s.copied;++i)fast_tick(&s,&p);
        assert(s.copied&&!s.snapshots);
        drain(&s,&p); /* Cancelled staging never publishes its source metadata. */
        assert(cerberus_dual_ble_copy_row(&s,1,&current,&current_source));
        assert_provenance(&current_source,old_boot,old_epoch,old_generation,1);
        assert(!memcmp(&selected,&current,sizeof(selected)));

        p.cost_ms=1;
        assert(cerberus_dual_ble_begin(&s,0,NULL,0)==ESP_OK);tick(&s,&p);
        const uint64_t new_epoch=p.epoch;const uint8_t new_generation=p.generation;
        if(version==1)completed(&p);
        await_snapshots(&s,&p,1);
        assert(cerberus_dual_ble_copy_row(&s,1,&current,&current_source));
        assert_provenance(&current_source,p.boot,new_epoch,new_generation,1);
        assert_provenance(&selected_source,old_boot,old_epoch,old_generation,1);
        assert(selected.request_started_ms<current.request_started_ms);
        /* Invalid copies cannot leak a prior identity or half a row. */
        memset(&current,0xa5,sizeof(current));memset(&current_source,0xa5,sizeof(current_source));
        assert(!cerberus_dual_ble_copy_row(&s,32,&current,&current_source));
        const cerberus_dual_ble_row_t empty={0};
        assert(!memcmp(&current,&empty,sizeof(current)));
        assert_provenance(&current_source,0,0,0,0);
        assert(!cerberus_dual_ble_copy_row(&s,0,&current,NULL));
        drain(&s,&p);
    }
    /* Lost BEGIN replies must not move the frozen row's original anchor. */
    cerberus_dual_ble_scan_t s;peer_t p;init_live(&s,&p);p.lose_begin_replies=2;
    start(&s,&p);await_snapshots(&s,&p,1);
    cerberus_dual_ble_row_t row;cerberus_dual_ble_provenance_t source;
    assert(cerberus_dual_ble_copy_row(&s,0,&row,&source));
    assert(row.request_started_ms+1==p.snapshot_clock_ms && row.request_started_ms+200<=p.now);
    assert_provenance(&source,p.boot,p.epoch,p.generation,0);drain(&s,&p);
    puts("dual BLE publication provenance: v1/v2 full identities, generation wrap, retained/replaced/cancelled banks and lost-BEGIN anchor passed");
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
    check_live_snapshots();check_snapshot_faults_and_stop();check_published_provenance();
    printf("dual BLE reader: owner/replay/fence/lost-reply/age/typed-row checks passed; store=%zu bytes\n",sizeof(s));
    return 0;
}
