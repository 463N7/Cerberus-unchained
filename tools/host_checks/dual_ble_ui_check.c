#define main inherited_monitor_suite
#include "s3_findmy_monitor_ui_check.c"
#undef main
static esp_err_t draw_button(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const char *s)
{return draw_small_button(x,y,w,h,s);}
static unsigned companion_exchanges;
esp_err_t c5_spi_link_dual_ble_exchange(uint8_t cmd,const void *q,size_t qn,void *r,size_t rn)
{(void)cmd;(void)q;(void)qn;memset(r,0,rn);++companion_exchanges;return ESP_ERR_NOT_SUPPORTED;}
#include "dual_ble_ui.h"
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
    for(unsigned i=0;i<6;++i){
        cerberus_dual_ble_row_t *r=&dual_ble_ui_store->rows[i];memset(r,0,sizeof(*r));
        memcpy(r->address,"ABCDEF",6);r->address_type=(uint8_t)(i%4);r->rssi=-60-(int)i;
        snprintf(r->name,sizeof(r->name),"Peer %u",5U-i);r->request_started_ms=1000;r->timing_flags=3;
        r->raw_length=3;r->raw[0]=2;r->raw[1]=1;r->raw[2]=(uint8_t)i;
    }
    paint_list();assert(strstr(drawn,"SIGNAL >")&&strstr(drawn,"ALL AGES >"));
    assert(!strcmp(dual_ble_ui_painted[0].name,"Peer 5"));
    assert(dual_ble_ui_tap(40,120)==DUAL_BLE_UI_REDRAW);paint_list();
    assert(dual_ble_ui_sort==CERBERUS_DUAL_BLE_SORT_NAME&&!strcmp(dual_ble_ui_painted[0].name,"Peer 0"));
    save(prefix,"name-sorted");
    /* A bank replacement after paint must never redirect the selected source. */
    memset(dual_ble_ui_store->rows,0,sizeof(dual_ble_ui_store->rows));dual_ble_ui_store->count=0;
    assert(dual_ble_ui_tap(20,160)==DUAL_BLE_UI_REDRAW);
    cerberus_c5_ble_result_t target;assert(dual_ble_ui_selected_peer(&target));
    assert(!strcmp(target.name,"Peer 0")&&target.address_type==1&&!memcmp(target.address,"ABCDEF",6));
    assert(dual_ble_ui_selected.raw[2]==5&&target.flags==0);
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
    masked=true;paint_list();assert(!strstr(drawn,"46:45:44:43:42:41")&&strstr(drawn,"BLE-"));masked=false;
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
    assert(strstr(drawn,"C5 PASSIVE")&&strstr(drawn,"C5 -70 dBm"));
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
    heap_caps_free(dual_ble_ui_store);dual_ble_ui_store=NULL;
    puts("Dual BLE UI: optional PSRAM/error isolation, bounded stable signal/name/scan ordering, conservative age filtering, expiry/pagination, frozen typed rows, no radio calls on controls, source-only raw preview, and no old-target S3/C5 samples while draining passed");
    return 0;
}
