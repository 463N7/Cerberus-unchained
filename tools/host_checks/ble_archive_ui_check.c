#include "archive_framebuffer.h"
#define UI_BACKGROUND 0x0042
#define UI_PANEL 0x08A4
#define UI_MUTED 0x7CB5
#define UI_TEXT 0xE77F
#define TAG "host"
static unsigned checks;
static bool privacy_mask;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); abort(); } } while (0)
static bool privacy_addresses_masked(void) { return privacy_mask; }
static void cerberus_privacy_format_ble_identifier(const uint8_t *a,bool mask,char *out,size_t n)
{
    if(mask) snprintf(out,n,"XX:XX:XX:XX:XX:XX");
    else snprintf(out,n,"%02X:%02X:%02X:%02X:%02X:%02X",a[5],a[4],a[3],a[2],a[1],a[0]);
}
static const char *esp_err_to_name(esp_err_t error)
{
    return error==ESP_ERR_NO_MEM?"ESP_ERR_NO_MEM":error==ESP_ERR_INVALID_RESPONSE?
        "ESP_ERR_INVALID_RESPONSE":error==ESP_ERR_INVALID_STATE?"ESP_ERR_INVALID_STATE":"ESP_FAIL";
}
static esp_err_t draw_centered(uint16_t y,const char *text,uint16_t color,uint8_t scale)
{ CHECK(strlen(text)*6U*scale<=320U); return cerberus_display_draw_text((320U-strlen(text)*6U*scale)/2U,y,text,color,UI_BACKGROUND,scale); }
static esp_err_t draw_header(const char *text) { return draw_centered(20,text,UI_TEXT,2); }
static esp_err_t draw_button(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const char *text)
{
    CHECK(strlen(text)*6U<=w);
    esp_err_t error=cerberus_display_fill_rect(x,y,w,h,UI_PANEL);
    return error==ESP_OK?cerberus_display_draw_text(x+(w-strlen(text)*6U)/2U,y+(h-7U)/2U,text,UI_TEXT,UI_PANEL,1):error;
}
#include "ble_archive_ui.h"
static ble_archive_ui_store_t viewer;
static cerberus_ble_archive_t source;

static void clear_draw(void) { calls=fail_after=0; drawn_size=0; drawn[0]=0; }
static void seed(unsigned count)
{
    memset(&viewer,0,sizeof(viewer)); memset(&source,0,sizeof(source));
    source.saved_uptime_ms=1000000; source.count=(uint8_t)count;
    for(unsigned i=0;i<count;++i) {
        cerberus_ble_archive_row_t *r=&source.rows[i];
        r->address[0]=(uint8_t)(i+1U);r->address[5]=0xC0;r->address_type=1;
        snprintf(r->name,sizeof(r->name),"Saved advertiser %u",i+1U);
        r->rssi=-50;r->rssi_valid=true;r->manufacturer_id=0xFFFF;
        r->source_time_valid=true;r->original_source_uptime_ms=999000;r->copy_ceiling_ms=999500;
        r->scan_epoch_valid=true;r->scan_epoch=123;r->generation=4;r->index=(uint8_t)i;r->reports_seen=5;
        const uint8_t data[]={2,1,6,3,3,0x0F,0x18,4,0x09,'t','a','g',3,0xFF,0x4C,0};
        memcpy(r->advertisement,data,sizeof(data));r->advertisement_length=sizeof(data);
    }
    CHECK(cerberus_ble_archive_validate(&source));privacy_mask=false;clear_draw();
}
static void load(unsigned count)
{
    seed(count);CHECK(ble_archive_ui_begin(&viewer,1,false));
    CHECK(ble_archive_ui_complete(&viewer,1,&source,"B00001.BLE"));
}
static void preview(const char *directory,const char *name)
{
    char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    FILE *f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n320 480\n255\n");
    for(size_t i=0;i<sizeof(screen)/sizeof(screen[0]);++i) {
        uint16_t p=screen[i];uint8_t rgb[]={(uint8_t)((p>>11)*255/31),(uint8_t)(((p>>5)&63)*255/63),(uint8_t)((p&31)*255/31)};
        assert(fwrite(rgb,1,3,f)==3);
    }
    CHECK(fclose(f)==0);
}
static void test_jobs(const char *directory)
{
    seed(5); CHECK(!ble_archive_ui_begin(NULL,1,false));CHECK(!ble_archive_ui_begin(&viewer,0,false));
    CHECK(ble_archive_ui_begin(&viewer,1,true));CHECK(!ble_archive_ui_begin(&viewer,2,false));
    CHECK(!ble_archive_ui_error(&viewer,ESP_ERR_NO_MEM));CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"SAVING SNAPSHOT")&&strstr(drawn,"A SAVE MAY FINISH"));preview(directory,"archive-saving");
    CHECK(!ble_archive_ui_complete(&viewer,2,&source,"B00001.BLE"));
    CHECK(!ble_archive_ui_fail(&viewer,2,ESP_FAIL));CHECK(!ble_archive_ui_fail(&viewer,1,ESP_OK));
    CHECK(ble_archive_ui_tap(&viewer,64,426)==BLE_ARCHIVE_UI_CANCEL&&!viewer.job_pending);
    CHECK(!ble_archive_ui_complete(&viewer,1,&source,"late.BLE"));
    CHECK(!ble_archive_ui_fail(&viewer,1,ESP_FAIL));CHECK(!ble_archive_ui_begin(&viewer,1,false));
    CHECK(ble_archive_ui_begin(&viewer,2,false));clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"LOADING SNAPSHOT")&&strstr(drawn,"BACK CANCELS THIS LOAD"));
    CHECK(ble_archive_ui_complete(&viewer,2,&source,"B00002.BLE"));
    CHECK(!ble_archive_ui_complete(&viewer,2,&source,"duplicate.BLE"));
    CHECK(ble_archive_ui_begin(&viewer,3,false));
    cerberus_ble_archive_t prior=viewer.document;
    source.count=0;CHECK(!ble_archive_ui_complete(&viewer,3,&source,"invalid.BLE"));
    CHECK(viewer.view==BLE_ARCHIVE_UI_ERROR&&viewer.error==ESP_ERR_INVALID_RESPONSE);
    CHECK(!memcmp(&prior,&viewer.document,sizeof(prior))&&viewer.has_document);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"PREVIOUS SNAPSHOT RETAINED")&&strstr(drawn,"ESP_ERR_INVALID_RESPONSE"));preview(directory,"archive-error");
    CHECK(ble_archive_ui_tap(&viewer,64,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.view==BLE_ARCHIVE_UI_LIST);
    CHECK(ble_archive_ui_error(&viewer,ESP_ERR_NO_MEM));CHECK(!memcmp(&prior,&viewer.document,sizeof(prior)));
    CHECK(ble_archive_ui_begin(&viewer,UINT64_MAX,false));CHECK(ble_archive_ui_fail(&viewer,UINT64_MAX,ESP_FAIL));
    CHECK(!ble_archive_ui_begin(&viewer,1,false));
    clear_draw();CHECK(ble_archive_ui_draw(NULL,ESP_ERR_NO_MEM)==ESP_OK);
    CHECK(strstr(drawn,"ARCHIVE UNAVAILABLE")&&strstr(drawn,"ESP_ERR_NO_MEM")&&!strstr(drawn,"0 OBSERVATIONS"));
    CHECK(ble_archive_ui_tap(NULL,64,426)==BLE_ARCHIVE_UI_BACK&&ble_archive_ui_tap(NULL,8,150)==BLE_ARCHIVE_UI_NONE);
    preview(directory,"archive-unavailable");
}
static void test_list(const char *directory)
{
    load(32);cerberus_ble_archive_t prior=viewer.document;
    CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&viewer.paint_valid&&viewer.painted_count==4);
    CHECK(!memcmp(&prior,&viewer.document,sizeof(prior)));CHECK(strstr(drawn,"PAGE 1 OF 8")&&strstr(drawn,"AGE AT SAVE 1000ms"));
    preview(directory,"archive-list");
    const uint16_t gaps[][2]={{7,150},{312,150},{8,200},{8,203},{8,254},{8,308},{8,362},{156,374},{163,374},{8,418},{63,426},{256,426},{64,470}};
    for(unsigned i=0;i<sizeof(gaps)/sizeof(gaps[0]);++i) CHECK(ble_archive_ui_tap(&viewer,gaps[i][0],gaps[i][1])==BLE_ARCHIVE_UI_NONE);
    for(unsigned p=1;p<8;++p) {
        CHECK(ble_archive_ui_tap(&viewer,164,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.page==p&&!viewer.paint_valid);
        CHECK(ble_archive_ui_tap(&viewer,8,150)==BLE_ARCHIVE_UI_NONE);clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
        CHECK(viewer.painted[0].address[0]==p*4U+1U);
    }
    CHECK(ble_archive_ui_tap(&viewer,164,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.page==7);
    for(unsigned p=7;p>0;--p) CHECK(ble_archive_ui_tap(&viewer,8,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.page==p-1U);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    cerberus_ble_archive_row_t painted=viewer.painted[0];
    viewer.document.rows[0]=viewer.document.rows[1];viewer.document.saved_uptime_ms+=500000;
    CHECK(ble_archive_ui_tap(&viewer,8,150)==BLE_ARCHIVE_UI_REDRAW&&viewer.view==BLE_ARCHIVE_UI_DETAIL);
    CHECK(!memcmp(&painted,&viewer.selected,sizeof(painted)));CHECK(viewer.selected_saved_uptime_ms==1000000&&viewer.selected_age_ms==1000);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"C0:00:00:00:00:01")&&strstr(drawn,"AGE AT SAVE: 1000 ms"));preview(directory,"archive-detail");
    CHECK(ble_archive_ui_tap(&viewer,64,426)==BLE_ARCHIVE_UI_REDRAW&&viewer.view==BLE_ARCHIVE_UI_LIST);
    CHECK(ble_archive_ui_tap(&viewer,64,426)==BLE_ARCHIVE_UI_BACK);
    load(5);CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);CHECK(ble_archive_ui_tap(&viewer,164,374)==BLE_ARCHIVE_UI_REDRAW);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&viewer.painted_count==1);
    CHECK(ble_archive_ui_tap(&viewer,8,204)==BLE_ARCHIVE_UI_NONE);
}
static void test_paint_and_privacy(void)
{
    load(4);CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);unsigned total=calls;
    for(unsigned failure=1;failure<=total;++failure) {
        clear_draw();fail_after=failure;CHECK(ble_archive_ui_draw(&viewer,ESP_OK)!=ESP_OK);
        CHECK(!viewer.paint_valid&&ble_archive_ui_tap(&viewer,8,150)==BLE_ARCHIVE_UI_NONE);
    }
    clear_draw();privacy_mask=true;CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"XX:XX:XX:XX:XX:XX")&&!strstr(drawn,"C0:00:00:00:00:01"));
    CHECK(ble_archive_ui_tap(&viewer,8,150)==BLE_ARCHIVE_UI_REDRAW);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"XX:XX:XX:XX:XX:XX")&&!strstr(drawn,"C0:00:00:00:00:01"));
    for(unsigned type=0;type<4;++type)for(unsigned bits=0;bits<4;++bits) {
        viewer.selected.address_type=(uint8_t)type;viewer.selected.address[5]=(uint8_t)(bits<<6);
        clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    }
    memset(viewer.selected.name,'\xFE',32);viewer.selected.name[32]=0;
    viewer.selected.rssi_valid=false;viewer.selected.source_time_valid=false;viewer.selected.scan_epoch_valid=false;
    viewer.selected_age_valid=false;viewer.selected_saved_uptime_ms=UINT64_MAX;
    viewer.selected.uuid_length=16;memset(viewer.selected.uuid,0xFF,16);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"FEFEFEFEFEFEFEFEFEFEFEFEFEFEFEFE")&&strstr(drawn,"AGE AT SAVE: UNKNOWN"));
    CHECK(strstr(drawn,"RSSI UNAVAILABLE")&&strstr(drawn,"NO LOCATION OR WALL CLOCK"));
}
static void test_raw(const char *directory)
{
    load(1);CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);CHECK(ble_archive_ui_tap(&viewer,8,150)==BLE_ARCHIVE_UI_REDRAW);
    CHECK(ble_archive_ui_tap(&viewer,64,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.view==BLE_ARCHIVE_UI_RAW);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"ORIGINAL ADV ONLY / NO RSP")&&strstr(drawn,"FLAGS")&&strstr(drawn,"UUID16 LIST"));
    CHECK(strstr(drawn,"02 01 06 03 03 0F 18")&&strstr(drawn,"4 COMPLETE FIELDS / PAGE 1 OF 2"));
    preview(directory,"archive-raw");
    CHECK(ble_archive_ui_tap(&viewer,164,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.raw_page==1);
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&strstr(drawn,"MANUFACTURER DATA"));
    CHECK(ble_archive_ui_tap(&viewer,8,374)==BLE_ARCHIVE_UI_REDRAW&&viewer.raw_page==0);
    viewer.selected.advertisement_length=31;memset(viewer.selected.advertisement,0,31);
    for(unsigned i=0;i<30;i+=2){viewer.selected.advertisement[i]=1;viewer.selected.advertisement[i+1]=0xFE;}
    for(unsigned p=0;p<5;++p) {
        clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&strstr(drawn,"15 COMPLETE FIELDS"));
        CHECK(strstr(drawn,"UNKNOWN TYPE"));CHECK(ble_archive_ui_tap(&viewer,164,374)==BLE_ARCHIVE_UI_REDRAW);
        CHECK(viewer.raw_page==(p<4?p+1U:4U));
    }
    memset(viewer.selected.advertisement,0xFF,31);viewer.selected.advertisement[0]=30;viewer.raw_page=0;
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&strstr(drawn,"1 COMPLETE FIELDS"));
    viewer.selected.advertisement[0]=31;viewer.selected.advertisement_flags=0x83;
    clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK);
    CHECK(strstr(drawn,"MALFORMED OR CUT AD AT BYTE 0")&&strstr(drawn,"NO COMPLETE AD FIELDS")&&strstr(drawn,"RAW FLAGS 83"));
    CHECK(strstr(drawn,"SOURCE: TRUNCATED + MALFORMED"));
    preview(directory,"archive-malformed");
    viewer.selected.advertisement_flags=1;clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&strstr(drawn,"SOURCE: TRUNCATED / MISSING BYTES UNKNOWN"));
    viewer.selected.advertisement_flags=2;clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&strstr(drawn,"SOURCE MARKED MALFORMED / RAW RETAINED"));
    uint8_t offsets[15],bad;viewer.selected.advertisement_length=3;
    viewer.selected.advertisement[0]=0;viewer.selected.advertisement[1]=1;viewer.selected.advertisement[2]=0;
    CHECK(ble_archive_ui_fields(&viewer.selected,offsets,&bad)==0&&bad==0);
    viewer.selected.advertisement[1]=0;CHECK(ble_archive_ui_fields(&viewer.selected,offsets,&bad)==0&&bad==UINT8_MAX);
    viewer.selected.advertisement_length=0;clear_draw();CHECK(ble_archive_ui_draw(&viewer,ESP_OK)==ESP_OK&&strstr(drawn,"NO COMPLETE AD FIELDS"));
    CHECK(ble_archive_ui_tap(&viewer,64,426)==BLE_ARCHIVE_UI_REDRAW&&viewer.view==BLE_ARCHIVE_UI_DETAIL);
    CHECK(ble_archive_ui_tap(&viewer,64,426)==BLE_ARCHIVE_UI_REDRAW&&viewer.view==BLE_ARCHIVE_UI_LIST);
    CHECK(ble_archive_ui_tap(&viewer,64,426)==BLE_ARCHIVE_UI_BACK);
}
int main(int argc,char **argv)
{
    CHECK(argc==2);test_jobs(argv[1]);test_list(argv[1]);test_paint_and_privacy();test_raw(argv[1]);
    printf("BLE archive UI PASS %u assertions; no clock, heap, SD, IO worker or radio linked\n",checks);
    return 0;
}
