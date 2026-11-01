#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stddef.h>
typedef int esp_err_t;
enum { ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_ERR_NO_MEM,
    ESP_ERR_TIMEOUT, ESP_ERR_NOT_SUPPORTED, ESP_FAIL };
#define CONFIG_BT_NIMBLE_ENABLED 1
/* Allocator placement is an SDK behavior; mock figures below test capture and
 * lifecycle only. They are not measured admission constants or predictions. */
enum { MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2, MALLOC_CAP_SPIRAM = 4 };
typedef unsigned portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0U
static unsigned lock_depth;
#define portENTER_CRITICAL(lock) do { (void)(lock); assert(!lock_depth); ++lock_depth; } while (0)
#define portEXIT_CRITICAL(lock) do { (void)(lock); assert(lock_depth == 1); --lock_depth; } while (0)
#define pdMS_TO_TICKS(value) (value)
#include "s3_wifi_owner.h"
static bool psram_ready, will_sync;
static uint32_t ext_total, ext_free, int_free;
static int64_t mock_clock;
static esp_err_t sdk_error;
static unsigned sdk_calls, task_calls, delay_calls, wifi_reads, race_at_read;
static cerberus_s3_wifi_owner_t wifi_owner;
static struct { void (*reset_cb)(int); void (*sync_cb)(void); } ble_hs_cfg;
static int ble_hs_util_ensure_addr(int ignored) { (void)ignored; return 0; }
#if CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
static bool esp_psram_is_initialized(void) { return psram_ready; }
static size_t heap_caps_get_total_size(uint32_t caps)
{ assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); return ext_total; }
#endif
static size_t heap_caps_get_free_size(uint32_t caps)
{
    assert(!lock_depth);
    return caps & MALLOC_CAP_SPIRAM ? ext_free : int_free;
}
static size_t heap_caps_get_minimum_free_size(uint32_t caps)
{ assert(!lock_depth); return (caps & MALLOC_CAP_SPIRAM ? ext_free : int_free) / 3U; }
static int64_t esp_timer_get_time(void) { return ++mock_clock; }
cerberus_s3_wifi_owner_t cerberus_s3_wifi_owner(void)
{
    if (++wifi_reads == race_at_read) wifi_owner = CERBERUS_S3_WIFI_SCAN;
    return wifi_owner;
}
static esp_err_t nimble_port_init(void)
{
    assert(!lock_depth); ++sdk_calls;
    if (sdk_error) return sdk_error;
#if CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
    int_free -= 10000U; ext_free -= 20000U;
#else
    int_free -= 30000U;
#endif
    return ESP_OK;
}
static void nimble_port_freertos_init(void (*fn)(void *))
{
    assert(fn); ++task_calls; int_free -= 4096U;
    if (will_sync) ble_hs_cfg.sync_cb();
}
static void nimble_port_run(void) { }
static void nimble_port_freertos_deinit(void) { }
static void vTaskDelay(unsigned ticks) { assert(ticks == 10); ++delay_calls; }

#include "../../s3_main/main/s3_ble_host.c"

static void reset_test(void)
{
    assert(!lock_depth);
    atomic_store(&current_owner, CERBERUS_S3_BLE_OWNER_NONE);
    atomic_store(&initialized, false); atomic_store(&synced, false); atomic_store(&host_epoch, 0);
    memset(&memory_diagnostics, 0, sizeof(memory_diagnostics)); memory_sequence = 0;
    memset(&ble_hs_cfg, 0, sizeof(ble_hs_cfg));
    psram_ready = will_sync = true; ext_total = 2097152U; ext_free = 1300000U; int_free = 105000U;
    mock_clock = 0; sdk_error = ESP_OK;
    sdk_calls = task_calls = delay_calls = wifi_reads = race_at_read = 0;
    wifi_owner = CERBERUS_S3_WIFI_NONE;
}
static void ownership_tests(void)
{
    reset_test();
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_NONE) == ESP_ERR_INVALID_ARG);
    wifi_owner = CERBERUS_S3_WIFI_SCAN;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_INVALID_STATE);
    assert(!sdk_calls && !memory_sequence);
    reset_test(); race_at_read = 2;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_CENTRAL) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE && !sdk_calls);
    reset_test();
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    unsigned sequence = memory_sequence;
    psram_ready = false; /* A failed foreign claim may not release this owner. */
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_CENTRAL) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_CENTRAL) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_SCANNER && memory_sequence == sequence);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_NONE) == ESP_ERR_INVALID_ARG);
}
static void missing_psram_tests(void)
{
    for (unsigned missing = 0; missing < 2; ++missing) {
        reset_test();
        if (!missing) psram_ready = false;
        else ext_total = 0;
        for (unsigned owner = CERBERUS_S3_BLE_OWNER_SCANNER; owner <= CERBERUS_S3_BLE_OWNER_CENTRAL; ++owner) {
#if CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
            assert(cerberus_s3_ble_host_claim((cerberus_s3_ble_owner_t)owner) == ESP_ERR_NO_MEM);
            assert(!sdk_calls && !task_calls && !cerberus_s3_ble_host_initialized());
            assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
            assert(memory_diagnostics.samples[CERBERUS_S3_BLE_MEMORY_REFUSED].requested_owner == owner);
#else
            assert(cerberus_s3_ble_host_claim((cerberus_s3_ble_owner_t)owner) == ESP_OK);
            assert(cerberus_s3_ble_host_release((cerberus_s3_ble_owner_t)owner) == ESP_OK);
#endif
        }
        psram_ready = true; ext_total = 2097152U;
        assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
        assert(sdk_calls == 1 && cerberus_s3_ble_host_initialized());
        assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    }
}
static void sdk_failure_and_warm_tests(void)
{
    reset_test(); sdk_error = ESP_ERR_NO_MEM; ext_free = 1U;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_NO_MEM);
    assert(sdk_calls == 1 && !task_calls && !cerberus_s3_ble_host_initialized());
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
    assert(memory_diagnostics.samples[CERBERUS_S3_BLE_MEMORY_AFTER_INIT].error == ESP_ERR_NO_MEM);
    /* No invented headroom threshold: the IDF error is preserved. */
    sdk_error = ESP_OK; ext_free = 1300000U;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    uint32_t retained_internal = int_free, retained_external = ext_free;
    uint32_t init_sequence = memory_diagnostics.samples[CERBERUS_S3_BLE_MEMORY_AFTER_INIT].sequence;
    for (unsigned i = 0; i < 12; ++i) {
        assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_CENTRAL) == ESP_OK);
        assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_CENTRAL) == ESP_OK);
    }
    assert(sdk_calls == 2 && task_calls == 1 && int_free == retained_internal && ext_free == retained_external);
    assert(memory_diagnostics.samples[CERBERUS_S3_BLE_MEMORY_AFTER_INIT].sequence == init_sequence);
    psram_ready = false;
#if CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_NO_MEM);
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
    assert(cerberus_s3_ble_host_initialized() && cerberus_s3_ble_host_synced() && sdk_calls == 2);
#else
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
#endif
    psram_ready = true;
    cerberus_s3_ble_memory_diagnostics_t copied;
    memset(&copied, 0xa5, sizeof(copied));
    cerberus_s3_ble_host_memory_diagnostics(&copied);
    assert(!memcmp(&copied, &memory_diagnostics, sizeof(copied)));
    assert(copied.samples[CERBERUS_S3_BLE_MEMORY_RELEASED].internal_free == retained_internal);
    assert(copied.samples[CERBERUS_S3_BLE_MEMORY_RELEASED].psram_free == retained_external);
    assert(copied.samples[CERBERUS_S3_BLE_MEMORY_RELEASED].valid &&
        copied.samples[CERBERUS_S3_BLE_MEMORY_RELEASED].initialized);
    cerberus_s3_ble_host_memory_diagnostics(NULL);
    memory_sequence = UINT32_MAX;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_ADVERTISER) == ESP_OK);
    assert(memory_sequence == UINT32_MAX);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_ADVERTISER) == ESP_OK);
    assert(memory_sequence == UINT32_MAX);
}
static void sync_timeout_and_reset_tests(void)
{
    reset_test(); will_sync = false;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_TIMEOUT);
    assert(sdk_calls == 1 && task_calls == 1 && delay_calls == 200);
    assert(cerberus_s3_ble_host_initialized() && !cerberus_s3_ble_host_synced());
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
    assert(memory_diagnostics.samples[CERBERUS_S3_BLE_MEMORY_AFTER_SYNC].error == ESP_ERR_TIMEOUT);
    ble_hs_cfg.sync_cb();
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_CENTRAL) == ESP_OK && sdk_calls == 1);
    ble_hs_cfg.reset_cb(123);
    assert(cerberus_s3_ble_host_epoch() == 1 && !cerberus_s3_ble_host_synced());
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
}
int main(void)
{
    ownership_tests(); missing_psram_tests(); sdk_failure_and_warm_tests(); sync_timeout_and_reset_tests();
    printf("S3 BLE host %s: ownership/race, missing PSRAM all owners/retry, SDK faults, warm residency, sync/reset and fixed diagnostics passed; diagnostics=%zu bytes.\n",
        CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL ? "EXTERNAL" : "INTERNAL", sizeof(memory_diagnostics));
    return 0;
}
