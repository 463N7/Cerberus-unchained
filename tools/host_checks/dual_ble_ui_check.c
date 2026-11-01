#define main inherited_monitor_suite
#define cerberus_display_fill_rect companion_original_fill_rect
#include "s3_findmy_monitor_ui_check.c"
#undef cerberus_display_fill_rect
#undef main
static unsigned companion_fail_fill_after;
static bool companion_fail_final_control;
esp_err_t cerberus_display_fill_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color)
{
    if(companion_fail_fill_after&&! --companion_fail_fill_after)return ESP_FAIL;
    return companion_original_fill_rect(x,y,w,h,color);
}
static esp_err_t draw_button(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const char *s)
{if(companion_fail_final_control&&y==370){companion_fail_final_control=false;return ESP_FAIL;}
 return draw_small_button(x,y,w,h,s);}
static unsigned companion_exchanges;
#include "dual_ble_scan.h"
static bool family_peer_enabled, family_peer_supported, family_peer_attached, family_peer_owned;
static bool family_peer_hold_stop;
static unsigned family_peer_starts, family_peer_stop_requests, family_peer_releases;
static unsigned family_peer_lost_release;
static uint32_t family_peer_next_job;
static cerberus_dual_ble_start_t family_peer_start;
static esp_err_t family_peer_exchange(uint8_t cmd,const void *q,size_t qn,void *r,size_t rn)
{
    assert(rn==22U);
    if(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_INFO){
        assert(!qn);
        cerberus_dual_ble_info_t info={.boot_id=0x12345678,.next_job_id=family_peer_next_job,
            .version=CERBERUS_DUAL_BLE_VERSION,.capacity=32,.raw_bytes=31,
            .flags=CERBERUS_DUAL_BLE_READY|CERBERUS_DUAL_BLE_SNAPSHOTS};
        memcpy(r,&info,sizeof(info));return ESP_OK;
    }
    if(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_START){
        assert(qn==sizeof(family_peer_start));
        const cerberus_dual_ble_start_t *start=q;
        if(start->mode==CERBERUS_DUAL_BLE_FAMILY&&!family_peer_supported)return ESP_ERR_NOT_SUPPORTED;
        assert(!family_peer_attached&&start->job_id==family_peer_next_job);
        family_peer_start=*start;family_peer_attached=family_peer_owned=true;
        ++family_peer_starts;++family_peer_next_job;
    }else{
        const cerberus_dual_ble_binding_t *binding=q;
        assert(qn==sizeof(*binding)&&family_peer_attached);
        assert(binding->boot_id==family_peer_start.boot_id&&binding->job_id==family_peer_start.job_id);
        if(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_STOP){
            ++family_peer_stop_requests;if(!family_peer_hold_stop)family_peer_owned=false;
        }else if(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE){
            assert(!family_peer_owned);
            if(family_peer_lost_release){--family_peer_lost_release;return ESP_ERR_TIMEOUT;}
            ++family_peer_releases;family_peer_attached=false;
        }else assert(cmd==CERBERUS_SPI_COMMAND_DUAL_BLE_STATUS);
    }
    cerberus_dual_ble_status_t status={.epoch=77U+family_peer_start.job_id,
        .job_id=family_peer_start.job_id,.version=family_peer_start.version,
        .state=family_peer_owned?CERBERUS_BLE_SCAN_RUNNING:CERBERUS_BLE_SCAN_COMPLETE,
        .flags=family_peer_attached?(CERBERUS_DUAL_BLE_ATTACHED|CERBERUS_DUAL_BLE_SNAPSHOTS|
            (family_peer_owned?CERBERUS_DUAL_BLE_OWNED:0U)):0U};
    memcpy(r,&status,sizeof(status));return ESP_OK;
}
esp_err_t c5_spi_link_dual_ble_exchange(uint8_t cmd,const void *q,size_t qn,void *r,size_t rn)
{memset(r,0,rn);++companion_exchanges;
 return family_peer_enabled?family_peer_exchange(cmd,q,qn,r,rn):ESP_ERR_NOT_SUPPORTED;}
static unsigned companion_allocations;
static void *companion_calloc(size_t n,size_t size,unsigned caps)
{++companion_allocations;return heap_caps_calloc(n,size,caps);}
#define heap_caps_calloc companion_calloc
#include "dual_ble_ui.h"
#undef heap_caps_calloc
#include "s3_ble_tracker.h"
static cerberus_c5_ble_result_t ble_fox_hunt_target;
static bool ble_fox_hunt_start_pending,ble_fox_hunt_exit_pending;
static esp_err_t ble_fox_hunt_error;
static cerberus_s3_ble_track_status_t fox_sample;
static unsigned fox_status_reads;
esp_err_t cerberus_s3_ble_scanner_track_status(cerberus_s3_ble_track_status_t *out)
{++fox_status_reads;*out=fox_sample;return ESP_OK;}
#include "dual_ble_fox_draw_impl.h"

static void check_list_order(void)
{
    cerberus_dual_ble_row_t rows[32]={0}, original[32];
    const char *names[]={"beta","Alpha","alpha","Able","Zed","No","","A\001B"};
    const int signals[]={-80,-40,-40,127,20,-128,-60,-50};
    for(unsigned i=0;i<8;++i){
        strcpy(rows[i].name,names[i]);rows[i].rssi=(int8_t)signals[i];
        memcpy(rows[i].address,"ABCDEF",6);rows[i].address_type=(uint8_t)(i%4);
        rows[i].timing_flags=CERBERUS_DUAL_BLE_SEEN|CERBERUS_DUAL_BLE_RSSI_VALID;
        rows[i].request_started_ms=100;
    }
    rows[4].timing_flags=CERBERUS_DUAL_BLE_SEEN; /* Good-looking value without validity. */
    memcpy(original,rows,sizeof(rows));
    struct {uint8_t before,indexes[32],after;} guarded={.before=0xA5,.after=0x5A};
    const uint8_t signal[]={1,2,7,6,0,3,5,4}, name[]={6,7,3,1,2,0,5,4};
    assert(cerberus_dual_ble_list_order(rows,8,CERBERUS_DUAL_BLE_SORT_SIGNAL,false,100,guarded.indexes)==8);
    assert(!memcmp(guarded.indexes,signal,sizeof(signal)));
    assert(cerberus_dual_ble_list_order(rows,8,CERBERUS_DUAL_BLE_SORT_NAME,false,100,guarded.indexes)==8);
    assert(!memcmp(guarded.indexes,name,sizeof(name)));
    assert(cerberus_dual_ble_list_order(rows,8,CERBERUS_DUAL_BLE_SORT_SCAN,false,100,guarded.indexes)==8);
    for(unsigned i=0;i<8;++i)assert(guarded.indexes[i]==i);
    assert(!memcmp(original,rows,sizeof(rows))); /* Typed collisions and source bytes untouched. */
    char label[CERBERUS_BLE_NAME_MAX_LENGTH+1U];
    cerberus_dual_ble_list_label(&rows[7],label);assert(!strcmp(label,"A?B"));
    cerberus_dual_ble_list_label(&rows[6],label);assert(!strcmp(label,"<UNNAMED>"));
    memset(rows[7].name,'x',sizeof(rows[7].name));rows[7].name[1]=(char)0x80;
    cerberus_dual_ble_list_label(&rows[7],label);
    assert(strlen(label)==CERBERUS_BLE_NAME_MAX_LENGTH&&label[1]=='?');
    rows[0].callback_age_ms=0;rows[1].callback_age_ms=1;
    rows[2].timing_flags=CERBERUS_DUAL_BLE_RSSI_VALID;
    assert(!cerberus_dual_ble_list_rssi_valid(&rows[2]));
    rows[3].callback_age_ms=UINT32_MAX;rows[4].request_started_ms=30101;
    rows[5].callback_age_ms=1;rows[5].request_started_ms=101;
    assert(cerberus_dual_ble_list_order(rows,6,CERBERUS_DUAL_BLE_SORT_SCAN,true,30100,guarded.indexes)==2);
    assert(guarded.indexes[0]==0&&guarded.indexes[1]==5); /* Inclusive conservative <=30,000ms. */
    assert(cerberus_dual_ble_list_order(rows,6,CERBERUS_DUAL_BLE_SORT_SCAN,true,30101,guarded.indexes)==1);
    assert(guarded.indexes[0]==4); /* Future copy anchor becomes valid only when reached. */
    uint32_t age=123;
    assert(!cerberus_dual_ble_list_age(&rows[2],30100,&age)&&age==123);
    assert(!cerberus_dual_ble_list_age(&rows[3],30100,&age));
    assert(!cerberus_dual_ble_list_age(&rows[0],99,&age));
    assert(!cerberus_dual_ble_list_age(&rows[0],UINT64_MAX,&age));
    rows[0].request_started_ms=0;assert(cerberus_dual_ble_list_age(&rows[0],0,&age)&&age==0);
    for(unsigned i=0;i<32;++i){rows[i].rssi=-50;rows[i].timing_flags=3;strcpy(rows[i].name,"same");}
    assert(cerberus_dual_ble_list_order(rows,32,CERBERUS_DUAL_BLE_SORT_SIGNAL,false,100,guarded.indexes)==32);
    for(unsigned i=0;i<32;++i)assert(guarded.indexes[i]==i);
    assert(cerberus_dual_ble_list_order(rows,33,CERBERUS_DUAL_BLE_SORT_NAME,false,100,guarded.indexes)==0);
    assert(cerberus_dual_ble_list_order(rows,32,(cerberus_dual_ble_sort_t)99,false,100,guarded.indexes)==0);
    assert(cerberus_dual_ble_list_order(NULL,1,CERBERUS_DUAL_BLE_SORT_NAME,false,100,guarded.indexes)==0);
    assert(cerberus_dual_ble_list_order(NULL,0,CERBERUS_DUAL_BLE_SORT_NAME,false,100,guarded.indexes)==0);
    assert(guarded.before==0xA5&&guarded.after==0x5A);
}

static void paint_list(void)
{
    drawn[0]=0;
    assert(cerberus_display_fill_rect(0,0,320,480,UI_BACKGROUND)==ESP_OK);
    assert(dual_ble_ui_draw(false)==ESP_OK);
}

static void check_list_controls(const char *prefix)
{
    unsigned calls=companion_exchanges,radio_starts=starts,radio_stops=stops;
    cerberus_c5_ble_result_t s3_before=ble_selected_result;
    dual_ble_ui_detail=false;dual_ble_ui_offset=0;dual_ble_ui_recent_only=false;
    dual_ble_ui_sort=CERBERUS_DUAL_BLE_SORT_SIGNAL;dual_ble_ui_store->count=6;clock_ms=1000;
    dual_ble_ui_store->published_boot_id=17;dual_ble_ui_store->published_epoch=23;
    dual_ble_ui_store->published_generation=0; /* Wrapped generation is valid. */
    for(unsigned i=0;i<6;++i){
        cerberus_dual_ble_row_t *r=&dual_ble_ui_store->rows[i];memset(r,0,sizeof(*r));
        memcpy(r->address,"ABCDEF",6);r->address_type=(uint8_t)(i%4);r->rssi=-60-(int)i;
        snprintf(r->name,sizeof(r->name),"Peer %u",5U-i);r->request_started_ms=1000;r->timing_flags=3;
        r->raw_length=3;r->raw[0]=2;r->raw[1]=1;r->raw[2]=(uint8_t)i;
    }
    paint_list();assert(strstr(drawn,"SIGNAL >")&&strstr(drawn,"ALL AGES >"));
    dual_ble_ui_store->inventory_known=false;
    paint_list();assert(strstr(drawn,"BANK INVENTORY COUNTS UNAVAILABLE"));
    dual_ble_ui_store->inventory_known=dual_ble_ui_store->inventory_replacement_enabled=true;
    dual_ble_ui_store->inventory_replaced=UINT32_MAX;dual_ble_ui_store->inventory_dropped=UINT32_MAX;
    paint_list();assert(strstr(drawn,"BANK REPLACED 4294967295 / DROPPED 4294967295"));
    save(prefix,"inventory-counters");
    assert(!strcmp(dual_ble_ui_painted[0].name,"Peer 5"));
    assert(dual_ble_ui_tap(40,120)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(dual_ble_ui_sort==CERBERUS_DUAL_BLE_SORT_NAME&&!strcmp(dual_ble_ui_painted[0].name,"Peer 0"));
    save(prefix,"name-sorted");
    /* A bank replacement after paint must never redirect the selected source. */
    memset(dual_ble_ui_store->rows,0,sizeof(dual_ble_ui_store->rows));dual_ble_ui_store->count=0;
    dual_ble_ui_store->published_boot_id=99;dual_ble_ui_store->published_epoch=100;
    dual_ble_ui_store->published_generation=88;
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW);
    cerberus_c5_ble_result_t target;assert(dual_ble_ui_selected_peer(&target));
    assert(!strcmp(target.name,"Peer 0")&&target.address_type==1&&!memcmp(target.address,"ABCDEF",6));
    assert(dual_ble_ui_selected.raw[2]==5&&target.flags==0);
    assert(dual_ble_ui_selected_provenance.boot_id==17&&dual_ble_ui_selected_provenance.epoch==23&&
        dual_ble_ui_selected_provenance.generation==0&&dual_ble_ui_selected_provenance.index==5);
    assert(dual_ble_ui_tap(20,330)==DUAL_BLE_UI_REDRAW);
    paint_list();assert(dual_ble_ui_painted_count==0&&dual_ble_ui_offset==0);
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_NONE);
    for(unsigned i=0;i<6;++i){
        cerberus_dual_ble_row_t *r=&dual_ble_ui_store->rows[i];
        memcpy(r->address,"ABCDEF",6);r->address_type=(uint8_t)(i%4);
        snprintf(r->name,sizeof(r->name),"Age %u",i);r->rssi=-70;r->timing_flags=3;
        r->request_started_ms=1000;r->callback_age_ms=0;
    }
    dual_ble_ui_store->count=6;
    assert(dual_ble_ui_tap(40,120)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(dual_ble_ui_sort==CERBERUS_DUAL_BLE_SORT_SCAN&&strstr(drawn,"SCAN ORDER >"));
    assert(dual_ble_ui_tap(200,340)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(dual_ble_ui_offset==4&&dual_ble_ui_painted_count==2);
    /* Same count, filter expires: no published-bank event is required. */
    dual_ble_ui_store->rows[0].callback_age_ms=30000;
    dual_ble_ui_store->rows[1].callback_age_ms=30001;
    dual_ble_ui_store->rows[2].timing_flags=2;
    dual_ble_ui_store->rows[3].request_started_ms=1001;
    dual_ble_ui_store->rows[4].callback_age_ms=UINT32_MAX;
    dual_ble_ui_store->rows[5].callback_age_ms=30000;
    assert(dual_ble_ui_tap(200,120)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(dual_ble_ui_recent_only&&dual_ble_ui_offset==0&&dual_ble_ui_visible_count==2);
    assert(!strcmp(dual_ble_ui_painted[0].name,"Age 0")&&!strcmp(dual_ble_ui_painted[1].name,"Age 5"));
    assert(strstr(drawn,"WITHIN 30S >"));save(prefix,"recent-boundary");
    dual_ble_ui_store->rows[3].timing_flags=0;clock_ms=1001;paint_list();
    assert(!dual_ble_ui_visible_count&&!dual_ble_ui_painted_count);
    assert(strstr(drawn,"NO CONFIRMED RECENT C5 RESULTS")&&!strstr(drawn,"NO C5 OBSERVATIONS WITHIN 30S"));
    save(prefix,"recent-empty");
    assert(dual_ble_ui_tap(200,120)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"RECENCY?")&&strstr(drawn,"AGE UNKNOWN")&&!strstr(drawn,"STALE"));
    save(prefix,"recency-unconfirmed");
    assert(dual_ble_ui_tap(200,340)==DUAL_BLE_UI_REDRAW);paint_list();
    dual_ble_ui_store->count=1;paint_list(); /* Published bank shrank under page two. */
    assert(dual_ble_ui_offset==0&&dual_ble_ui_painted_count==1&&dual_ble_ui_visible_count==1);
    assert(dual_ble_ui_tap(200,340)==DUAL_BLE_UI_NONE);
    assert(dual_ble_ui_tap(160,120)==DUAL_BLE_UI_NONE); /* Control gap. */
    assert(dual_ble_ui_tap(20,190)==DUAL_BLE_UI_NONE); /* Row gap. */
    assert(dual_ble_ui_tap(40,120)==DUAL_BLE_UI_REDRAW);
    assert(dual_ble_ui_sort==CERBERUS_DUAL_BLE_SORT_SIGNAL); /* Complete control cycle. */
    paint_list();assert(strstr(drawn,"46:45:44:43:42:41"));
    masked=true;paint_list();assert(!strstr(drawn,"46:45:44:43:42:41")&&strstr(drawn,"BLE-")&&strstr(drawn,"Age 0"));
    dual_ble_ui_store->rows[0].name[0]='\0';paint_list();
    assert(strstr(drawn,"Unnamed device")&&!strstr(drawn,"46:45:44:43:42:41"));save(prefix,"masked-unnamed-list");
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"C5 / Unnamed device")&&!strstr(drawn,"46:45:44:43:42:41"));save(prefix,"masked-unnamed-detail");
    assert(dual_ble_ui_tap(250,210)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"C5 / Unnamed device")&&!strstr(drawn,"46:45:44:43:42:41"));
    assert(dual_ble_ui_tap(20,330)==DUAL_BLE_UI_REDRAW);masked=false;
    memset(dual_ble_ui_store->rows[0].name,'N',sizeof(dual_ble_ui_store->rows[0].name));
    dual_ble_ui_store->rows[0].name[1]=(char)0xFF;paint_list();
    assert(strstr(drawn,"N?NNNNNNNNNNNNNNNNNNNNNNNNNNNNNN"));save(prefix,"bounded-name");
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"C5 / N?NNNNNNNNNNNNNNNNNNNNNNNNNNNNNN"));save(prefix,"bounded-name-detail");
    assert(dual_ble_ui_selected_peer(&target)&&target.name[sizeof(target.name)-1U]=='\0');
    assert(dual_ble_ui_selected.name[sizeof(dual_ble_ui_selected.name)-1U]=='N'); /* Original retained. */
    assert(companion_exchanges==calls&&starts==radio_starts&&stops==radio_stops);
    assert(!memcmp(&s3_before,&ble_selected_result,sizeof(s3_before)));
}

static void check_reader_metrics(const char *prefix)
{
    unsigned calls=companion_exchanges,radio_starts=starts,radio_stops=stops;
    dual_ble_ui_detail=false;dual_ble_ui_offset=0;dual_ble_ui_recent_only=false;
    dual_ble_ui_store->count=1;dual_ble_ui_store->error=0;
    dual_ble_ui_store->live_updates=true;dual_ble_ui_store->snapshots=7;
    dual_ble_ui_store->sweeps=0;dual_ble_ui_store->has_first_result=true;
    dual_ble_ui_store->first_result_ms=627;dual_ble_ui_store->enabled=true;
    strcpy(dual_ble_ui_store->rows[0].name,"Snapshot test peer");
    dual_ble_ui_store->rows[0].callback_age_ms=0;
    dual_ble_ui_store->rows[0].request_started_ms=(uint64_t)clock_ms;
    dual_ble_ui_store->remote.flags=CERBERUS_DUAL_BLE_ATTACHED|CERBERUS_DUAL_BLE_OWNED;
    dual_ble_ui_store->phase=5; /* Existing NAME-reading phase: rendering accepts overlapping flags. */
    paint_list();
    assert(strstr(drawn,"C5 LIVE / 1 OF 1 / 7 UPDATES"));
    assert(strstr(drawn,"RECEIVING + READING / 0 RF SWEEPS"));
    assert(strstr(drawn,"FIRST RESULTS IN 627 ms"));save(prefix,"live-reading");
    dual_ble_ui_store->first_result_ms=0;paint_list();
    assert(strstr(drawn,"FIRST RESULTS IN 0 ms")); /* Zero is valid only with the explicit flag. */
    dual_ble_ui_store->has_first_result=false;paint_list();
    assert(strstr(drawn,"NO NEW RESULTS THIS RUN")&&!strstr(drawn,"FIRST RESULTS IN"));
    dual_ble_ui_store->count=0;paint_list();
    assert(strstr(drawn,"WAITING FOR C5 DISCOVERIES")&&!strstr(drawn,"COMPLETED C5 SWEEP"));
    dual_ble_ui_store->live_updates=false;dual_ble_ui_store->remote.flags=CERBERUS_DUAL_BLE_ATTACHED;
    dual_ble_ui_store->sweeps=1;paint_list();
    assert(strstr(drawn,"C5 WHOLE SWEEP")&&strstr(drawn,"READING / 1 RF SWEEPS"));
    assert(strstr(drawn,"WAITING FOR A COMPLETED C5 SWEEP")&&!strstr(drawn,"UPDATES"));
    save(prefix,"whole-sweep");
    dual_ble_ui_store->live_updates=true;dual_ble_ui_store->snapshots=UINT32_MAX;
    dual_ble_ui_store->sweeps=UINT32_MAX;dual_ble_ui_store->has_first_result=true;
    dual_ble_ui_store->first_result_ms=UINT32_MAX;paint_list();
    assert(strstr(drawn,"4294967295 UPDATES")&&strstr(drawn,"4294967295 RF SWEEPS"));
    assert(strstr(drawn,"FIRST RESULTS IN 4294967295 ms"));save(prefix,"reader-metric-bounds");
    assert(companion_exchanges==calls&&starts==radio_starts&&stops==radio_stops);
}

static void check_receive_metadata(const char *prefix)
{
    unsigned calls=companion_exchanges,radio_starts=starts,radio_stops=stops;
    cerberus_c5_ble_result_t s3_before=ble_selected_result;
    dual_ble_ui_open(true);dual_ble_ui_offset=0;dual_ble_ui_recent_only=false;
    dual_ble_ui_store->count=1;
    cerberus_dual_ble_row_t *r=&dual_ble_ui_store->rows[0];memset(r,0,sizeof(*r));
    memcpy(r->address,"ABCDEF",6);strcpy(r->name,"Extended test observation");
    r->rssi=-45;r->timing_flags=3;r->request_started_ms=(uint64_t)clock_ms;
    r->raw_length=31;r->raw[0]=254;r->raw[1]=0xFF;r->event_type=3;
    r->raw_flags=CERBERUS_BLE_ADVERTISEMENT_TRUNCATED|CERBERUS_BLE_ADVERTISEMENT_MALFORMED;
    r->receive=(cerberus_dual_ble_receive_metadata_t){
        .source_kind=CERBERUS_DUAL_BLE_RECEIVE_EXTENDED,.callback_length=255,.sid=15,
        .primary_phy=1,.secondary_phy=3,.ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
    assert(cerberus_dual_ble_receive_metadata_valid(&r->receive,r->raw_length,r->event_type,r->raw_flags));
    paint_list();
    cerberus_dual_ble_receive_metadata_t frozen=r->receive;
    memset(&r->receive,0,sizeof(r->receive));r->raw_length=3; /* New live bank cannot rebind selection. */
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW);
    assert(!memcmp(&dual_ble_ui_selected.receive,&frozen,sizeof(frozen)));
    assert(!dual_ble_ui_receive_info);
    paint_list();assert(strstr(drawn,"ORIGINAL ADV 31 B")&&strstr(drawn,"RAW ADV *"));
    save(prefix,"metadata-raw-prefix");
    assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"C5 OBSERVATIONS ONLY")&&!strstr(drawn,"RX INFO NOT SAVED"));
    assert(strstr(drawn,"C5 EXTENDED CALLBACK")&&strstr(drawn,"CALLBACK 255 B / PREVIEW 31 B"));
    assert(strstr(drawn,"PHY 1M PRIMARY / CODED SECONDARY")&&strstr(drawn,"SID 15 / PROPS 00 / CTRL COMPLETE"));
    assert(strstr(drawn,"FULL AD LAYOUT VALID / PREVIEW CUT"));
    assert(!strstr(drawn,"FULL AD LAYOUT INVALID"));save(prefix,"metadata-extended");
    assert(dual_ble_ui_tap(108,210)==DUAL_BLE_UI_NONE); /* Gaps between three tabs. */
    assert(dual_ble_ui_tap(212,210)==DUAL_BLE_UI_NONE);
    assert(dual_ble_ui_tap(200,224)==DUAL_BLE_UI_NONE);
    assert(dual_ble_ui_tap(20,210)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"ORIGINAL ADV 31 B")&&!strstr(drawn,"CTRL COMPLETE")&&!strstr(drawn,"RX INFO NOT SAVED"));
    assert(!memcmp(&dual_ble_ui_selected.receive,&frozen,sizeof(frozen)));

    dual_ble_ui_selected.receive.ad_validation=CERBERUS_DUAL_BLE_AD_INVALID;
    dual_ble_ui_selected.receive.primary_phy=3;dual_ble_ui_selected.receive.secondary_phy=2;
    dual_ble_ui_selected.receive.sid=0xFF;
    assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"PHY CODED PRIMARY / 2M SECONDARY")&&strstr(drawn,"SID N/A / PROPS 00 / CTRL COMPLETE"));
    assert(strstr(drawn,"FULL AD LAYOUT INVALID / PREVIEW CUT"));
    save(prefix,"metadata-malformed-full");
    dual_ble_ui_selected.receive=(cerberus_dual_ble_receive_metadata_t){
        .source_kind=CERBERUS_DUAL_BLE_RECEIVE_LEGACY,.callback_length=3,.sid=0xFF,
        .data_status=0xFF,.ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
    dual_ble_ui_selected.raw_length=3;dual_ble_ui_selected.raw_flags=0;
    memset(dual_ble_ui_selected.raw,0,sizeof(dual_ble_ui_selected.raw));
    dual_ble_ui_selected.raw[0]=2;dual_ble_ui_selected.raw[1]=1;dual_ble_ui_selected.raw[2]=6;
    assert(cerberus_dual_ble_receive_metadata_valid(&dual_ble_ui_selected.receive,3,3,0));paint_list();
    assert(strstr(drawn,"C5 LEGACY CALLBACK")&&strstr(drawn,"CALLBACK 3 B / PREVIEW 3 B"));
    assert(strstr(drawn,"PHY N/A PRIMARY / N/A SECONDARY")&&strstr(drawn,"SID N/A / PROPS N/A / CTRL STATUS N/A"));
    assert(!strstr(drawn,"CTRL COMPLETE")&&!strstr(drawn,"PREVIEW CUT"));save(prefix,"metadata-legacy");
    dual_ble_ui_selected.receive.source_kind=CERBERUS_DUAL_BLE_RECEIVE_EXTENDED;
    dual_ble_ui_selected.receive.properties=0x10;dual_ble_ui_selected.receive.primary_phy=1;
    dual_ble_ui_selected.receive.data_status=0;
    assert(cerberus_dual_ble_receive_metadata_valid(&dual_ble_ui_selected.receive,3,3,0));paint_list();
    assert(strstr(drawn,"C5 EXT CALLBACK / LEGACY PDU"));save(prefix,"metadata-legacy-pdu");
    memset(&dual_ble_ui_selected.receive,0,sizeof(dual_ble_ui_selected.receive));paint_list();
    assert(strstr(drawn,"RECEIVE INFO UNAVAILABLE")&&strstr(drawn,"NO METADATA FOR THIS OBSERVATION"));
    assert(!strstr(drawn,"CTRL COMPLETE")&&!strstr(drawn,"CALLBACK 0 B"));save(prefix,"metadata-unknown");
    assert(dual_ble_ui_tap(20,330)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW&&!dual_ble_ui_receive_info);
    assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW&&dual_ble_ui_receive_info);
    dual_ble_ui_open(true);assert(!dual_ble_ui_detail&&!dual_ble_ui_receive_info);
    assert(companion_exchanges==calls&&starts==radio_starts&&stops==radio_stops);
    assert(!memcmp(&s3_before,&ble_selected_result,sizeof(s3_before)));
}

static void check_paint_failure_and_unified_selection(const char *prefix)
{
    unsigned exchanges=companion_exchanges,radio_starts=starts,radio_stops=stops;
    dual_ble_ui_open(true);dual_ble_ui_offset=0;dual_ble_ui_recent_only=false;
    dual_ble_ui_store->count=4;dual_ble_ui_store->published_boot_id=101;
    dual_ble_ui_store->published_epoch=202;dual_ble_ui_store->published_generation=0;
    for(unsigned i=0;i<4;++i){
        cerberus_dual_ble_row_t *row=&dual_ble_ui_store->rows[i];memset(row,0,sizeof(*row));
        memcpy(row->address,"ABCDEF",6);row->address_type=(uint8_t)i;row->rssi=-40-(int)i;
        snprintf(row->name,sizeof(row->name),"Frozen source %u",i);
        row->timing_flags=3;row->request_started_ms=(uint64_t)clock_ms;
    }
    paint_list();assert(dual_ble_ui_painted_count==4);
    companion_fail_fill_after=2;assert(dual_ble_ui_draw(false)==ESP_FAIL);
    assert(!dual_ble_ui_painted_count&&dual_ble_ui_tap(20,160)==DUAL_BLE_UI_NONE);
    paint_list();assert(dual_ble_ui_painted_count==4);
    companion_fail_final_control=true;assert(dual_ble_ui_draw(false)==ESP_FAIL);
    assert(!dual_ble_ui_painted_count&&dual_ble_ui_tap(20,160)==DUAL_BLE_UI_NONE);
    paint_list();assert(dual_ble_ui_painted_count==4);
    cerberus_dual_ble_row_t row,expected;
    cerberus_dual_ble_provenance_t proof,expected_proof;
    assert(cerberus_dual_ble_copy_row(dual_ble_ui_store,2,&row,&proof));
    expected=row;expected_proof=proof;
    dual_ble_ui_select_frozen(&row,&proof);
    memset(&row,0,sizeof(row));memset(&proof,0,sizeof(proof));
    memset(dual_ble_ui_store->rows,0,sizeof(dual_ble_ui_store->rows));
    ++dual_ble_ui_store->published_epoch;++dual_ble_ui_store->published_boot_id;
    assert(dual_ble_ui_detail&&dual_ble_ui_from_unified&&!dual_ble_ui_receive_info);
    assert(!memcmp(&expected,&dual_ble_ui_selected,sizeof(expected)));
    assert(!memcmp(&expected_proof,&dual_ble_ui_selected_provenance,sizeof(expected_proof)));
    paint_list();assert(strstr(drawn,"BACK TO BLE LIST"));save(prefix,"unified-frozen-detail");
    assert(dual_ble_ui_tap(40,335)==DUAL_BLE_UI_BACK_S3);
    assert(!memcmp(&expected,&dual_ble_ui_selected,sizeof(expected))); /* Retained detail is inert off-page. */
    dual_ble_ui_open(false);assert(!dual_ble_ui_from_unified);
    assert(companion_exchanges==exchanges&&starts==radio_starts&&stops==radio_stops);
}

static void hint_row(cerberus_dual_ble_row_t *row,const uint8_t *bytes,uint8_t length)
{
    memset(row,0,sizeof(*row));memcpy(row->address,"ABCDEF",6);row->address_type=1;
    strcpy(row->name,"Frozen original");row->rssi=-70;row->timing_flags=3;
    row->request_started_ms=(uint64_t)clock_ms;row->event_type=3;row->raw_length=length;
    if(length)memcpy(row->raw,bytes,length);
    row->receive=(cerberus_dual_ble_receive_metadata_t){
        .source_kind=CERBERUS_DUAL_BLE_RECEIVE_LEGACY,.callback_length=length,.sid=255,
        .data_status=255,.ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
}
static void show_hints(const cerberus_dual_ble_row_t *row,const cerberus_dual_ble_provenance_t *proof)
{
    dual_ble_ui_select_frozen(row,proof);
    assert(!dual_ble_ui_hints&&!dual_ble_ui_receive_info&&!dual_ble_ui_hint_page);
    assert(dual_ble_ui_tap(250,210)==DUAL_BLE_UI_REDRAW&&dual_ble_ui_hints&&!dual_ble_ui_receive_info);
    paint_list();assert((strstr(drawn,"C5 ORIGINAL")||strstr(drawn,"C5 FULL ORIGINAL"))&&
        strstr(drawn,"/ HINTS")&&strstr(drawn,"HINTS *"));
    assert(!strstr(drawn,"BEEP")&&!strstr(drawn,"CONNECT"));
}
static void hint_next(void)
{assert(dual_ble_ui_tap(160,303)==DUAL_BLE_UI_REDRAW);paint_list();}
static void check_family_hints(const char *prefix)
{
    unsigned exchanges=companion_exchanges,radio_starts=starts,radio_stops=stops,allocations=companion_allocations;
    cerberus_c5_ble_result_t s3_before=ble_selected_result;
    const cerberus_dual_ble_provenance_t proof={.boot_id=123,.epoch=456,.generation=0,.index=31};
    cerberus_dual_ble_row_t row;
    /* Four independent families in one complete original. Aggregate peer
       metadata is deliberately misleading; only raw AD determines facts. */
    const uint8_t mixed[]={7,3,0x44,0xfd,0x81,0x30,0x5f,0xfd,6,9,'H','C','-','0','5'};
    hint_row(&row,mixed,sizeof(mixed));strcpy(row.name,"HC-06");row.manufacturer_id=0x004c;
    show_hints(&row,&proof);
    assert(strstr(drawn,"COMPLETE REPORT / 4 FAMILY HINTS")&&strstr(drawn,"FIND MY FLIPPER META HC NAME"));
    assert(dual_ble_ui_hint_pages==5&&strstr(drawn,"NO RESPONSE DATA"));save(prefix,"hints-mixed-summary");
    hint_next();assert(strstr(drawn,"FIND MY FORMAT HINTS")&&strstr(drawn,"APPLE NO / FMNA YES / DULT NO"));
    assert(strstr(drawn,"FMNA UUID WIDTHS: 16"));save(prefix,"hints-findmy");
    hint_next();assert(strstr(drawn,"FLIPPER ADVERTISED SERVICE CODES")&&strstr(drawn,"CODES: 3081"));
    assert(strstr(drawn,"SAME-REPORT NAME NO / SERVICE DATA NO"));save(prefix,"hints-flipper");
    hint_next();assert(strstr(drawn,"META IDENTIFIER HINTS")&&strstr(drawn,"ID FD5F / LIST16"));save(prefix,"hints-meta");
    hint_next();assert(strstr(drawn,"HC COMPLETE-NAME HINT")&&strstr(drawn,"HC-05")&&!strstr(drawn,"EXACT NAME HC-06"));
    save(prefix,"hints-hc");hint_next();assert(!dual_ble_ui_hint_page);
    /* Controls have nonoverlapping bounds and never invoke transport/radio.
       Pagination is enabled only by a fully successful paint. */
    assert(dual_ble_ui_tap(108,210)==DUAL_BLE_UI_NONE&&dual_ble_ui_tap(212,210)==DUAL_BLE_UI_NONE);
    assert(dual_ble_ui_tap(20,224)==DUAL_BLE_UI_NONE&&dual_ble_ui_tap(20,312)==DUAL_BLE_UI_NONE);
    assert(dual_ble_ui_tap(312,303)==DUAL_BLE_UI_NONE);
    companion_fail_final_control=true;assert(dual_ble_ui_draw(false)==ESP_FAIL);
    assert(!dual_ble_ui_hint_pages&&dual_ble_ui_tap(160,303)==DUAL_BLE_UI_NONE);
    paint_list();hint_next();
    assert(dual_ble_ui_tap(20,210)==DUAL_BLE_UI_REDRAW&&!dual_ble_ui_hints);paint_list();
    assert(strstr(drawn,"ORIGINAL ADV 15 B")&&!strstr(drawn,"FIND MY FORMAT HINTS"));
    assert(dual_ble_ui_tap(160,303)==DUAL_BLE_UI_NONE);
    assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW&&dual_ble_ui_receive_info);paint_list();
    assert(strstr(drawn,"C5 LEGACY CALLBACK")&&!strstr(drawn,"FIND MY FORMAT HINTS"));
    assert(dual_ble_ui_tap(250,210)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"FIND MY FORMAT HINTS"));
    assert(dual_ble_ui_tap(20,330)==DUAL_BLE_UI_BACK_S3&&dual_ble_ui_tap(200,330)==DUAL_BLE_UI_FOX);
    assert(dual_ble_ui_tap(20,65)==DUAL_BLE_UI_BACK_S3&&dual_ble_ui_tap(100,390)==DUAL_BLE_UI_TOGGLE);

    uint8_t beacon[30]={2,1,6,26,0xff,0x4c,0,2,0x15};
    for(unsigned i=0;i<16;++i)beacon[9+i]=(uint8_t)i;
    beacon[25]=beacon[26]=beacon[27]=255;beacon[28]=254;beacon[29]=128;
    hint_row(&row,beacon,sizeof(beacon));show_hints(&row,&proof);hint_next();
    assert(strstr(drawn,"IBEACON ADVERTISED FORMAT")&&strstr(drawn,"00010203-0405-0607-0809-0A0B0C0D0E0F"));
    assert(strstr(drawn,"MAJOR 65535 / MINOR 65534")&&strstr(drawn,"MEASURED POWER -128 dBm (ADVERTISED)"));
    assert(strstr(drawn,"LAST ADV -70 dBm"));save(prefix,"hints-ibeacon");
    /* Immutable painted selection, including old generation0 and source age. */
    cerberus_dual_ble_row_t frozen=dual_ble_ui_selected;
    memset(dual_ble_ui_store->rows,0,sizeof(dual_ble_ui_store->rows));
    ++dual_ble_ui_store->published_epoch;++dual_ble_ui_store->published_boot_id;
    memset(&row,0,sizeof(row));paint_list();
    assert(!memcmp(&frozen,&dual_ble_ui_selected,sizeof(frozen)));
    assert(!memcmp(&proof,&dual_ble_ui_selected_provenance,sizeof(proof)));
    assert(strstr(drawn,"00010203-0405-0607-0809-0A0B0C0D0E0F"));
    hint_next();assert(strstr(drawn,"META HINT SUPPRESSED BY ID RULE")&&strstr(drawn,"ID 004C / MFG16"));

    const uint8_t flock[]={3,3,0,0x31,3,3,0,0x32};
    hint_row(&row,flock,sizeof(flock));show_hints(&row,&proof);hint_next();
    assert(strstr(drawn,"FLOCK ADVERTISED CLUES")&&strstr(drawn,"SERVICES: 3100 3200"));save(prefix,"hints-flock");
    hint_row(&row,NULL,0);memcpy(row.address,(uint8_t[]){1,2,3,0x81,0x8e,0x58},6);row.address_type=0;
    show_hints(&row,&proof);assert(strstr(drawn,"C5 ORIGINAL + ADDRESS")&&strstr(drawn,"FLOCK ADDR"));
    hint_next();assert(strstr(drawn,"PUBLIC ADDRESS PREFIX CLUE / NOT ID")&&strstr(drawn,"FLOCK ADDRESS PREFIX CLUE"));
    assert(!strstr(drawn,"FLOCK ADVERTISED CLUES"));save(prefix,"hints-flock-address");

    const uint8_t meta_many[]={9,3,0x5f,0xfd,0xb7,0xfe,0xb8,0xfe,0x4c,0};
    hint_row(&row,meta_many,sizeof(meta_many));show_hints(&row,&proof);
    assert(dual_ble_ui_hint_pages==3&&strstr(drawn,"NO SUPPORTED FAMILY HINT")&&strstr(drawn,"META ID RULE SUPPRESSES"));
    hint_next();assert(strstr(drawn,"ID FD5F / LIST16")&&strstr(drawn,"ID FEB7 / LIST16"));
    assert(strstr(drawn,"ID/CONTEXT PAIRS 1-2 OF 4"));save(prefix,"hints-meta-pairs-first");
    hint_next();assert(strstr(drawn,"ID FEB8 / LIST16")&&strstr(drawn,"ID 004C / LIST16"));
    assert(strstr(drawn,"ID/CONTEXT PAIRS 3-4 OF 4")&&!strstr(drawn,"MORE ID/CONTEXT"));save(prefix,"hints-meta-pairs-last");
    hint_next();assert(!dual_ble_ui_hint_page&&strstr(drawn,"NO SUPPORTED FAMILY HINT"));

    const uint8_t flags[]={2,1,6};hint_row(&row,flags,sizeof(flags));show_hints(&row,&proof);
    assert(strstr(drawn,"COMPLETE REPORT / 0 FAMILY HINTS")&&strstr(drawn,"NO SUPPORTED FAMILY HINT"));
    assert(dual_ble_ui_hint_pages==1&&dual_ble_ui_tap(160,303)==DUAL_BLE_UI_NONE);save(prefix,"hints-complete-none");
    memset(&row.receive,0,sizeof(row.receive));show_hints(&row,&proof);
    assert(strstr(drawn,"RECEIVE INFO UNAVAILABLE")&&!strstr(drawn,"NO SUPPORTED FAMILY HINT"));save(prefix,"hints-unknown");
    hint_row(&row,beacon,sizeof(beacon));row.raw_length=31;row.raw_flags=1;
    row.receive=(cerberus_dual_ble_receive_metadata_t){.source_kind=CERBERUS_DUAL_BLE_RECEIVE_EXTENDED,
        .callback_length=255,.sid=255,.primary_phy=3,.ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
    show_hints(&row,&proof);assert(strstr(drawn,"FULL ORIGINAL NOT RETAINED")&&!strstr(drawn,"IBEACON ADVERTISED FORMAT"));
    save(prefix,"hints-truncated");
    assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW);paint_list();assert(strstr(drawn,"CALLBACK 255 B / PREVIEW 31 B"));
    hint_row(&row,flags,sizeof(flags));row.raw[0]=20;show_hints(&row,&proof);
    assert(strstr(drawn,"AD EVIDENCE NOT CLASSIFIABLE")&&!strstr(drawn,"NO SUPPORTED FAMILY HINT"));save(prefix,"hints-malformed");
    hint_row(&row,flags,sizeof(flags));row.raw_flags=CERBERUS_BLE_ADVERTISEMENT_MALFORMED;
    show_hints(&row,&proof);assert(strstr(drawn,"AD EVIDENCE NOT CLASSIFIABLE")&&!strstr(drawn,"COMPLETE AD NOT VALID"));
    assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(strstr(drawn,"FULL AD LAYOUT VALID")&&!strstr(drawn,"FULL AD LAYOUT INVALID"));
    hint_row(&row,flags,sizeof(flags));cerberus_dual_ble_provenance_t invalid=proof;invalid.epoch=0;
    show_hints(&row,&invalid);assert(strstr(drawn,"SOURCE PROOF UNAVAILABLE"));
    show_hints(&row,&proof);dual_ble_ui_open(true);
    assert(!dual_ble_ui_detail&&!dual_ble_ui_hints&&!dual_ble_ui_receive_info&&!dual_ble_ui_hint_pages&&!dual_ble_ui_hint_page);
    assert(companion_exchanges==exchanges&&starts==radio_starts&&stops==radio_stops&&companion_allocations==allocations);
    assert(!memcmp(&s3_before,&ble_selected_result,sizeof(s3_before)));
}

static void family_tick(uint8_t family,uint64_t now)
{
    clock_ms=(int64_t)now;unsigned calls=companion_exchanges;
    dual_ble_ui_sync_family(family,now);
    assert(companion_exchanges-calls<=4U);
}

static void check_full_original_hints(const char *prefix)
{
    static const uint8_t beacon[27]={26,0xff,0x4c,0,2,0x15,1,2,3,4};
    static const uint8_t findmy[]={3,3,0x44,0xfd},flipper[]={3,3,0x82,0x30};
    static const uint8_t flock[]={4,0xff,0xc8,9,1},meta[]={3,0xff,0x5f,0xfd};
    static const uint8_t hc[]={6,9,'H','C','-','0','5'};
    const struct {const uint8_t *data;size_t length;const char *label;} cases[]={
        {beacon,sizeof(beacon),"IBEACON"},{findmy,sizeof(findmy),"FIND MY"},
        {flipper,sizeof(flipper),"FLIPPER"},{flock,sizeof(flock),"FLOCK"},
        {meta,sizeof(meta),"META"},{hc,sizeof(hc),"HC NAME"}};
    unsigned exchanges=companion_exchanges,radio_starts=starts,radio_stops=stops;
    const cerberus_dual_ble_provenance_t proof={.boot_id=123,.epoch=456,.generation=0,.index=31};
    for(unsigned i=0;i<6;++i){
        uint8_t full[255]={0};size_t prefix_length=sizeof(full)-cases[i].length;
        full[0]=(uint8_t)(prefix_length-1U);full[1]=0xff;
        memcpy(full+prefix_length,cases[i].data,cases[i].length);
        cerberus_dual_ble_row_t row;
        hint_row(&row,full,31);row.raw_flags=3;
        row.receive=(cerberus_dual_ble_receive_metadata_t){
            .source_kind=CERBERUS_DUAL_BLE_RECEIVE_EXTENDED,.callback_length=255,
            .sid=0xff,.primary_phy=1,.ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
        cerberus_dual_ble_family_evidence_t evidence;
        assert(cerberus_dual_ble_family_parse_ad(full,sizeof(full),row.address,row.address_type,&evidence));
        assert(evidence.categories&(1U<<i));
        assert(cerberus_dual_ble_family_wire_encode(&evidence,(uint8_t)(i+1U),&row.family));
        show_hints(&row,&proof);
        assert(strstr(drawn,"C5 FULL ORIGINAL")&&strstr(drawn,"FULL 255 B / PREVIEW 31 B"));
        assert(strstr(drawn,cases[i].label)&&strstr(drawn,"SD SAVES PREVIEW + DECODED HINTS"));
        if(i==2U)save(prefix,"hints-full255-summary");
        cerberus_dual_ble_row_t frozen=dual_ble_ui_selected;
        memset(dual_ble_ui_store->rows,0,sizeof(dual_ble_ui_store->rows));
        ++dual_ble_ui_store->published_boot_id;++dual_ble_ui_store->published_epoch;
        memset(&row,0,sizeof(row));hint_next();
        assert(!memcmp(&frozen,&dual_ble_ui_selected,sizeof(frozen)));
        assert(!memcmp(&proof,&dual_ble_ui_selected_provenance,sizeof(proof)));
        if(i==2U){assert(strstr(drawn,"CODES: 3082"));save(prefix,"hints-full255-frozen-flipper");}
        assert(dual_ble_ui_tap(20,210)==DUAL_BLE_UI_REDRAW);paint_list();
        assert(strstr(drawn,"ORIGINAL ADV 31 B")&&!strstr(drawn,"FULL 255 B"));
        assert(dual_ble_ui_tap(200,210)==DUAL_BLE_UI_REDRAW);paint_list();
        assert(strstr(drawn,"CALLBACK 255 B / PREVIEW 31 B")&&strstr(drawn,"FULL AD LAYOUT VALID / PREVIEW CUT"));
        frozen.family.version=0xff;show_hints(&frozen,&proof);
        assert(!strstr(drawn,"COMPLETE REPORT")&&!strstr(drawn,"C5 FULL ORIGINAL"));
        memset(&frozen.family,0,sizeof(frozen.family));show_hints(&frozen,&proof);
        assert(strstr(drawn,"FULL ORIGINAL NOT RETAINED"));
    }
    /* A dense full report exercises the longest family summary plus blocked
     * Meta detail without replacing any of the frozen raw bytes. */
    uint8_t dense[255]={0};
    const uint8_t mixed[]={7,3,0x44,0xfd,0x81,0x30,0x5f,0xfd,6,9,'H','C','-','0','5'};
    const uint8_t raven[]={5,3,0,0x31,0,0x32};
    size_t at=sizeof(dense)-sizeof(beacon)-sizeof(mixed)-sizeof(raven);
    dense[0]=(uint8_t)(at-1U);dense[1]=0xff;
    memcpy(dense+at,beacon,sizeof(beacon));at+=sizeof(beacon);
    memcpy(dense+at,mixed,sizeof(mixed));at+=sizeof(mixed);memcpy(dense+at,raven,sizeof(raven));
    cerberus_dual_ble_row_t dense_row;hint_row(&dense_row,dense,31);dense_row.raw_flags=3;
    dense_row.receive=(cerberus_dual_ble_receive_metadata_t){
        .source_kind=CERBERUS_DUAL_BLE_RECEIVE_EXTENDED,.callback_length=255,
        .sid=255,.primary_phy=1,.ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
    cerberus_dual_ble_family_evidence_t dense_evidence;
    assert(cerberus_dual_ble_family_parse_ad(dense,sizeof(dense),dense_row.address,dense_row.address_type,&dense_evidence));
    assert(cerberus_dual_ble_family_wire_encode(&dense_evidence,1,&dense_row.family));
    show_hints(&dense_row,&proof);
    assert(strstr(drawn,"IBEACON FIND MY FLIPPER FLOCK HC NAME")&&strstr(drawn,"FULL 255 B / PREVIEW 31 B / 5 HINTS"));
    assert(dual_ble_ui_hint_pages==7);save(prefix,"hints-full255-multiple");
    for(unsigned i=0;i<5;++i)hint_next();
    assert(strstr(drawn,"META HINT SUPPRESSED BY ID RULE")&&strstr(drawn,"ID 004C / MFG16"));
    save(prefix,"hints-full255-blocked-meta");
    assert(companion_exchanges==exchanges&&starts==radio_starts&&stops==radio_stops);
}

static void check_focused_lifecycle(const char *prefix)
{
    unsigned radio_starts=starts,radio_stops=stops;
    assert(!dual_ble_ui_store);
    dual_ble_ui_mode=-1;dual_ble_ui_family=0;dual_ble_ui_attempted=false;
    family_peer_enabled=family_peer_supported=true;family_peer_next_job=1;
    family_tick(1,100);family_tick(1,101);
    assert(family_peer_starts==1&&family_peer_attached&&dual_ble_ui_store->start.mode==CERBERUS_DUAL_BLE_FAMILY);
    cerberus_dual_ble_view_t view;dual_ble_ui_get_view(&view);assert(view.requested_family==1);
    cerberus_dual_ble_row_t row={.address={1,2,3,4,5,6},.address_type=2};
    const cerberus_dual_ble_provenance_t proof={.boot_id=11,.epoch=22,.index=3};
    strcpy(row.name,"Retained before family change");dual_ble_ui_select_frozen(&row,&proof);
    family_peer_hold_stop=true;
    family_tick(2,110);family_tick(2,500);family_tick(3,13000);
    assert(family_peer_starts==1&&family_peer_stop_requests>=3&&family_peer_attached);
    assert(dual_ble_ui_family==3&&!dual_ble_ui_attempted&&!dual_ble_ui_released());
    assert(!memcmp(&row,&dual_ble_ui_selected,sizeof(row))&&
        !memcmp(&proof,&dual_ble_ui_selected_provenance,sizeof(proof)));
    family_peer_hold_stop=false;family_peer_lost_release=1;
    for(uint64_t now=14000;now<18000&&family_peer_starts<2;now+=250)family_tick(3,now);
    assert(family_peer_starts==2&&family_peer_releases==1&&family_peer_attached);
    dual_ble_ui_get_view(&view);assert(view.requested_family==3);
    assert(!memcmp(&row,&dual_ble_ui_selected,sizeof(row)));
    /* STOP/Back/sleep all submit mode -1; none may rearm a pending family. */
    family_peer_hold_stop=true;family_tick(4,18000);
    clock_ms=18001;dual_ble_ui_sync(-1,NULL,0,18001);assert(family_peer_starts==2);
    family_peer_hold_stop=false;
    for(uint64_t now=18250;now<20000&&!dual_ble_ui_released();now+=250){
        clock_ms=(int64_t)now;
        dual_ble_ui_sync(-1,NULL,0,now);
    }
    assert(dual_ble_ui_released()&&!family_peer_attached&&family_peer_starts==2);
    family_tick(7,20000);assert(dual_ble_ui_mode==-1&&family_peer_starts==2);
    family_peer_supported=false;family_tick(2,21000);family_tick(2,21001);
    assert(!family_peer_attached&&family_peer_starts==2&&dual_ble_ui_released());
    dual_ble_ui_get_view(&view);assert(view.error==ESP_ERR_NOT_SUPPORTED&&view.requested_family==2);
    dual_ble_ui_detail=false;paint_list();
    assert(strstr(drawn,"FOCUS UNAVAILABLE / S3 VIEW AVAILABLE")&&strstr(drawn,"C5 FOCUSED / 0 OF 0 RETAINED"));
    assert(strstr(drawn,"NO C5 RESULTS / FOCUS UNAVAILABLE")&&!strstr(drawn,"C5 LIVE"));save(prefix,"focus-unavailable");
    unsigned calls=companion_exchanges;
    family_tick(2,22000);family_tick(2,23000);assert(companion_exchanges==calls);
    family_tick(0,24000);family_tick(0,24001);
    assert(family_peer_starts==3&&family_peer_start.mode==CERBERUS_DUAL_BLE_DISCOVERY);
    clock_ms=24002;dual_ble_ui_sync(-1,NULL,0,24002);assert(dual_ble_ui_released()&&!family_peer_attached);
    assert(starts==radio_starts&&stops==radio_stops);
    family_peer_enabled=false;heap_caps_free(dual_ble_ui_store);dual_ble_ui_store=NULL;
}

int main(int argc,char **argv)
{
    check_list_order();
    assert(argc==2);clock_ms=100;dual_ble_ui_sync(-1,NULL,0,100);
    assert(!dual_ble_ui_store&&dual_ble_ui_released());
    fail_alloc=true;dual_ble_ui_sync(0,NULL,0,100);assert(!dual_ble_ui_store&&dual_ble_ui_error==ESP_ERR_NO_MEM);
    fail_alloc=false;dual_ble_ui_retry();dual_ble_ui_sync(0,NULL,0,101);
    assert(dual_ble_ui_store);clock_ms=102;dual_ble_ui_sync(0,NULL,0,102);
    assert(dual_ble_ui_store->error==ESP_ERR_NOT_SUPPORTED&&companion_exchanges==1);
    clock_ms=200;dual_ble_ui_sync(0,NULL,0,200);assert(companion_exchanges==1); /* Explicit retry only. */
    dual_ble_ui_store->error=0;dual_ble_ui_store->count=6;
    dual_ble_ui_store->published_boot_id=7;dual_ble_ui_store->published_epoch=8;
    dual_ble_ui_store->published_generation=9;
    for(unsigned i=0;i<6;++i){
        cerberus_dual_ble_row_t *r=&dual_ble_ui_store->rows[i];
        memset(r,0,sizeof(*r));memcpy(r->address,"ABCDEF",6);r->address_type=(uint8_t)(i%4);
        snprintf(r->name,sizeof(r->name),"Companion %u",i);r->rssi=-70-(int)i;
        r->callback_age_ms=100;r->request_started_ms=100;r->timing_flags=3;
        r->raw_length=3;r->raw[0]=2;r->raw[1]=1;r->raw[2]=6;r->flags=CERBERUS_BLE_RESULT_CONNECTABLE;
    }
    memset(&ble_selected_result,0x55,sizeof(ble_selected_result));
    cerberus_c5_ble_result_t unchanged=ble_selected_result;
    dual_ble_ui_open(true);drawn[0]=0;
    /* The production draw_detail_page clears the canvas before this helper. */
    assert(cerberus_display_fill_rect(0,0,320,480,UI_BACKGROUND)==ESP_OK);
    assert(dual_ble_ui_draw(true)==ESP_OK);
    assert(strstr(drawn,"C5 WHOLE SWEEP")&&strstr(drawn,"C5 -70 dBm"));
    assert(strstr(drawn,"STOP BOTH RECEIVERS")&&!strstr(drawn,"BEEP"));save(argv[1],"list");
    dual_ble_ui_store->rows[0].address[0]=99;
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW);
    cerberus_c5_ble_result_t target;assert(dual_ble_ui_selected_peer(&target));
    assert(target.address[0]=='A'&&target.address_type==0&&target.flags==0&&target.uuid_length==0);
    assert(!memcmp(&unchanged,&ble_selected_result,sizeof(unchanged)));
    drawn[0]=0;assert(cerberus_display_fill_rect(0,0,320,480,UI_BACKGROUND)==ESP_OK);
    assert(dual_ble_ui_draw(true)==ESP_OK);
    assert(strstr(drawn,"ORIGINAL ADV 3 B")&&strstr(drawn,"02 01 06"));
    assert(strstr(drawn,"AGE <= 200 ms"));save(argv[1],"detail");
    assert(dual_ble_ui_tap(200,330)==DUAL_BLE_UI_FOX);
    assert(dual_ble_ui_tap(100,390)==DUAL_BLE_UI_TOGGLE);
    assert(dual_ble_ui_tap(100,65)==DUAL_BLE_UI_BACK_S3);
    assert(dual_ble_ui_tap(20,330)==DUAL_BLE_UI_REDRAW);
    assert(dual_ble_ui_tap(200,340)==DUAL_BLE_UI_REDRAW);drawn[0]=0;
    assert(dual_ble_ui_draw(false)==ESP_OK&&dual_ble_ui_painted_count==2);
    assert(strstr(drawn,"Companion 4")&&!strstr(drawn,"Companion 0"));
    assert(dual_ble_ui_tap(20,340)==DUAL_BLE_UI_REDRAW);
    dual_ble_ui_mode=CERBERUS_DUAL_BLE_TRACK;memcpy(dual_ble_ui_target,target.address,6);dual_ble_ui_type=0;
    dual_ble_ui_store->start.mode=1;memcpy(dual_ble_ui_store->start.address,target.address,6);
    dual_ble_ui_store->start.address_type=0;dual_ble_ui_store->track_request_ms=100;
    dual_ble_ui_store->has_track=true;dual_ble_ui_store->track=(cerberus_dual_ble_track_t){.age_ms=50,.rssi=-81,.flags=3};
    drawn[0]=0;assert(dual_ble_ui_draw_fox(280,200)==ESP_OK);
    assert(strstr(drawn,"C5 -81 dBm / AGE <= 150 ms"));
    ++dual_ble_ui_target[0];drawn[0]=0;assert(dual_ble_ui_draw_fox(280,200)==ESP_OK);
    assert(strstr(drawn,"WAITING")&&!strstr(drawn,"-81")); /* Never relabel an old target sample. */
    dual_ble_ui_mode=-1;drawn[0]=0;assert(dual_ble_ui_draw_fox(280,200)==ESP_OK);
    assert(!strstr(drawn,"-81"));
    /* Actual complete Fox drawing: a newly selected target must not inherit
     * the still-draining S3 tracker's old RSSI, report count or green meter. */
    ble_fox_hunt_target=target;strcpy(ble_fox_hunt_target.name,"NEW TARGET");
    fox_sample=(cerberus_s3_ble_track_status_t){.enabled=true,.seen=true,.fresh=true,
        .address={99,88,77,66,55,44},.address_type=2,.rssi_dbm=-31,.age_ms=15,.reports=777};
    ble_fox_hunt_start_pending=true;drawn[0]=0;assert(draw_ble_fox_hunt()==ESP_OK);
    assert(fox_status_reads==0&&strstr(drawn,"NEW TARGET")&&strstr(drawn,"RELEASING SCANNERS"));
    assert(strstr(drawn,"WAITING FOR THIS EXACT ADDRESS")&&strstr(drawn,"S3 MATCHED REPORTS 0"));
    assert(!strstr(drawn,"-31")&&!strstr(drawn,"777"));
    for(unsigned y=204;y<230;++y)for(unsigned x=20;x<300;++x)assert(screen[y*320+x]==UI_PANEL);
    save(argv[1],"fox-pending-new-target");
    ble_fox_hunt_error=ESP_ERR_TIMEOUT;drawn[0]=0;assert(draw_ble_fox_hunt()==ESP_OK);
    assert(fox_status_reads==0&&!strstr(drawn,"-31")&&!strstr(drawn,"777"));
    ble_fox_hunt_error=ESP_OK;ble_fox_hunt_start_pending=false;
    memcpy(fox_sample.address,target.address,6);fox_sample.address_type=target.address_type;
    fox_sample.rssi_dbm=-65;fox_sample.reports=3;drawn[0]=0;assert(draw_ble_fox_hunt()==ESP_OK);
    assert(fox_status_reads==1&&strstr(drawn,"S3 -65 dBm")&&strstr(drawn,"S3 MATCHED REPORTS 3"));
    assert(screen[204*320+20]==UI_GOOD&&screen[204*320+160]==UI_PANEL);
    save(argv[1],"fox-bound-new-target");
    check_list_controls(argv[1]);
    check_reader_metrics(argv[1]);
    check_receive_metadata(argv[1]);
    check_paint_failure_and_unified_selection(argv[1]);
    check_family_hints(argv[1]);
    check_full_original_hints(argv[1]);
    heap_caps_free(dual_ble_ui_store);dual_ble_ui_store=NULL;
    check_focused_lifecycle(argv[1]);
    puts("Dual BLE UI: frozen typed rows, raw/receive/hints tabs, all family evidence and complete Meta paging without RF/allocation, unavailable/nohint distinction, source labels, reader metrics and old-target isolation passed");
    return 0;
}
