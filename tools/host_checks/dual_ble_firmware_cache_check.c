#include "c5_spi_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
static void *link_mutex=(void *)1;
static bool held,mutex_error;
static uint8_t sequence;
static uint32_t peer_firmware_version,reported_version=61;
static int64_t now_us=1000000,probe_us,request_us;
static unsigned probes,commands;
static esp_err_t probe_error,request_error;
static bool corrupt;
static int64_t esp_timer_get_time(void){return now_us;}
static int take_link_mutex(unsigned ticks){assert(ticks==5&&!held);if(mutex_error)return 0;held=true;return 1;}
static void give_link_mutex(void){assert(held);held=false;}
static esp_err_t get_status_v2_passive_locked(uint8_t seq,uint32_t deadline,cerberus_c5_status_t *out)
{(void)seq;assert(held&&deadline==1020);++probes;memset(out,0,sizeof(*out));out->firmware_version=reported_version;now_us+=probe_us;return probe_error;}
static esp_err_t spi_request_response_passive(const void *input,void *discard,void *output,size_t size,uint32_t deadline)
{
    (void)discard;assert(held&&deadline==1020&&size==sizeof(cerberus_spi_frame_t));++commands;
    const cerberus_spi_frame_t *request=input;assert(request->command==CERBERUS_SPI_COMMAND_DUAL_BLE_INFO);
    cerberus_dual_ble_info_t info={.boot_id=123,.next_job_id=1,.version=1,.capacity=32,.raw_bytes=31,.flags=8};
    assert(cerberus_protocol_prepare_frame(output,request->command,request->sequence,&info,sizeof(info)));
    if(corrupt)((cerberus_spi_frame_t *)output)->sequence++;
    now_us+=request_us;return request_error;
}
#include "c5_dual_ble_link_impl.h"
static esp_err_t run(cerberus_dual_ble_info_t *out)
{return c5_spi_link_dual_ble_exchange(CERBERUS_SPI_COMMAND_DUAL_BLE_INFO,NULL,0,out,sizeof(*out));}
static void reset(void)
{assert(!held);peer_firmware_version=0;reported_version=61;probes=commands=0;probe_error=request_error=ESP_OK;
 probe_us=request_us=0;now_us=1000000;mutex_error=corrupt=false;}
static bool zero(const void *p,size_t n){const unsigned char *b=p;for(size_t i=0;i<n;++i)if(b[i])return false;return true;}
int main(void)
{
    cerberus_dual_ble_info_t out;
    reset();peer_firmware_version=61;assert(run(&out)==ESP_OK&&commands==1&&!probes&&out.boot_id==123);
    reset();assert(run(&out)==ESP_OK&&probes==1&&commands==1&&peer_firmware_version==61);
    reset();probe_error=ESP_ERR_TIMEOUT;memset(&out,0xa5,sizeof(out));
    assert(run(&out)==ESP_ERR_TIMEOUT&&probes==1&&!commands&&!peer_firmware_version&&zero(&out,sizeof(out)));
    probe_error=ESP_OK;assert(run(&out)==ESP_OK&&probes==2&&commands==1); /* Transient loss recovers without false unsupported. */
    reset();reported_version=60;assert(run(&out)==ESP_ERR_NOT_SUPPORTED&&probes==1&&!commands&&zero(&out,sizeof(out)));
    now_us=1000000;assert(run(&out)==ESP_ERR_NOT_SUPPORTED&&probes==1&&!commands); /* Known old remains blocked. */
    reset();probe_us=20000;assert(run(&out)==ESP_ERR_TIMEOUT&&probes==1&&!commands&&!peer_firmware_version);
    reset();probe_us=-1;assert(run(&out)==ESP_ERR_TIMEOUT&&!commands);
    reset();probe_us=10000;request_us=10001;assert(run(&out)==ESP_ERR_TIMEOUT&&commands==1&&zero(&out,sizeof(out)));
    reset();probe_us=5000;request_us=5000;assert(run(&out)==ESP_OK&&commands==1);
    reset();probe_error=ESP_ERR_INVALID_RESPONSE;assert(run(&out)==ESP_ERR_INVALID_RESPONSE&&!commands);
    reset();corrupt=true;assert(run(&out)==ESP_ERR_INVALID_RESPONSE&&zero(&out,sizeof(out)));
    reset();mutex_error=true;assert(run(&out)==ESP_ERR_TIMEOUT&&!probes&&!commands);
    reset();assert(c5_spi_link_dual_ble_exchange(1,NULL,0,&out,sizeof(out))==ESP_ERR_INVALID_ARG&&!probes&&!commands);
    puts("Production firmware-cache recovery passed: known/unknown/old firmware, transient timeout recovery, shared deadline, invalid reply and no unintended command");
    return 0;
}
