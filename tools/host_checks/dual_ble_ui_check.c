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
int main(int argc,char **argv)
{
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
    assert(strstr(drawn,"C5 PASSIVE RECEIVER")&&strstr(drawn,"C5 -70 dBm"));
    assert(strstr(drawn,"STOP BOTH RECEIVERS")&&!strstr(drawn,"BEEP"));save(argv[1],"list");
    dual_ble_ui_store->rows[0].address[0]=99;
    assert(dual_ble_ui_tap(20,140)==DUAL_BLE_UI_REDRAW);
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
    heap_caps_free(dual_ble_ui_store);dual_ble_ui_store=NULL;
    puts("Dual BLE UI: optional PSRAM/error isolation, frozen typed rows, source-only raw preview, no S3 action mutation and no old-target S3/C5 RSSI/meter/count while draining passed");
    return 0;
}
