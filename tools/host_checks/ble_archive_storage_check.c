#undef NDEBUG
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define native_open _open
#define native_fdopen _fdopen
#define native_close _close
#define native_fileno _fileno
#define native_chdir _chdir
#define native_mkdir(path) _mkdir(path)
#define native_rmdir _rmdir
#else
#include <unistd.h>
#define native_open open
#define native_fdopen fdopen
#define native_close close
#define native_fileno fileno
#define native_chdir chdir
#define native_mkdir(path) mkdir(path,0775)
#define native_rmdir rmdir
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif
#include "esp_err.h"
#include "esp_check.h"
#include "archive_storage_types.h"
#include "ble_archive.h"
#include "ble_archive_family_fixture.h"
#define CERBERUS_STORAGE_MOUNT_PATH "sdcard"
#define TAG "archive-storage-test"
#define ESP_LOGI(...) ((void)0)
static unsigned checks, cases;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"Archive storage %u: %s\n",__LINE__,#x);exit(1);}}while(0)
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
typedef int *SemaphoreHandle_t;
static int mutex_token;
static SemaphoreHandle_t storage_mutex;
static bool locked, sd_mounted, fail_lock, fail_mutex, wardrive_busy;
static cerberus_storage_owner_t storage_owner;
static FILE *sd_wifi_capture_file;
static struct {bool active;} sd_recon;
static char sd_operation_source_path[CERBERUS_STORAGE_PATH_MAX+CERBERUS_STORAGE_NAME_MAX];
static unsigned locks, unlocks, streams, descriptors;
static unsigned handoff_probes,unmounts;
static bool probe_handoff;
esp_err_t cerberus_storage_unmount(void);
esp_err_t cerberus_storage_reserve_for_usb(void);
static esp_err_t unmount_locked(void){CHECK(locked);++unmounts;sd_mounted=false;return ESP_OK;}
static bool wardrive_storage_busy(void){return wardrive_busy;}
static SemaphoreHandle_t xSemaphoreCreateMutex(void){return fail_mutex?NULL:&mutex_token;}
static int xSemaphoreTake(SemaphoreHandle_t value,unsigned wait)
{CHECK(value==&mutex_token&&wait==1000);if(locked||fail_lock)return 0;locked=true;++locks;return 1;}
static void xSemaphoreGive(SemaphoreHandle_t value){CHECK(value==&mutex_token&&locked);locked=false;++unlocks;}
enum event {STAT,MKDIR,OPEN,FDOPEN,RAW_CLOSE,WRITE,FLUSH,SYNC,CLOSE,RENAME,FOPEN,READ,GETC,EVENTS};
static unsigned calls[EVENTS],fault[EVENTS];
static bool extra_error, no_eof, short_read, collide_all, race_temp, race_final;
static unsigned zero_read_at;
static int stat_size_delta;
static bool event(enum event which)
{
    CHECK(locked);
    if(probe_handoff){
        CHECK(cerberus_storage_unmount()==ESP_ERR_TIMEOUT);
        CHECK(cerberus_storage_reserve_for_usb()==ESP_ERR_TIMEOUT);
        CHECK(sd_mounted&&storage_owner==CERBERUS_STORAGE_OWNER_FIRMWARE&&!unmounts);
        ++handoff_probes;
    }
    ++calls[which];return fault[which]&&calls[which]==fault[which];
}
static void put(const char *path,const void *bytes,size_t length)
{FILE *f=fopen(path,"wb");CHECK(f&&fwrite(bytes,1,length,f)==length&&fclose(f)==0);}
static int wrapped_stat(const char *path,struct stat *item)
{
    if(event(STAT)){errno=EIO;return -1;}
    const char *leaf=strrchr(path,'/');
    if(collide_all&&leaf&&strlen(leaf)==11&&leaf[1]=='B'){memset(item,0,sizeof(*item));item->st_mode=S_IFREG;return 0;}
    int result=stat(path,item);
    if(result==0&&S_ISREG(item->st_mode))item->st_size+=stat_size_delta;
    return result;
}
static int wrapped_mkdir(const char *path,int mode)
{CHECK(mode==0775);if(event(MKDIR)){errno=EIO;return -1;}return native_mkdir(path);}
static int wrapped_open(const char *path,int flags,int mode)
{
    CHECK(flags==(O_CREAT|O_EXCL|O_WRONLY)&&mode==0664);
    if(event(OPEN)){errno=EIO;return -1;}
    if(race_temp){race_temp=false;put(path,"KEEP",4);}
    int fd=native_open(path,flags|O_BINARY,mode);if(fd>=0)++descriptors;return fd;
}
static FILE *wrapped_fdopen(int fd,const char *mode)
{CHECK(descriptors&&strcmp(mode,"wb")==0);if(event(FDOPEN))return NULL;FILE*f=native_fdopen(fd,mode);if(f){--descriptors;++streams;}return f;}
static int wrapped_close(int fd){bool fail=event(RAW_CLOSE);CHECK(descriptors);--descriptors;int r=native_close(fd);return fail?-1:r;}
static FILE *wrapped_fopen(const char *path,const char *mode)
{CHECK(!strcmp(mode,"rb"));if(event(FOPEN))return NULL;FILE*f=fopen(path,mode);if(f)++streams;return f;}
static size_t wrapped_write(const void *data,size_t size,size_t count,FILE *stream)
{CHECK(size==1&&count&&streams);bool fail=event(WRITE);return fwrite(data,size,fail?count/2:count,stream);}
static int wrapped_ferror(FILE *stream){return extra_error||ferror(stream);}
static int wrapped_feof(FILE *stream){return no_eof?0:feof(stream);}
static int wrapped_flush(FILE *stream){if(event(FLUSH))return EOF;return fflush(stream);}
static int wrapped_sync(int fd){CHECK(fd>=0);return event(SYNC)?-1:0;}
static int wrapped_fclose(FILE *stream)
{bool fail=event(CLOSE);CHECK(streams);--streams;int r=fclose(stream);return fail?EOF:r;}
static int wrapped_rename(const char *source,const char *destination)
{CHECK(!streams&&!descriptors);if(event(RENAME))return -1;return rename(source,destination);}
static size_t wrapped_read(void *data,size_t size,size_t count,FILE *stream)
{
    CHECK(size==1&&count);bool fail=event(READ);
    if(fail||zero_read_at==calls[READ])return 0;
    if(short_read&&count>7)count=7;
    return fread(data,size,count,stream);
}
static int wrapped_getc(FILE *stream){if(event(GETC))return 'X';return fgetc(stream);}
/* The successful-close hook creates a competing final entry before the final
 * absence gate, proving publication cannot replace it under fault injection. */
static int close_with_race(FILE *stream)
{int r=wrapped_fclose(stream);if(race_final){race_final=false;put("sdcard/CERBERUS/BLE/B00001.BLE","KEEP",4);}return r;}
#define stat(p,s) wrapped_stat(p,s)
#define mkdir wrapped_mkdir
#define open wrapped_open
#define fdopen wrapped_fdopen
#define close wrapped_close
#define fopen wrapped_fopen
#define fwrite wrapped_write
#define ferror wrapped_ferror
#define feof wrapped_feof
#define fflush wrapped_flush
#define fsync wrapped_sync
#define fileno native_fileno
#define fclose close_with_race
#define rename wrapped_rename
#define fread wrapped_read
#define fgetc wrapped_getc
#include "archive_storage_impl.h"
#undef stat
#undef mkdir
#undef open
#undef fdopen
#undef close
#undef fopen
#undef fwrite
#undef ferror
#undef feof
#undef fflush
#undef fsync
#undef fileno
#undef fclose
#undef rename
#undef fread
#undef fgetc

static uint8_t payload[CERBERUS_STORAGE_BLE_ARCHIVE_MAX_BYTES],output[CERBERUS_STORAGE_BLE_ARCHIVE_MAX_BYTES];
static char name[256];
static void setup(void)
{
    CHECK(!locked&&!streams&&!descriptors&&locks==unlocks);
    if(cases)CHECK(native_chdir("..")==0);
    char directory[32];snprintf(directory,sizeof(directory),"case%04u",++cases);
    CHECK(native_mkdir(directory)==0&&native_chdir(directory)==0&&native_mkdir("sdcard")==0);
    CHECK(native_mkdir("sdcard/CERBERUS")==0&&native_mkdir("sdcard/CERBERUS/BLE")==0);
    storage_mutex=&mutex_token;sd_mounted=true;storage_owner=CERBERUS_STORAGE_OWNER_FIRMWARE;
    sd_recon.active=wardrive_busy=fail_lock=fail_mutex=false;sd_wifi_capture_file=NULL;sd_ble_archive_next_index=1;
    extra_error=no_eof=short_read=collide_all=race_temp=race_final=false;zero_read_at=0;stat_size_delta=0;
    handoff_probes=unmounts=0;probe_handoff=false;
    memset(calls,0,sizeof(calls));memset(fault,0,sizeof(fault));name[0]=0;
    for(size_t i=0;i<sizeof(payload);++i)payload[i]=(uint8_t)(i*17U);
}
static bool exists(const char *file)
{char path[128];snprintf(path,sizeof(path),"sdcard/CERBERUS/BLE/%s",file);struct stat s;return stat(path,&s)==0;}
static void verify(const char *file,const void *expected,size_t length)
{
    char path[128];snprintf(path,sizeof(path),"sdcard/CERBERUS/BLE/%s",file);
    FILE*f=fopen(path,"rb");CHECK(f);CHECK(fread(output,1,sizeof(output),f)==length);
    CHECK(!memcmp(output,expected,length)&&fgetc(f)==EOF&&fclose(f)==0);
}
static esp_err_t save(size_t length){return cerberus_storage_ble_archive_save(payload,length,name,sizeof(name));}
static esp_err_t load(size_t *length)
{memset(output,0xA5,sizeof(output));*length=123;return cerberus_storage_ble_archive_load("/CERBERUS/BLE","B00001.BLE",output,sizeof(output),length);}
static void failure_output(size_t length){CHECK(length==0);for(size_t i=0;i<sizeof(output);++i)CHECK(output[i]==0);}
static void clean(void){CHECK(!locked&&!streams&&!descriptors&&locks==unlocks);}

static void check_full_v5_document(void)
{
    static cerberus_ble_archive_t document,decoded;
    static uint8_t original[CERBERUS_BLE_ARCHIVE_MAX_SIZE];
    CHECK(CERBERUS_STORAGE_BLE_ARCHIVE_MAX_BYTES==16928U&&CERBERUS_BLE_ARCHIVE_MAX_SIZE==16928U);
    memset(&document,0,sizeof(document));document.count=64;document.saved_uptime_ms=100;
    for(unsigned i=0;i<64;++i){
        cerberus_ble_archive_row_t *r=&document.rows[i];
        r->receiver=i<32?CERBERUS_BLE_ARCHIVE_RECEIVER_S3:CERBERUS_BLE_ARCHIVE_RECEIVER_C5;
        r->address[0]=(uint8_t)(i%32);r->address_type=(uint8_t)(i%4);r->index=(uint8_t)(i%32);
        r->rssi=-42;r->rssi_valid=true;r->scan_epoch_valid=true;r->scan_epoch=UINT64_C(0x1234567812345678);
        r->advertisement_length=3;memcpy(r->advertisement,"\002\001\006",3);
        if(i<32){
            r->source_time_valid=true;r->original_source_uptime_ms=90;r->copy_ceiling_ms=100;
            r->response_state=CERBERUS_BLE_ARCHIVE_RESPONSE_PRESENT;
            r->response_length=r->response_original_length=3;r->response_event_type=4;
            memcpy(r->response,"\002\011R",3);
            r->response_original_sequence=1;r->response_sequence=2;
            r->response_original_observed_ms=90;r->response_observed_ms=91;
            r->response_source_uptime_ms=91;r->response_time_valid=true;
        }else{
            r->rssi_kind=CERBERUS_BLE_ARCHIVE_RSSI_LATEST_ADV;
            r->receiver_boot_id=UINT64_C(0xabcdef0123456789);r->age_upper_valid=true;r->age_upper_at_save_ms=500+i;
            r->advertisement_length=31;
            r->advertisement_event_type=3;r->advertisement_flags=3;
            CHECK(archive_test_family_original(r->advertisement,&r->receive,&r->family,
                r->address,r->address_type,(i%2)?'5':'6'));
        }
    }
    setup();size_t encoded=0;
    CHECK(cerberus_ble_archive_encode(&document,payload,sizeof(payload),&encoded)&&encoded==16928U);
    CHECK(payload[8]==5&&payload[12]==8&&payload[13]==1);
    CHECK(payload[32+63*264+216]==1&&payload[32+63*264+220]==CERBERUS_BLE_HC_05);
    memcpy(original,payload,encoded);
    CHECK(save(encoded)==ESP_OK&&!strcmp(name,"B00001.BLE"));
    size_t got=0;CHECK(load(&got)==ESP_OK&&got==encoded&&!memcmp(output,payload,encoded));
    CHECK(cerberus_ble_archive_decode(output,got,&decoded)&&!memcmp(&decoded,&document,sizeof(document)));
    /* Failure publishing another full-size document leaves the prior mixed
     * snapshot and its complete family suffix untouched. */
    document.rows[63].receive.sid=1;
    CHECK(archive_test_family_original(document.rows[63].advertisement,&document.rows[63].receive,
        &document.rows[63].family,document.rows[63].address,document.rows[63].address_type,'6'));
    CHECK(cerberus_ble_archive_encode(&document,payload,sizeof(payload),&encoded));
    const enum event publish_failures[]={WRITE,FLUSH,SYNC,CLOSE,RENAME};
    for(unsigned n=0;n<sizeof(publish_failures)/sizeof(publish_failures[0]);++n){
        memset(fault,0,sizeof(fault));fault[publish_failures[n]]=calls[publish_failures[n]]+1;
        CHECK(save(encoded)!=ESP_OK);
        char final[16],temporary[16];snprintf(final,sizeof(final),"B%05u.BLE",n+2);
        snprintf(temporary,sizeof(temporary),"B%05u.TMP",n+2);
        CHECK(!exists(final)&&exists(temporary));
        verify("B00001.BLE",original,sizeof(original));clean();
    }
    /* A full-size file that grows by one byte is rejected before fopen. */
    memset(fault,0,sizeof(fault));stat_size_delta=1;
    unsigned opened=calls[FOPEN];CHECK(load(&got)==ESP_ERR_INVALID_SIZE&&calls[FOPEN]==opened);
    failure_output(got);clean();
}

int main(void)
{
    check_full_v5_document();
    for(unsigned length=1;length<=sizeof(payload);length=length<128?length+31:sizeof(payload)+1){
        setup();CHECK(save(length)==ESP_OK&&!strcmp(name,"B00001.BLE"));verify(name,payload,length);
        size_t got;CHECK(load(&got)==ESP_OK&&got==length&&!memcmp(output,payload,length));clean();
    }
    setup();CHECK(save(sizeof(payload))==ESP_OK);size_t got;short_read=true;
    CHECK(load(&got)==ESP_OK&&got==sizeof(payload)&&!memcmp(output,payload,got)&&calls[READ]>800);clean();
    setup();probe_handoff=true;CHECK(save(128)==ESP_OK&&load(&got)==ESP_OK&&got==128&&handoff_probes>10);
    probe_handoff=false;CHECK(cerberus_storage_reserve_for_usb()==ESP_OK&&unmounts==1&&!sd_mounted&&storage_owner==CERBERUS_STORAGE_OWNER_USB);clean();
    setup();put("sdcard/CERBERUS/BLE/B00001.BLE","KEEP",4);put("sdcard/CERBERUS/BLE/B00002.TMP","KEEP",4);
    CHECK(save(32)==ESP_OK&&!strcmp(name,"B00003.BLE"));verify("B00001.BLE","KEEP",4);verify("B00002.TMP","KEEP",4);
    setup();race_temp=true;CHECK(save(32)==ESP_OK&&!strcmp(name,"B00002.BLE"));verify("B00001.TMP","KEEP",4);
    setup();race_final=true;CHECK(save(32)!=ESP_OK&&!strcmp(name,"B00001.TMP"));verify("B00001.BLE","KEEP",4);verify("B00001.TMP",payload,32);
    setup();collide_all=true;CHECK(save(32)==ESP_ERR_NOT_FOUND&&calls[STAT]==1026&&!calls[OPEN]&&!name[0]);clean();
    setup();sd_ble_archive_next_index=99999;CHECK(save(32)==ESP_OK&&!strcmp(name,"B99999.BLE"));CHECK(save(32)==ESP_OK&&!strcmp(name,"B00001.BLE"));
    const enum event save_faults[]={STAT,OPEN,FDOPEN,WRITE,FLUSH,SYNC,CLOSE,RENAME};
    for(unsigned i=0;i<sizeof(save_faults)/sizeof(save_faults[0]);++i){
        setup();fault[save_faults[i]]=1;CHECK(save(61)!=ESP_OK);CHECK(!exists("B00001.BLE"));
        if(save_faults[i]>=FDOPEN){CHECK(exists("B00001.TMP")&&!strcmp(name,"B00001.TMP"));}
        CHECK(!calls[RENAME]||save_faults[i]==RENAME);clean();
        memset(fault,0,sizeof(fault));CHECK(save(61)==ESP_OK);clean();
    }
    for(unsigned nth=3;nth<=5;++nth){setup();fault[STAT]=nth;CHECK(save(61)!=ESP_OK&&!exists("B00001.BLE"));clean();}
    setup();CHECK(native_rmdir("sdcard/CERBERUS/BLE")==0);fault[MKDIR]=1;CHECK(save(32)!=ESP_OK&&!name[0]);clean();
    setup();extra_error=true;CHECK(save(32)!=ESP_OK&&!calls[RENAME]&&exists("B00001.TMP"));clean();
    setup();fault[FDOPEN]=1;fault[RAW_CLOSE]=1;CHECK(save(32)!=ESP_OK&&calls[RAW_CLOSE]==1);clean();
    for(unsigned owner=0;owner<7;++owner){
        setup();if(owner==0)sd_mounted=false;if(owner==1)storage_owner=CERBERUS_STORAGE_OWNER_USB;
        if(owner==2)sd_recon.active=true;if(owner==3)wardrive_busy=true;if(owner==4)sd_wifi_capture_file=(FILE*)1;
        if(owner==5)fail_lock=true;if(owner==6){storage_mutex=NULL;fail_mutex=true;}
        CHECK(save(32)!=ESP_OK&&!calls[STAT]&&!calls[OPEN]);CHECK(load(&got)!=ESP_OK);failure_output(got);clean();
    }
    const enum event read_faults[]={STAT,FOPEN,READ,GETC,CLOSE};
    for(unsigned i=0;i<sizeof(read_faults)/sizeof(read_faults[0]);++i){
        setup();put("sdcard/CERBERUS/BLE/B00001.BLE",payload,128);fault[read_faults[i]]=1;
        CHECK(load(&got)!=ESP_OK);failure_output(got);verify("B00001.BLE",payload,128);clean();
    }
    for(unsigned kind=0;kind<6;++kind){
        setup();put("sdcard/CERBERUS/BLE/B00001.BLE",payload,128);
        if(kind==0)stat_size_delta=1;if(kind==1)stat_size_delta=-1;if(kind==2)extra_error=true;
        if(kind==3)no_eof=true;if(kind==4){short_read=true;zero_read_at=3;}if(kind==5)stat_size_delta=CERBERUS_STORAGE_BLE_ARCHIVE_MAX_BYTES;
        CHECK(load(&got)!=ESP_OK);if(kind==5)CHECK(!calls[FOPEN]);failure_output(got);clean();
    }
    setup();CHECK(load(&got)==ESP_ERR_NOT_FOUND);failure_output(got);
    put("sdcard/CERBERUS/BLE/B00001.BLE",payload,0);CHECK(load(&got)==ESP_ERR_INVALID_SIZE);failure_output(got);
    setup();CHECK(native_mkdir("sdcard/COPIES")==0);put("sdcard/COPIES/snapshot.ble",payload,77);
    CHECK(cerberus_storage_ble_archive_load("/COPIES","snapshot.ble",output,sizeof(output),&got)==ESP_OK&&got==77&&!memcmp(output,payload,77));
    const char *bad[]={"../B00001.BLE","B00001.TMP","B00001.BLE/",".BLE",""};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i)CHECK(cerberus_storage_ble_archive_load("/COPIES",bad[i],output,sizeof(output),&got)==ESP_ERR_INVALID_ARG);
    CHECK(cerberus_storage_ble_archive_load("/../COPIES","snapshot.ble",output,sizeof(output),&got)==ESP_ERR_INVALID_ARG);
    CHECK(save(0)==ESP_ERR_INVALID_ARG&&save(sizeof(payload)+1)==ESP_ERR_INVALID_ARG);
    CHECK(cerberus_storage_ble_archive_save(payload,1,name,10)==ESP_ERR_INVALID_ARG);
    CHECK(cerberus_storage_ble_archive_load("/COPIES","snapshot.ble",output,1,&got)==ESP_ERR_INVALID_SIZE);clean();
    printf("Archive storage passed %u checks in %u isolated file cases; injected sync is not SD durability evidence.\n",checks,cases);
    return 0;
}
