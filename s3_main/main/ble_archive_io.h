#pragma once

#include "ble_archive.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define CERBERUS_BLE_ARCHIVE_IO_PATH_CAPACITY 512U
#define CERBERUS_BLE_ARCHIVE_IO_NAME_CAPACITY 256U

typedef enum {
    CERBERUS_BLE_ARCHIVE_IO_IDLE = 0,
    CERBERUS_BLE_ARCHIVE_IO_STARTING,
    CERBERUS_BLE_ARCHIVE_IO_SAVING,
    CERBERUS_BLE_ARCHIVE_IO_LOADING,
    CERBERUS_BLE_ARCHIVE_IO_COMPLETE,
    CERBERUS_BLE_ARCHIVE_IO_ERROR,
    CERBERUS_BLE_ARCHIVE_IO_CANCELED,
} cerberus_ble_archive_io_state_t;
typedef enum {
    CERBERUS_BLE_ARCHIVE_IO_SAVE = 0,
    CERBERUS_BLE_ARCHIVE_IO_LOAD,
} cerberus_ble_archive_io_operation_t;
typedef struct {
    cerberus_ble_archive_io_state_t state;
    cerberus_ble_archive_io_operation_t operation;
    esp_err_t error;
    uint64_t job_id;
    bool worker_active, cancel_requested, result_ready;
    uint32_t bytes; /* Complete encoded/read bytes, not partial-write progress. */
    uint8_t count;
    char relative_path[CERBERUS_BLE_ARCHIVE_IO_PATH_CAPACITY];
    char file_name[CERBERUS_BLE_ARCHIVE_IO_NAME_CAPACITY];
} cerberus_ble_archive_io_status_t;

/* UI-facing calls do no filesystem or radio work. One job owns an immutable
 * PSRAM document and codec buffer; there is no internal-RAM fallback. Job IDs
 * never repeat within a boot. Release the previous job before starting another.
 * Save copies the complete document before returning; callers retain ownership. */
esp_err_t cerberus_ble_archive_io_save(const cerberus_ble_archive_t *document,
    uint64_t *job_id);
esp_err_t cerberus_ble_archive_io_load(const char *relative_path,
    const char *file_name, uint64_t *job_id);
void cerberus_ble_archive_io_get_status(cerberus_ble_archive_io_status_t *status);
/* Copies only a fully validated completed SAVE or LOAD, without changing output on
 * failure. The immutable result is pinned and copied outside the critical section;
 * a concurrent Release prevents new readers and defers disposal until all accepted
 * copies return. Historical data never becomes a live scan or active target. */
esp_err_t cerberus_ble_archive_io_copy_result(uint64_t job_id,
    cerberus_ble_archive_t *document);
/* Cancel before storage skips it. An in-flight filesystem call is not
 * preempted: a SAVE reports its actual COMPLETE/ERROR result and may publish;
 * a canceled LOAD discards its result. Failed saves retain their .TMP name.
 * Release during any active phase defers disposal until worker cleanup. These
 * operations are byte-bounded, not guaranteed filesystem-latency bounds. */
void cerberus_ble_archive_io_cancel(uint64_t job_id);
void cerberus_ble_archive_io_release(uint64_t job_id);
const char *cerberus_ble_archive_io_state_name(cerberus_ble_archive_io_state_t state);
