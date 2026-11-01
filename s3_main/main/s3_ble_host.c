#include "s3_ble_host.h"
#include "s3_wifi_owner.h"
#include "sdkconfig.h"
#include <string.h>

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#if CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
#include "esp_psram.h"
#endif
#include <stdatomic.h>

static atomic_int current_owner;
static atomic_bool initialized;
static atomic_bool synced;
static atomic_uint host_epoch;
static portMUX_TYPE memory_lock = portMUX_INITIALIZER_UNLOCKED;
static cerberus_s3_ble_memory_diagnostics_t memory_diagnostics;
static uint32_t memory_sequence;

_Static_assert(sizeof(cerberus_s3_ble_memory_diagnostics_t) <= 256U,
    "BLE memory diagnostics must remain a small fixed snapshot");

static void memory_capture(cerberus_s3_ble_memory_stage_t stage,
    cerberus_s3_ble_owner_t requested_owner, esp_err_t error)
{
    const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const uint32_t external = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    cerberus_s3_ble_memory_sample_t sample = {
        .receipt_us = esp_timer_get_time(),
        .internal_free = heap_caps_get_free_size(internal),
        .internal_minimum = heap_caps_get_minimum_free_size(internal),
        .psram_free = heap_caps_get_free_size(external),
        .psram_minimum = heap_caps_get_minimum_free_size(external),
        .error = error, .requested_owner = (uint8_t)requested_owner,
        .initialized = atomic_load(&initialized), .synced = atomic_load(&synced), .valid = true,
    };
    portENTER_CRITICAL(&memory_lock);
    if (memory_sequence != UINT32_MAX) ++memory_sequence;
    sample.sequence = memory_sequence; /* Saturating diagnostic ordinal, not a job identity. */
    memory_diagnostics.samples[stage] = sample;
    portEXIT_CRITICAL(&memory_lock);
}

void cerberus_s3_ble_host_memory_diagnostics(cerberus_s3_ble_memory_diagnostics_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&memory_lock);
    *out = memory_diagnostics;
    portEXIT_CRITICAL(&memory_lock);
}

static void host_sync(void)
{
    atomic_store(&synced, ble_hs_util_ensure_addr(0) == 0);
}

static void host_reset(int reason)
{
    (void)reason;
    atomic_store(&synced, false);
    atomic_fetch_add(&host_epoch, 1U);
    /* GAP procedures and connections are gone after a controller reset. */
    atomic_store(&current_owner, CERBERUS_S3_BLE_OWNER_NONE);
}

static void host_task(void *unused)
{
    (void)unused;
    nimble_port_run();
    atomic_store(&synced, false);
    /* The host is intentionally kept for this firmware session. A stopped
     * host cannot be silently reused by an already initialized scanner. */
    nimble_port_freertos_deinit();
}

cerberus_s3_ble_owner_t cerberus_s3_ble_host_owner(void)
{
    return (cerberus_s3_ble_owner_t)atomic_load(&current_owner);
}

bool cerberus_s3_ble_host_initialized(void) { return atomic_load(&initialized); }
bool cerberus_s3_ble_host_synced(void) { return atomic_load(&synced); }
unsigned cerberus_s3_ble_host_epoch(void) { return atomic_load(&host_epoch); }

esp_err_t cerberus_s3_ble_host_claim(cerberus_s3_ble_owner_t owner)
{
    if (owner != CERBERUS_S3_BLE_OWNER_SCANNER && owner != CERBERUS_S3_BLE_OWNER_ADVERTISER &&
        owner != CERBERUS_S3_BLE_OWNER_CENTRAL)
        return ESP_ERR_INVALID_ARG;
    if (cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_SCAN) return ESP_ERR_INVALID_STATE;
    int expected = CERBERUS_S3_BLE_OWNER_NONE;
    if (!atomic_compare_exchange_strong(&current_owner, &expected, owner))
        return ESP_ERR_INVALID_STATE;
    /* The Wi-Fi scanner claims first and then checks BLE ownership. This
     * second check closes a claim race before any controller/GAP work starts. */
    if (cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_SCAN) {
        expected = owner;
        (void)atomic_compare_exchange_strong(&current_owner, &expected, CERBERUS_S3_BLE_OWNER_NONE);
        return ESP_ERR_INVALID_STATE;
    }
#if CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL
    /* EXTERNAL is external-only. Missing optional PSRAM must not enter IDF's
     * allocator from any common host owner. This checks presence, not future
     * allocation success; IDF's existing exhaustion behavior is unchanged. */
    if (!esp_psram_is_initialized() ||
        !heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) {
        memory_capture(CERBERUS_S3_BLE_MEMORY_REFUSED, owner, ESP_ERR_NO_MEM);
        expected = owner;
        (void)atomic_compare_exchange_strong(&current_owner, &expected, CERBERUS_S3_BLE_OWNER_NONE);
        return ESP_ERR_NO_MEM;
    }
#endif
    if (!atomic_load(&initialized)) {
        memory_capture(CERBERUS_S3_BLE_MEMORY_BEFORE_INIT, owner, ESP_OK);
        esp_err_t err = nimble_port_init();
        memory_capture(CERBERUS_S3_BLE_MEMORY_AFTER_INIT, owner, err);
        if (err != ESP_OK) {
            atomic_store(&current_owner, CERBERUS_S3_BLE_OWNER_NONE);
            return err;
        }
        ble_hs_cfg.reset_cb = host_reset;
        ble_hs_cfg.sync_cb = host_sync;
        atomic_store(&initialized, true);
        nimble_port_freertos_init(host_task);
    }
    for (unsigned i = 0; i < 200U && !atomic_load(&synced); ++i)
        vTaskDelay(pdMS_TO_TICKS(10));
    if (!atomic_load(&synced)) {
        memory_capture(CERBERUS_S3_BLE_MEMORY_AFTER_SYNC, owner, ESP_ERR_TIMEOUT);
        atomic_store(&current_owner, CERBERUS_S3_BLE_OWNER_NONE);
        return ESP_ERR_TIMEOUT;
    }
    memory_capture(CERBERUS_S3_BLE_MEMORY_AFTER_SYNC, owner, ESP_OK);
    return ESP_OK;
}

esp_err_t cerberus_s3_ble_host_release(cerberus_s3_ble_owner_t owner)
{
    if (owner != CERBERUS_S3_BLE_OWNER_SCANNER && owner != CERBERUS_S3_BLE_OWNER_ADVERTISER &&
        owner != CERBERUS_S3_BLE_OWNER_CENTRAL)
        return ESP_ERR_INVALID_ARG;
    int expected = owner;
    if (!atomic_compare_exchange_strong(&current_owner, &expected, CERBERUS_S3_BLE_OWNER_NONE))
        return ESP_ERR_INVALID_STATE;
    memory_capture(CERBERUS_S3_BLE_MEMORY_RELEASED, owner, ESP_OK);
    return ESP_OK;
}
#else
cerberus_s3_ble_owner_t cerberus_s3_ble_host_owner(void) { return CERBERUS_S3_BLE_OWNER_NONE; }
bool cerberus_s3_ble_host_initialized(void) { return false; }
bool cerberus_s3_ble_host_synced(void) { return false; }
unsigned cerberus_s3_ble_host_epoch(void) { return 0U; }
esp_err_t cerberus_s3_ble_host_claim(cerberus_s3_ble_owner_t owner)
{ (void)owner; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t cerberus_s3_ble_host_release(cerberus_s3_ble_owner_t owner)
{ (void)owner; return ESP_ERR_NOT_SUPPORTED; }
void cerberus_s3_ble_host_memory_diagnostics(cerberus_s3_ble_memory_diagnostics_t *out)
{ if (out) memset(out, 0, sizeof(*out)); }
#endif
