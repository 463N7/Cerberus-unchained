#include "archive_live_adapters.h"
#include "ble_archive_io.h"
#include "s3_ble_discovery_inventory.h"
#include "s3_findmy_monitor.h"
#define ESP_ERROR_CHECK(x) CHECK((x) == ESP_OK)
#define MALLOC_CAP_SPIRAM 1U
#define MALLOC_CAP_8BIT 2U
typedef enum { UI_PAGE_BLE, UI_PAGE_FILES, UI_PAGE_BLE_ARCHIVE, UI_PAGE_MAIN } ui_page_t;
typedef enum { UI_BLE_MODE_MENU, UI_BLE_MODE_DETAIL } ui_ble_mode_t;
enum { UI_FILE_MODE_LIST };
static ui_page_t current_page;
static ui_ble_mode_t ble_mode;
static int file_mode;
static bool ble_selection_valid, ble_selected_raw_frozen, ble_selected_original_time64_valid;
static uint64_t ble_selected_original_ms64, ble_selected_source_ceiling_ms64;
static cerberus_c5_ble_result_t ble_selected_result;
static cerberus_findmy_monitor_record_t ble_findmy_selected;
typedef struct { cerberus_s3_ble_discovery_inventory_t inventory; } ble_discovery_live_store_t;
static ble_discovery_live_store_t discovery;
static ble_discovery_live_store_t *ble_discovery_live;
static int64_t now_us;
static int64_t esp_timer_get_time(void) { return now_us; }
static void *allocation;
static unsigned allocations, frees, saves, loads, copies, cancels, releases, repaints;
static bool allocation_fails;
static esp_err_t start_error, copy_error;
static uint64_t sequence;
static uint64_t last_canceled_job, last_released_job;
static cerberus_ble_archive_io_status_t io;
static cerberus_ble_archive_t saved;
static void *heap_caps_calloc(size_t count,size_t size,unsigned caps)
{ CHECK(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));CHECK(!allocation);++allocations;
  if(allocation_fails)return NULL;allocation=calloc(count,size);CHECK(allocation);return allocation; }
static void heap_caps_free(void *p) { CHECK(p&&p==allocation);++frees;free(p);allocation=NULL; }
static esp_err_t draw_detail_page(void);
static esp_err_t draw_current_page(void);
static void open_tool_page(ui_page_t page) { current_page=page; }
esp_err_t cerberus_ble_archive_io_save(const cerberus_ble_archive_t *doc,uint64_t *job)
{ ++saves;*job=0;if(start_error)return start_error;CHECK(cerberus_ble_archive_validate(doc));saved=*doc;
  io=(cerberus_ble_archive_io_status_t){.job_id=++sequence,.worker_active=true,.state=CERBERUS_BLE_ARCHIVE_IO_SAVING};*job=sequence;return ESP_OK; }
esp_err_t cerberus_ble_archive_io_load(const char *path,const char *name,uint64_t *job)
{ ++loads;*job=0;CHECK(path&&name);if(start_error)return start_error;
  io=(cerberus_ble_archive_io_status_t){.job_id=++sequence,.worker_active=true,.state=CERBERUS_BLE_ARCHIVE_IO_LOADING};*job=sequence;return ESP_OK; }
void cerberus_ble_archive_io_get_status(cerberus_ble_archive_io_status_t *out) { *out=io; }
esp_err_t cerberus_ble_archive_io_copy_result(uint64_t job,cerberus_ble_archive_t *out)
{ ++copies;CHECK(job==io.job_id&&!io.worker_active&&io.result_ready);if(copy_error)return copy_error;*out=saved;return ESP_OK; }
void cerberus_ble_archive_io_cancel(uint64_t job) { CHECK(job);++cancels;last_canceled_job=job;if(job==io.job_id)io.cancel_requested=true; }
void cerberus_ble_archive_io_release(uint64_t job) { CHECK(job);++releases;last_released_job=job;if(job==io.job_id)io.result_ready=false; }
#include "ble_archive_live_ui.h"
static esp_err_t draw_detail_page(void) { drawn_size=0;drawn[0]=0;calls=0;++repaints;return ble_archive_draw(); }
static esp_err_t draw_current_page(void) { ++repaints;return current_page==UI_PAGE_BLE_ARCHIVE?draw_detail_page():ESP_OK; }

static void seed(void)
{
    ble_archive_dispose();memset(&discovery,0,sizeof(discovery));ble_discovery_live=&discovery;
    discovery.inventory.initialized=true;discovery.inventory.count=2;now_us=10000000;
    for(unsigned i=0;i<2;++i){
        cerberus_s3_ble_discovery_entry_t *e=&discovery.inventory.entries[i];
        e->epoch=8+i;e->source_now_ms=9500;e->original_source_ms=9000;
        e->record.peer.address[0]=7;e->record.peer.address_type=i;e->record.peer.rssi=-44;
        strcpy(e->record.peer.name,i?"random typed peer":"public typed peer");
        e->record.peer.manufacturer_id=0xFFFF;e->record.generation=4;e->record.index=i;
        e->record.advertisement.address[0]=7;e->record.advertisement.address_type=i;
        uint8_t data[]={2,1,6};memcpy(e->record.advertisement.data,data,3);e->record.advertisement.length=3;
        e->record.advertisement.reports_seen=9;
    }
    cerberus_s3_ble_discovery_record_t *r=&discovery.inventory.entries[0].record;
    ble_findmy_selected=(cerberus_findmy_monitor_record_t){.peer=r->peer,.advertisement=r->advertisement,
        .generation=r->generation,.index=r->index,.observed_ms=9000};
    ble_selected_result=r->peer;ble_selection_valid=ble_selected_raw_frozen=ble_selected_original_time64_valid=true;
    ble_selected_original_ms64=9000;ble_selected_source_ceiling_ms64=9500;
    current_page=UI_PAGE_BLE;ble_mode=UI_BLE_MODE_MENU;allocation_fails=false;start_error=copy_error=ESP_OK;
    io=(cerberus_ble_archive_io_status_t){0};privacy_mask=false;
}
static void complete(void)
{ io.state=CERBERUS_BLE_ARCHIVE_IO_COMPLETE;io.worker_active=false;io.result_ready=true;
  strcpy(io.file_name,"B00001.BLE");ble_archive_tick(false); }
static void test_capture(void)
{
    seed();ble_archive_open_menu();CHECK(ble_archive_view&&ble_archive_menu);CHECK(strstr(drawn,"SAVE RETAINED LIST"));
    CHECK(ble_archive_capture(false)==ESP_OK&&ble_archive_view->incoming.count==2);
    CHECK(ble_archive_view->incoming.rows[0].scan_epoch==8&&ble_archive_view->incoming.rows[1].address_type==1);
    CHECK(ble_archive_view->incoming.rows[0].original_source_uptime_ms==9000);
    now_us=-1;CHECK(ble_archive_capture(false)==ESP_ERR_INVALID_STATE);now_us=10000000;
    discovery.inventory.entries[0].source_now_ms=11000;CHECK(ble_archive_capture(false)==ESP_ERR_INVALID_RESPONSE);
    discovery.inventory.entries[0].source_now_ms=9500;
    discovery.inventory.entries[1].record.advertisement.address[0]=8;CHECK(ble_archive_capture(false)==ESP_ERR_INVALID_RESPONSE);
    discovery.inventory.entries[1].record.advertisement.address[0]=7;
    ble_selected_original_time64_valid=false;CHECK(ble_archive_capture(true)==ESP_OK);
    CHECK(!ble_archive_view->incoming.rows[0].source_time_valid&&!ble_archive_view->incoming.rows[0].copy_ceiling_ms);
    CHECK(!ble_archive_view->incoming.rows[0].scan_epoch_valid);
    ble_selected_raw_frozen=false;CHECK(ble_archive_capture(true)==ESP_ERR_NOT_FOUND);
    ble_selected_raw_frozen=true;ble_selected_result.address_type=3;CHECK(ble_archive_capture(true)==ESP_ERR_NOT_FOUND);
    cerberus_ble_archive_row_t row,before;memset(&row,0xA5,sizeof(row));before=row;
    cerberus_c5_ble_result_t peer=ble_findmy_selected.peer;cerberus_c5_ble_advertisement_t raw=ble_findmy_selected.advertisement;
    peer.uuid_length=3;CHECK(!cerberus_ble_archive_capture_row(&row,&peer,&raw,0,0,0,false,0,0,false));CHECK(!memcmp(&row,&before,sizeof(row)));
    peer.uuid_length=0;peer.rssi=126;CHECK(cerberus_ble_archive_capture_row(&row,&peer,&raw,0,0,0,false,0,0,false));CHECK(!row.rssi_valid&&row.rssi==127);
    memset(peer.name,'X',sizeof(peer.name));CHECK(!cerberus_ble_archive_capture_row(&row,&peer,&raw,0,0,0,false,0,0,false));
    ble_archive_return();CHECK(!allocation&&current_page==UI_PAGE_BLE&&ble_mode==UI_BLE_MODE_MENU);
}
static void test_jobs(void)
{
    seed();ble_mode=UI_BLE_MODE_DETAIL;unsigned old_saves=saves;ble_archive_save_selected();
    CHECK(saves==old_saves+1&&ble_archive_view->job_pending&&current_page==UI_PAGE_BLE_ARCHIVE);
    CHECK(saved.count==1&&!strcmp(saved.rows[0].name,"public typed peer"));
    strcpy(ble_findmy_selected.peer.name,"later changed");complete();
    CHECK(ble_archive_view->has_document&&!ble_archive_view->job_pending&&strstr(drawn,"HISTORICAL"));
    CHECK(!strcmp(ble_archive_view->document.rows[0].name,"public typed peer"));
    ble_archive_tap(20,160);CHECK(ble_archive_view->selected_valid);ble_archive_tap(64,426);
    CHECK(ble_archive_view&&ble_archive_view->view==BLE_ARCHIVE_UI_LIST);ble_archive_tap(64,426);
    CHECK(!allocation&&current_page==UI_PAGE_BLE&&ble_mode==UI_BLE_MODE_DETAIL);
    seed();ble_archive_open_menu();ble_archive_tap(40,160);CHECK(saved.count==2&&ble_archive_view->job_pending);
    uint64_t wanted=io.job_id;io.job_id=wanted+100;io.worker_active=false;io.state=CERBERUS_BLE_ARCHIVE_IO_ERROR;
    ble_archive_tick(false);CHECK(ble_archive_view->job_pending);io.job_id=wanted;io.worker_active=true;
    unsigned canceled=cancels;ble_archive_tap(64,426);CHECK(cancels==canceled+1&&!allocation&&current_page==UI_PAGE_BLE);
    complete();CHECK(!allocation); /* A late worker outcome cannot reopen the page. */
    seed();current_page=UI_PAGE_FILES;ble_archive_open_file("/moved","old.BLE");CHECK(ble_archive_view->job_pending);
    copy_error=ESP_FAIL;complete();CHECK(ble_archive_view->view==BLE_ARCHIVE_UI_ERROR&&!ble_archive_view->has_document);
    ble_archive_tap(64,426);CHECK(!allocation&&current_page==UI_PAGE_FILES);
    seed();ble_archive_save_selected();unsigned paint=repaints;io.worker_active=false;io.state=CERBERUS_BLE_ARCHIVE_IO_ERROR;io.error=ESP_ERR_INVALID_RESPONSE;
    ble_archive_tick(true);CHECK(repaints==paint&&!ble_archive_view->job_pending);ble_archive_return();
    seed();start_error=ESP_ERR_INVALID_STATE;ble_archive_save_selected();CHECK(ble_archive_view->view==BLE_ARCHIVE_UI_ERROR&&!ble_archive_view->job_pending);ble_archive_return();
    seed();allocation_fails=true;ble_archive_open_menu();CHECK(!ble_archive_view&&strstr(drawn,"ARCHIVE UNAVAILABLE"));ble_archive_tap(64,426);CHECK(current_page==UI_PAGE_BLE);
    seed();ble_archive_open_menu();ble_archive_tap(40,290);CHECK(current_page==UI_PAGE_FILES&&file_mode==UI_FILE_MODE_LIST&&!allocation);
    seed();ble_archive_save_selected();current_page=UI_PAGE_MAIN;ble_archive_tick(false);CHECK(!allocation);
    CHECK(ble_archive_is_file("B00001.BLE")&&ble_archive_is_file("old.ble")&&!ble_archive_is_file("x.TMP")&&!ble_archive_is_file(NULL)&&!ble_archive_is_file(".BLE"));
}
static void test_accepted_job_rejected_by_view(void)
{
    for(unsigned operation=0;operation<2;++operation) {
        seed();ble_archive_save_selected();complete();
        cerberus_ble_archive_t previous=ble_archive_view->document;
        /* Inject an unexpectedly reused job identity at the IO boundary. The
         * actual viewer begin rejects it; both controller entry points must
         * release the newly accepted job and retain the previous document. */
        const uint64_t accepted_job=sequence+1U;
        ble_archive_view->job_id=accepted_job;
        unsigned canceled=cancels,released=releases,copied=copies;
        if(operation==0)ble_archive_save_selected();
        else ble_archive_open_file("/moved","another.BLE");
        CHECK(io.job_id==accepted_job&&io.cancel_requested);
        CHECK(cancels==canceled+1U&&releases==released+1U);
        CHECK(last_canceled_job==accepted_job&&last_released_job==accepted_job);
        CHECK(ble_archive_open_error==ESP_ERR_INVALID_STATE);
        CHECK(ble_archive_view->view==BLE_ARCHIVE_UI_ERROR&&!ble_archive_view->job_pending);
        CHECK(ble_archive_view->has_document&&!memcmp(&previous,&ble_archive_view->document,sizeof(previous)));
        CHECK(strstr(drawn,"PREVIOUS SNAPSHOT RETAINED"));
        ble_archive_tick(false);CHECK(copies==copied&&releases==released+1U);
        ble_archive_return();CHECK(!allocation&&current_page==UI_PAGE_BLE);
    }
}
static void test_complete_without_result(void)
{
    seed();ble_archive_save_selected();complete();
    cerberus_ble_archive_t previous=ble_archive_view->document;
    ble_archive_open_file("/moved","missing-result.BLE");
    const uint64_t job=io.job_id;
    unsigned copied=copies,released=releases;
    io.worker_active=false;io.state=CERBERUS_BLE_ARCHIVE_IO_COMPLETE;
    io.result_ready=false;io.error=ESP_OK;
    ble_archive_tick(false);
    CHECK(copies==copied&&releases==released+1U&&last_released_job==job);
    CHECK(!ble_archive_view->job_pending&&ble_archive_view->view==BLE_ARCHIVE_UI_ERROR);
    CHECK(ble_archive_view->error==ESP_ERR_INVALID_STATE);
    CHECK(ble_archive_view->has_document&&!memcmp(&previous,&ble_archive_view->document,sizeof(previous)));
    CHECK(strstr(drawn,"PREVIOUS SNAPSHOT RETAINED"));
    ble_archive_tick(false);CHECK(copies==copied&&releases==released+1U);
    ble_archive_return();CHECK(!allocation&&current_page==UI_PAGE_BLE);
}
int main(void)
{
    test_capture();test_jobs();test_accepted_job_rejected_by_view();test_complete_without_result();
    ble_archive_dispose();CHECK(frees+1==allocations); /* One rejected allocation. */
    printf("%u archive capture/controller checks passed; saves=%u loads=%u copies=%u releases=%u.\n",checks,saves,loads,copies,releases);return 0;
}
