#undef NDEBUG
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include "ble_archive_io.h"
#include "archive_storage_types.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Archive IO %u: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
static bool locked;
#define portENTER_CRITICAL(p) do{(void)(p);CHECK(!locked);locked=true;}while(0)
#define portEXIT_CRITICAL(p) do{(void)(p);CHECK(locked);locked=false;}while(0)
#define MALLOC_CAP_INTERNAL 1U
#define MALLOC_CAP_8BIT 2U
#define MALLOC_CAP_SPIRAM 4U
#define pdPASS 1
static size_t free_internal=100000,largest_internal=50000;
static unsigned allocations,frees,outstanding,tasks,deletes;
static bool fail_alloc,fail_task,immediate_task,release_in_alloc,storage_active;
static void (*pending_task)(void*);
static void *pending_argument;
static uint64_t last_job;
static void (*storage_hook)(void);
static cerberus_ble_archive_io_status_t status(void)
{cerberus_ble_archive_io_status_t s;cerberus_ble_archive_io_get_status(&s);return s;}
static size_t heap_caps_get_free_size(unsigned caps){CHECK(!locked&&caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));return free_internal;}
static size_t heap_caps_get_largest_free_block(unsigned caps){CHECK(!locked&&caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));return largest_internal;}
static void *heap_caps_calloc(size_t count,size_t size,unsigned caps)
{
    CHECK(!locked&&count==1&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++allocations;
    void*p=fail_alloc?NULL:calloc(count,size);if(p)++outstanding;
    if(release_in_alloc)cerberus_ble_archive_io_release(status().job_id);
    return p;
}
static void heap_caps_free(void *p){CHECK(!locked&&!storage_active);if(p){CHECK(outstanding);--outstanding;++frees;}free(p);}
static int xTaskCreate(void(*fn)(void*),const char *name,unsigned stack,void*arg,unsigned priority,void*handle)
{
    CHECK(!locked&&!pending_task&&!strcmp(name,"ble_archive")&&stack==6144&&priority==2&&!handle);++tasks;
    if(fail_task)return 0;
    if(immediate_task)fn(arg);else{pending_task=fn;pending_argument=arg;}return pdPASS;
}
static void vTaskDelete(void *handle){CHECK(!handle&&!locked);++deletes;}
static uint8_t storage_bytes[CERBERUS_BLE_ARCHIVE_MAX_SIZE];
static size_t storage_length;
static unsigned saves,loads;
static esp_err_t storage_error;
static bool partial_save;
static char last_path[512],last_name[256];
static esp_err_t cerberus_storage_ble_archive_save(const uint8_t*data,size_t length,char*name,size_t capacity)
{
    CHECK(!locked&&!storage_active&&length<=sizeof(storage_bytes)&&capacity==256);++saves;storage_active=true;
    CHECK(status().worker_active&&status().state==CERBERUS_BLE_ARCHIVE_IO_SAVING);
    if(storage_hook)storage_hook();
    if(storage_error==ESP_OK){memcpy(storage_bytes,data,length);storage_length=length;strcpy(name,"B00001.BLE");}
    else if(partial_save)strcpy(name,"B00001.TMP");
    storage_active=false;return storage_error;
}
static esp_err_t cerberus_storage_ble_archive_load(const char*path,const char*name,uint8_t*data,size_t capacity,size_t*length)
{
    CHECK(!locked&&!storage_active&&capacity==sizeof(storage_bytes));++loads;storage_active=true;
    CHECK(status().worker_active&&status().state==CERBERUS_BLE_ARCHIVE_IO_LOADING);
    strcpy(last_path,path);strcpy(last_name,name);if(storage_hook)storage_hook();
    if(storage_error==ESP_OK){memcpy(data,storage_bytes,storage_length);*length=storage_length;}else *length=0;
    storage_active=false;return storage_error;
}
/* Interleave Release and nested readers at the actual large copy. No document
 * bytes may be copied while interrupts are disabled, nor freed while pinned. */
static void (*copy_hook)(void);
static unsigned document_copies;
static void *archive_checked_copy(void *out,const void *in,size_t bytes)
{
    if(bytes==sizeof(cerberus_ble_archive_t)){
        CHECK(!locked);++document_copies;
        void(*hook)(void)=copy_hook;copy_hook=NULL;if(hook)hook();
    }
    return memcpy(out,in,bytes);
}
#define memcpy archive_checked_copy
#include "archive_io_impl.h"
#undef memcpy

static char console_text[1600];
static size_t console_length;
static void console_printf(const char *format,...)
{
    va_list args;va_start(args,format);
    int n=vsnprintf(console_text+console_length,sizeof(console_text)-console_length,format,args);
    va_end(args);CHECK(n>=0&&(size_t)n<sizeof(console_text)-console_length);console_length+=(size_t)n;
}
#include "archive_console_impl.h"
static void console_check(cerberus_ble_archive_io_state_t expected_state)
{
    cerberus_ble_archive_io_status_t before=status();CHECK(before.state==expected_state);
    unsigned saved_allocations=allocations,saved_frees=frees,saved_tasks=tasks,saved_saves=saves,saved_loads=loads;
    console_length=0;console_text[0]=0;print_ble_archive();
    cerberus_ble_archive_io_status_t after=status();CHECK(!memcmp(&before,&after,sizeof(before)));
    CHECK(allocations==saved_allocations&&frees==saved_frees&&tasks==saved_tasks&&saves==saved_saves&&loads==saved_loads);
    char line[600];
#define FIELD(format,value) do{CHECK(snprintf(line,sizeof(line),format "\r\n",value)>0);CHECK(strstr(console_text,line));}while(0)
    FIELD("ble_archive_state=%s",cerberus_ble_archive_io_state_name(expected_state));
    FIELD("ble_archive_active=%u",before.worker_active?1U:0U);
    FIELD("ble_archive_error=%" PRId32,(int32_t)before.error);
    FIELD("ble_archive_job_id=%" PRIu64,before.job_id);
    FIELD("ble_archive_operation=%u",(unsigned)before.operation);
    FIELD("ble_archive_cancel_requested=%u",before.cancel_requested?1U:0U);
    FIELD("ble_archive_result_ready=%u",before.result_ready?1U:0U);
    FIELD("ble_archive_bytes=%" PRIu32,before.bytes);
    FIELD("ble_archive_rows=%u",(unsigned)before.count);
    FIELD("ble_archive_path=%s",before.relative_path);
    FIELD("ble_archive_file=%s",before.file_name);
#undef FIELD
}

static cerberus_ble_archive_t input,expected,output,untouched,nested_output;
static void fill(unsigned count)
{
    memset(&input,0,sizeof(input));input.saved_uptime_ms=100000;input.count=(uint8_t)count;
    for(unsigned i=0;i<count;++i){
        cerberus_ble_archive_row_t *r=&input.rows[i];r->address[0]=(uint8_t)(i%32);r->address[5]=0xA1;r->address_type=i%4;
        snprintf(r->name,sizeof(r->name),"test-%u",i);r->rssi=-42;r->rssi_valid=true;
        r->advertisement_length=31;for(unsigned j=0;j<31;++j)r->advertisement[j]=(uint8_t)(i+j);
        r->copy_ceiling_ms=99999;r->source_time_valid=true;r->original_source_uptime_ms=90000;
        r->receiver=CERBERUS_BLE_ARCHIVE_RECEIVER_S3;r->index=(uint8_t)(i%32);
        if(i<32&&!(i%2)){
            r->scan_epoch_valid=true;r->scan_epoch=55;
            memset(r->advertisement,0,sizeof(r->advertisement));r->advertisement_length=3;
            r->advertisement[0]=2;r->advertisement[1]=1;r->advertisement[2]=6;
            r->response_state=CERBERUS_BLE_ARCHIVE_RESPONSE_PRESENT;
            r->response_length=r->response_original_length=5;r->response_event_type=4;
            memcpy(r->response,"\004\011RSP",5);
            r->response_original_sequence=1;r->response_sequence=2;
            r->response_original_observed_ms=90000;r->response_observed_ms=90010;
            r->response_time_valid=true;r->response_source_uptime_ms=90010;
        }
        if(i>=32){
            r->receiver=CERBERUS_BLE_ARCHIVE_RECEIVER_C5;r->rssi_kind=CERBERUS_BLE_ARCHIVE_RSSI_LATEST_ADV;
            r->source_time_valid=false;r->copy_ceiling_ms=r->original_source_uptime_ms=0;
            r->receiver_boot_id=0xC501;r->scan_epoch_valid=true;r->scan_epoch=55;r->generation=7;
            r->age_upper_valid=true;r->age_upper_at_save_ms=123+i;
            if(i%3==1){
                r->receive=(cerberus_dual_ble_receive_metadata_t){.source_kind=CERBERUS_DUAL_BLE_RECEIVE_LEGACY,
                    .callback_length=31,.sid=0xff,.data_status=0xff,.ad_validation=CERBERUS_DUAL_BLE_AD_INVALID};
                r->advertisement_flags=2;
            } else if(i%3==2||i==63){
                r->receive=(cerberus_dual_ble_receive_metadata_t){.source_kind=CERBERUS_DUAL_BLE_RECEIVE_EXTENDED,
                    .callback_length=255,.sid=(uint8_t)(i%16),.primary_phy=1,.secondary_phy=3,
                    .ad_validation=CERBERUS_DUAL_BLE_AD_VALID};
                r->advertisement_event_type=3;r->advertisement_flags=3;
                r->advertisement[0]=254;r->advertisement[1]=0xff;
            }
        }
    }
    CHECK(cerberus_ble_archive_validate(&input));expected=input;
}
static void reset(void)
{
    CHECK(!locked&&!pending_task&&!storage_active&&!status().worker_active&&!archive_io_readers);
    cerberus_ble_archive_io_release(status().job_id);CHECK(!outstanding&&!archive_io_context);
    free_internal=100000;largest_internal=50000;fail_alloc=fail_task=immediate_task=release_in_alloc=false;
    storage_hook=copy_hook=NULL;storage_error=ESP_OK;partial_save=false;saves=loads=0;last_path[0]=last_name[0]=0;
    memset(&output,0xA5,sizeof(output));untouched=output;
}
static void run_worker(void)
{
    CHECK(pending_task);void(*fn)(void*)=pending_task;void*arg=pending_argument;pending_task=NULL;pending_argument=NULL;
    unsigned before=deletes;fn(arg);CHECK(deletes==before+1&&!status().worker_active&&!storage_active&&!locked);
}
static uint64_t start_save(void)
{uint64_t id;CHECK(cerberus_ble_archive_io_save(&input,&id)==ESP_OK&&id>last_job);last_job=id;return id;}
static uint64_t start_load(void)
{uint64_t id;CHECK(cerberus_ble_archive_io_load("/COPIED","saved.ble",&id)==ESP_OK&&id>last_job);last_job=id;return id;}
static void release_hook(void)
{
    console_check(status().operation==CERBERUS_BLE_ARCHIVE_IO_SAVE?CERBERUS_BLE_ARCHIVE_IO_SAVING:CERBERUS_BLE_ARCHIVE_IO_LOADING);
    uint64_t id=status().job_id;unsigned before=frees;cerberus_ble_archive_io_release(id);CHECK(frees==before&&outstanding==1);
    uint64_t refused=99;CHECK(cerberus_ble_archive_io_load("/","x.BLE",&refused)==ESP_ERR_INVALID_STATE&&refused==0);
    CHECK(cerberus_ble_archive_io_copy_result(id,&output)==ESP_ERR_INVALID_STATE&&!memcmp(&output,&untouched,sizeof(output)));
}
static void cancel_hook(void){cerberus_ble_archive_io_cancel(status().job_id);CHECK(status().cancel_requested&&outstanding==1);}
static void release_during_copy(void)
{
    CHECK(!locked&&archive_io_readers>0&&outstanding==1);
    unsigned before=frees;uint64_t id=status().job_id;
    cerberus_ble_archive_io_release(id);
    CHECK(frees==before&&outstanding==1&&archive_io_context&&!status().result_ready);
    uint64_t refused=99;
    CHECK(cerberus_ble_archive_io_save(&input,&refused)==ESP_ERR_INVALID_STATE&&refused==0);
    CHECK(cerberus_ble_archive_io_copy_result(id,&untouched)==ESP_ERR_INVALID_STATE);
}
static void nested_copy_then_release(void)
{
    CHECK(archive_io_readers==1&&outstanding==1);
    copy_hook=release_during_copy;
    CHECK(cerberus_ble_archive_io_copy_result(status().job_id,&nested_output)==ESP_OK);
    CHECK(!memcmp(&nested_output,&expected,sizeof(expected))&&archive_io_readers==1&&outstanding==1);
}

int main(void)
{
    CHECK(CERBERUS_STORAGE_BLE_ARCHIVE_MAX_BYTES==13856U&&CERBERUS_BLE_ARCHIVE_MAX_SIZE==13856U);
    console_check(CERBERUS_BLE_ARCHIVE_IO_IDLE);
    CHECK(!strcmp(console_text,"ble_archive_state=IDLE\r\nble_archive_active=0\r\nble_archive_error=0\r\n"
        "ble_archive_job_id=0\r\nble_archive_operation=0\r\nble_archive_cancel_requested=0\r\n"
        "ble_archive_result_ready=0\r\nble_archive_bytes=0\r\nble_archive_rows=0\r\nble_archive_path=\r\nble_archive_file=\r\n"));
    for(unsigned count=1;count<=64;++count){
        reset();fill(count);uint64_t id=start_save();CHECK(!saves&&!loads&&status().worker_active&&outstanding==1);
        console_check(CERBERUS_BLE_ARCHIVE_IO_STARTING);
        CHECK(cerberus_ble_archive_io_copy_result(id,&output)==ESP_ERR_INVALID_STATE&&!memcmp(&output,&untouched,sizeof(output)));
        input.rows[0].name[0]='!';
        for(unsigned i=32;i<count;++i)memset(&input.rows[i].receive,0,sizeof(input.rows[i].receive));
        uint64_t unused;CHECK(cerberus_ble_archive_io_save(&input,&unused)==ESP_ERR_INVALID_STATE);
        run_worker();CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_COMPLETE&&status().result_ready&&status().count==count);
        console_check(CERBERUS_BLE_ARCHIVE_IO_COMPLETE);
        CHECK(status().bytes==32+216*count&&!strcmp(status().relative_path,"/CERBERUS/BLE")&&!strcmp(status().file_name,"B00001.BLE"));
        if(count==64)CHECK(storage_length==13856U&&storage_bytes[13855]==CERBERUS_DUAL_BLE_AD_VALID);
        CHECK(cerberus_ble_archive_io_copy_result(id,&output)==ESP_OK&&!memcmp(&output,&expected,sizeof(output)));
        cerberus_ble_archive_io_release(id);CHECK(!outstanding&&!status().result_ready);
        console_check(CERBERUS_BLE_ARCHIVE_IO_COMPLETE);
        memset(&output,0xA5,sizeof(output));untouched=output;CHECK(cerberus_ble_archive_io_copy_result(id,&output)!=ESP_OK&&!memcmp(&output,&untouched,sizeof(output)));
        id=start_load();run_worker();CHECK(loads==1&&saves==1&&status().result_ready&&status().count==count);
        CHECK(cerberus_ble_archive_io_copy_result(id,&output)==ESP_OK&&!memcmp(&output,&expected,sizeof(output)));
        CHECK(!strcmp(last_path,"/COPIED")&&!strcmp(last_name,"saved.ble"));
    }
    for(unsigned nested=0;nested<2;++nested){
        reset();fill(64);uint64_t copy_id=start_save();run_worker();
        unsigned before=frees,copies=document_copies;
        copy_hook=nested?nested_copy_then_release:release_during_copy;
        CHECK(cerberus_ble_archive_io_copy_result(copy_id,&output)==ESP_OK);
        CHECK(!memcmp(&output,&expected,sizeof(output))&&!archive_io_readers&&!archive_io_context&&!outstanding);
        CHECK(frees==before+1&&document_copies==copies+1+nested);
        cerberus_ble_archive_io_release(copy_id);CHECK(frees==before+1);
    }
    reset();fill(64);uint64_t saturated_id=start_save();run_worker();archive_io_readers=UINT16_MAX;
    CHECK(cerberus_ble_archive_io_copy_result(saturated_id,&output)==ESP_ERR_INVALID_STATE&&!memcmp(&output,&untouched,sizeof(output)));
    archive_io_readers=0;cerberus_ble_archive_io_release(saturated_id);
    reset();fill(1);input.count=0;uint64_t id=start_save();run_worker();CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_ERROR&&!saves&&!outstanding);
    for(unsigned kind=0;kind<5;++kind){
        reset();fill(2);if(kind==0)free_internal=24575;if(kind==1)largest_internal=8191;if(kind==2)fail_alloc=true;if(kind==3)fail_task=true;
        if(kind==4){immediate_task=true;release_in_alloc=true;}
        esp_err_t e=cerberus_ble_archive_io_save(&input,&id);CHECK(id>last_job);last_job=id;
        if(kind==4)CHECK(e==ESP_OK&&status().state==CERBERUS_BLE_ARCHIVE_IO_CANCELED);
        else CHECK(e==ESP_ERR_NO_MEM&&status().state==CERBERUS_BLE_ARCHIVE_IO_ERROR);
        CHECK(!saves&&!loads&&!outstanding&&!pending_task&&!status().worker_active);
    }
    for(unsigned operation=0;operation<2;++operation){
        for(unsigned release=0;release<2;++release){
            reset();fill(64);CHECK(cerberus_ble_archive_encode(&input,storage_bytes,sizeof(storage_bytes),&storage_length));
            id=operation?start_load():start_save();if(release)cerberus_ble_archive_io_release(id);else cerberus_ble_archive_io_cancel(id);
            CHECK(outstanding==1&&status().worker_active);run_worker();
            CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_CANCELED&&!saves&&!loads&&!outstanding&&!status().result_ready);
        }
        for(unsigned release=0;release<2;++release){
            reset();fill(64);CHECK(cerberus_ble_archive_encode(&input,storage_bytes,sizeof(storage_bytes),&storage_length));
            storage_hook=release?release_hook:cancel_hook;id=operation?start_load():start_save();run_worker();
            CHECK(status().cancel_requested&&status().state==(operation?CERBERUS_BLE_ARCHIVE_IO_CANCELED:CERBERUS_BLE_ARCHIVE_IO_COMPLETE));
            CHECK(status().result_ready==(!operation&&!release));
            if(!operation&&!release){
                CHECK(cerberus_ble_archive_io_copy_result(id,&output)==ESP_OK&&!memcmp(&output,&expected,sizeof(output)));
            }
            if(!operation){CHECK(saves==1&&!strcmp(status().file_name,"B00001.BLE"));}
            cerberus_ble_archive_io_release(id);CHECK(!outstanding);
        }
    }
    reset();fill(64);storage_error=ESP_FAIL;partial_save=true;id=start_save();run_worker();
    CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_ERROR&&!strcmp(status().file_name,"B00001.TMP")&&!outstanding);
    console_check(CERBERUS_BLE_ARCHIVE_IO_ERROR);
    CHECK(cerberus_ble_archive_io_copy_result(id,&output)!=ESP_OK&&!memcmp(&output,&untouched,sizeof(output)));
    reset();fill(64);CHECK(cerberus_ble_archive_encode(&input,storage_bytes,sizeof(storage_bytes),&storage_length));storage_bytes[13855]^=1;
    id=start_load();run_worker();CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_ERROR&&status().error==ESP_ERR_INVALID_RESPONSE&&!outstanding);
    CHECK(cerberus_ble_archive_io_copy_result(id,&output)!=ESP_OK&&!memcmp(&output,&untouched,sizeof(output)));
    reset();storage_error=ESP_ERR_TIMEOUT;id=start_load();run_worker();CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_ERROR&&!outstanding);
    reset();fill(1);char path[512]="/COPY",name[256]="before.BLE";CHECK(cerberus_ble_archive_encode(&input,storage_bytes,sizeof(storage_bytes),&storage_length));
    CHECK(cerberus_ble_archive_io_load(path,name,&id)==ESP_OK);path[1]='!';name[0]='!';run_worker();
    CHECK(!strcmp(last_path,"/COPY")&&!strcmp(last_name,"before.BLE"));
    uint64_t stale=id;cerberus_ble_archive_io_release(id);id=start_save();cerberus_ble_archive_io_cancel(stale);cerberus_ble_archive_io_release(stale);
    CHECK(!status().cancel_requested&&outstanding==1);run_worker();CHECK(status().result_ready);
    reset();fill(1);immediate_task=true;id=start_save();CHECK(status().state==CERBERUS_BLE_ARCHIVE_IO_COMPLETE&&!pending_task);reset();
    char badpath[512];memset(badpath,'X',sizeof(badpath));CHECK(cerberus_ble_archive_io_load(badpath,"x.BLE",&id)==ESP_ERR_INVALID_ARG&&id==0);
    CHECK(cerberus_ble_archive_io_save(NULL,&id)==ESP_ERR_INVALID_ARG&&id==0);CHECK(cerberus_ble_archive_io_load(NULL,"x.BLE",&id)==ESP_ERR_INVALID_ARG);
    CHECK(cerberus_ble_archive_io_save(&input,NULL)==ESP_ERR_INVALID_ARG);CHECK(cerberus_ble_archive_io_copy_result(0,NULL)==ESP_ERR_INVALID_ARG);
    archive_io_last_job=UINT64_MAX;CHECK(cerberus_ble_archive_io_save(&input,&id)==ESP_ERR_INVALID_STATE&&id==0&&!pending_task);
    CHECK(!strcmp(cerberus_ble_archive_io_state_name(CERBERUS_BLE_ARCHIVE_IO_CANCELED),"CANCELED"));
    CHECK(!strcmp(cerberus_ble_archive_io_state_name((cerberus_ble_archive_io_state_t)99),"UNKNOWN"));
    cerberus_ble_archive_io_get_status(NULL);cerberus_ble_archive_io_release(0);cerberus_ble_archive_io_cancel(0);
    CHECK(!outstanding&&allocations>=frees);printf("Archive async lifecycle passed %u checks; host sizeof context=%zu status=%zu.\n",checks,sizeof(ble_archive_io_context_t),sizeof(cerberus_ble_archive_io_status_t));
    return 0;
}
