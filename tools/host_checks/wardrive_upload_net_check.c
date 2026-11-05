#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "wardrive_upload_net.h"
#include "s3_wifi_owner.h"
#include "s3_ble_host.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static unsigned calls,fail_call,allocs,stops,deinits,releases;
static int64_t clock_us;
static bool started,initialized,claimed,station_alive,sync_alive,stop_fail,ble_busy,cancel_now,ip_missing,sync_missing;
static bool response_incomplete,read_fail,write_fail;
static esp_netif_t iface;
static unsigned service,status=200,expected_length,used,read_at;
static char sent[8192],auth[300],content_type[100],api_key[150],filename[100];
static const char *response;
struct test_http {bool active;};static struct test_http client;
static esp_err_t call(void){return ++calls==fail_call?ESP_FAIL:ESP_OK;}
void test_enter(void){} void test_exit(void){}
void *heap_caps_malloc(size_t n,unsigned caps){assert(caps==6);void *p=malloc(n);if(p)++allocs;return p;}
void heap_caps_free(void *p){if(p){assert(allocs);--allocs;free(p);}}
int64_t esp_timer_get_time(void){return clock_us;}
void vTaskDelay(unsigned ms){clock_us+=(int64_t)ms*1000;}
cerberus_s3_ble_owner_t cerberus_s3_ble_host_owner(void){return ble_busy?CERBERUS_S3_BLE_OWNER_SCANNER:CERBERUS_S3_BLE_OWNER_NONE;}
esp_err_t cerberus_s3_wifi_claim(cerberus_s3_wifi_owner_t owner){assert(owner==CERBERUS_S3_WIFI_UPLOAD);if(claimed)return ESP_ERR_INVALID_STATE;claimed=true;return ESP_OK;}
esp_err_t cerberus_s3_wifi_release(cerberus_s3_wifi_owner_t owner){assert(owner==CERBERUS_S3_WIFI_UPLOAD&&claimed&&!initialized&&!station_alive);claimed=false;++releases;return ESP_OK;}
esp_err_t cerberus_s3_wifi_prepare(cerberus_s3_wifi_owner_t owner){assert(owner==CERBERUS_S3_WIFI_UPLOAD&&claimed);return call();}
esp_netif_t *esp_netif_create_default_wifi_sta(void){if(call()!=ESP_OK)return NULL;station_alive=true;return &iface;}
void esp_netif_destroy_default_wifi(esp_netif_t *p){assert(p==&iface&&station_alive&&!initialized);station_alive=false;}
esp_err_t esp_wifi_init(const wifi_init_config_t *c){assert(c->static_rx_buf_num==4&&c->dynamic_rx_buf_num==8);esp_err_t e=call();if(e==ESP_OK)initialized=true;return e;}
esp_err_t esp_wifi_deinit(void){assert(initialized&&!started);initialized=false;++deinits;return ESP_OK;}
esp_err_t esp_wifi_start(void){assert(initialized);esp_err_t e=call();if(e==ESP_OK)started=true;return e;}
esp_err_t esp_wifi_stop(void){assert(started);++stops;if(stop_fail){stop_fail=false;return ESP_FAIL;}started=false;return ESP_OK;}
esp_err_t esp_wifi_set_storage(int v){assert(v==WIFI_STORAGE_RAM);return call();}
esp_err_t esp_wifi_set_mode(int v){assert(v==WIFI_MODE_STA);return call();}
esp_err_t esp_wifi_set_ps(int v){assert(v==WIFI_PS_NONE);return call();}
esp_err_t esp_wifi_set_config(int v,const wifi_config_t *c){assert(v==WIFI_IF_STA&&!memcmp(c->sta.ssid,"fixture",7));return call();}
esp_err_t esp_wifi_connect(void){assert(started);return call();}
esp_err_t esp_netif_get_ip_info(esp_netif_t *p,esp_netif_ip_info_t *ip){assert(p==&iface);ip->ip.addr=ip_missing?0:1;return ESP_OK;}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *c){assert(!strcmp(c->server,"pool.ntp.org"));esp_err_t e=call();if(e==ESP_OK)sync_alive=true;return e;}
void esp_netif_sntp_deinit(void){assert(sync_alive);sync_alive=false;}
esp_err_t esp_netif_sntp_sync_wait(unsigned ms){assert(sync_alive);clock_us+=(int64_t)ms*1000;return sync_missing?ESP_ERR_TIMEOUT:ESP_OK;}
esp_err_t esp_crt_bundle_attach(void *p){(void)p;return ESP_OK;}
int mbedtls_base64_encode(unsigned char *out,size_t cap,size_t *n,const unsigned char *data,size_t len)
{
    static const char b[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if(cap<((len+2)/3)*4+1)return -1;*n=0;
    for(size_t i=0;i<len;i+=3){unsigned v=(unsigned)data[i]<<16;if(i+1<len)v|=(unsigned)data[i+1]<<8;if(i+2<len)v|=data[i+2];
        out[(*n)++]=b[(v>>18)&63];out[(*n)++]=b[(v>>12)&63];out[(*n)++]=i+1<len?b[(v>>6)&63]:'=';out[(*n)++]=i+2<len?b[v&63]:'=';}
    out[*n]=0;return 0;
}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c)
{
    assert(!client.active&&started&&sync_alive);assert(c->disable_auto_redirect&&c->crt_bundle_attach==esp_crt_bundle_attach&&c->method==HTTP_METHOD_POST);
    const char *urls[]={"https://api.wigle.net/api/v2/file/upload","https://wdgwars.pl/api/upload-csv","https://wardrift.net/v1/ingest/signals"};
    assert(!strcmp(c->url,urls[service])&&c->timeout_ms==10000&&c->buffer_size<=1024&&c->buffer_size_tx<=1024);
    client.active=true;used=read_at=0;sent[0]=auth[0]=content_type[0]=api_key[0]=filename[0]=0;return &client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value)
{assert(c==&client&&c->active);if(!strcmp(key,"Authorization"))strcpy(auth,value);
    else if(!strcmp(key,"Content-Type"))strcpy(content_type,value);else if(!strcmp(key,"X-API-Key"))strcpy(api_key,value);
    else if(!strcmp(key,"X-File-Name"))strcpy(filename,value);else assert(!"unknown header");return ESP_OK;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int length){assert(c==&client&&length>0&&length<8192);expected_length=(unsigned)length;return ESP_OK;}
int esp_http_client_write(esp_http_client_handle_t c,const char *bytes,int length)
{assert(c==&client&&length>0);if(write_fail)return -1;int n=length>17?17:length;assert(used+(unsigned)n<sizeof(sent));memcpy(sent+used,bytes,(size_t)n);used+=(unsigned)n;sent[used]=0;return n;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c){assert(c==&client&&used==expected_length);return (int64_t)strlen(response);}
int esp_http_client_get_status_code(esp_http_client_handle_t c){assert(c==&client);return (int)status;}
int esp_http_client_read(esp_http_client_handle_t c,char *buffer,int length){assert(c==&client);if(read_fail)return -1;
    size_t n=strlen(response)-read_at;if(n>(size_t)length)n=(size_t)length;if(n>7)n=7;memcpy(buffer,response+read_at,n);read_at+=(unsigned)n;return (int)n;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c){assert(c==&client);return !response_incomplete&&read_at==strlen(response);}
esp_err_t esp_http_client_close(esp_http_client_handle_t c){assert(c==&client);return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){assert(c==&client&&c->active);c->active=false;return ESP_OK;}
static bool canceled(void *p){(void)p;return cancel_now;}
static void progress(void *p,const char *stage,uint32_t sent_bytes,uint32_t total){(void)p;(void)stage;assert(sent_bytes<=total);}
int main(int argc,char **argv)
{
    assert(argc==2);cerberus_upload_config_t cfg={0};strcpy(cfg.ssid,"fixture");strcpy(cfg.password,"password");
    strcpy(cfg.wigle_name,"name");strcpy(cfg.wigle_token,"token");memset(cfg.wdgwars_token,'a',64);strcpy(cfg.wardrift_token,"fixture-token");
    for(unsigned failure=1;failure<=10;++failure){calls=0;fail_call=failure;assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)!=ESP_OK);
        assert(cerberus_upload_net_end()==ESP_OK&&!claimed&&!initialized&&!started&&!station_alive&&!sync_alive);}
    fail_call=0;ble_busy=true;assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)!=ESP_OK);assert(cerberus_upload_net_end()==ESP_OK);ble_busy=false;
    ip_missing=true;assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)==ESP_ERR_TIMEOUT);assert(cerberus_upload_net_end()==ESP_OK);ip_missing=false;
    sync_missing=true;assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)==ESP_ERR_TIMEOUT);assert(cerberus_upload_net_end()==ESP_OK);sync_missing=false;
    cancel_now=true;assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)!=ESP_OK&&!claimed);cancel_now=false;
    assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)==ESP_OK);
    FILE *f=fopen(argv[1],"rb");assert(f);cerberus_upload_file_t plan;assert(cerberus_upload_file_prepare(f,&plan,NULL,NULL));
    const char *responses[]={"{\"success\":true,\"results\":{\"transids\":[{\"transId\":\"test-123\"}]}}","{\"ok\":true,\"imported\":1}","{\"accepted\":true}"};
    for(service=0;service<3;++service){response=responses[service];unsigned http;
        cerberus_upload_reply_t r=cerberus_upload_net_send(&cfg,(cerberus_upload_service_t)service,f,&plan,0,"fixture-001.csv",canceled,progress,NULL,&http);
        assert(r.result==(service==0?CERBERUS_UPLOAD_QUEUED:CERBERUS_UPLOAD_ACCEPTED)&&http==200&&!client.active&&!allocs);
        if(service==0)assert(!strcmp(auth,"Basic bmFtZTp0b2tlbg==")&&!api_key[0]);
        else assert(!auth[0]&&!strcmp(api_key,service==1?cfg.wdgwars_token:cfg.wardrift_token));
        if(service==2){assert(!strcmp(content_type,"text/csv")&&!strcmp(filename,"fixture-001.csv"));assert(used==plan.summary.bytes);}
        else assert(strstr(content_type,"multipart/form-data")&&strstr(sent,"name=\"file\"")&&strstr(sent,"filename=\"fixture-001.csv\""));
        for(unsigned fault=0;fault<5;++fault){response_incomplete=fault==0;read_fail=fault==1;write_fail=fault==2;status=fault==3?302:200;cancel_now=fault==4;
            r=cerberus_upload_net_send(&cfg,(cerberus_upload_service_t)service,f,&plan,0,"fixture.csv",canceled,progress,NULL,&http);
            assert(r.result==CERBERUS_UPLOAD_UNKNOWN&&!client.active&&!allocs);}
        response_incomplete=read_fail=write_fail=cancel_now=false;status=200;
    }
    fclose(f);stop_fail=true;assert(cerberus_upload_net_end()!=ESP_OK&&claimed&&started&&initialized);
    assert(cerberus_upload_net_begin(&cfg,canceled,progress,NULL)!=ESP_OK);
    assert(cerberus_upload_net_end()==ESP_OK&&!claimed&&!station_alive&&!initialized&&!sync_alive);
    assert(stops&&deinits&&releases);puts("Actual network adapter: HTTPS/auth/body streaming, partial I/O, timeout/cancel and retained cleanup passed");return 0;
}
