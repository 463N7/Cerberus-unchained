#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CERBERUS_S3_BLE_OWNER_NONE = 0,
    CERBERUS_S3_BLE_OWNER_SCANNER,
    CERBERUS_S3_BLE_OWNER_ADVERTISER,
    CERBERUS_S3_BLE_OWNER_CENTRAL,
} cerberus_s3_ble_owner_t;

/* One NimBLE host serves passive Recon scanning and the Radio Lab advertiser.
 * Claim before a GAP operation; release only after discovery/advertising ends. */
esp_err_t cerberus_s3_ble_host_claim(cerberus_s3_ble_owner_t owner);
esp_err_t cerberus_s3_ble_host_release(cerberus_s3_ble_owner_t owner);
cerberus_s3_ble_owner_t cerberus_s3_ble_host_owner(void);
bool cerberus_s3_ble_host_initialized(void);
bool cerberus_s3_ble_host_synced(void);
/* Changes whenever the host resets; an in-flight client operation becomes stale. */
unsigned cerberus_s3_ble_host_epoch(void);

typedef enum {
    CERBERUS_S3_BLE_MEMORY_BEFORE_INIT,
    CERBERUS_S3_BLE_MEMORY_AFTER_INIT,
    CERBERUS_S3_BLE_MEMORY_AFTER_SYNC,
    CERBERUS_S3_BLE_MEMORY_RELEASED,
    CERBERUS_S3_BLE_MEMORY_REFUSED,
    CERBERUS_S3_BLE_MEMORY_STAGE_COUNT,
} cerberus_s3_ble_memory_stage_t;

/* Process-wide heap observations at the named lifecycle boundary, not BLE-only
 * allocation totals. receipt_us is local monotonic receipt time, not RF time.
 * requested_owner identifies the caller, not an assertion of current ownership.
 * error is the admission/init/sync result at that phase; valid=false means no
 * sample. One fixed slot per phase; no packet data, heap allocation or SPI.
 * Largest-block walks remain in the explicit memory console command. */
typedef struct {
    int64_t receipt_us;
    uint32_t sequence;
    uint32_t internal_free, internal_minimum;
    uint32_t psram_free, psram_minimum;
    esp_err_t error;
    uint8_t requested_owner;
    bool initialized, synced, valid;
} cerberus_s3_ble_memory_sample_t;

typedef struct {
    cerberus_s3_ble_memory_sample_t samples[CERBERUS_S3_BLE_MEMORY_STAGE_COUNT];
} cerberus_s3_ble_memory_diagnostics_t;

void cerberus_s3_ble_host_memory_diagnostics(cerberus_s3_ble_memory_diagnostics_t *out);
