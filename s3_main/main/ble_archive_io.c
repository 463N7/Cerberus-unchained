#include "ble_archive_io.h"
#include "storage.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define BLE_ARCHIVE_IO_STACK_BYTES 6144U
#define BLE_ARCHIVE_IO_MIN_FREE_INTERNAL (24U * 1024U)
#define BLE_ARCHIVE_IO_MIN_LARGEST_INTERNAL (BLE_ARCHIVE_IO_STACK_BYTES + 2048U)

_Static_assert(CERBERUS_BLE_ARCHIVE_MAX_SIZE <= CERBERUS_STORAGE_BLE_ARCHIVE_MAX_BYTES,
    "Archive codec must fit its storage bound");
_Static_assert(CERBERUS_BLE_ARCHIVE_IO_PATH_CAPACITY == CERBERUS_STORAGE_PATH_MAX &&
    CERBERUS_BLE_ARCHIVE_IO_NAME_CAPACITY == CERBERUS_STORAGE_NAME_MAX,
    "Archive paths must fit storage validators");

typedef struct {
    cerberus_ble_archive_t document;
    uint8_t bytes[CERBERUS_BLE_ARCHIVE_MAX_SIZE];
    char relative_path[CERBERUS_BLE_ARCHIVE_IO_PATH_CAPACITY];
    char file_name[CERBERUS_BLE_ARCHIVE_IO_NAME_CAPACITY];
} ble_archive_io_context_t;
static portMUX_TYPE archive_io_lock = portMUX_INITIALIZER_UNLOCKED;
static ble_archive_io_context_t *archive_io_context;
static cerberus_ble_archive_io_status_t archive_io_status;
static uint64_t archive_io_last_job;
static bool archive_io_release_requested;
static uint16_t archive_io_readers;

static bool archive_io_string_fits(const char *value, size_t capacity)
{
    if (value == NULL) return false;
    for (size_t i = 0U; i < capacity; ++i) if (value[i] == '\0') return true;
    return false;
}

static bool archive_io_canceled(void)
{
    portENTER_CRITICAL(&archive_io_lock);
    bool canceled = archive_io_status.cancel_requested;
    portEXIT_CRITICAL(&archive_io_lock);
    return canceled;
}

static void archive_io_finish(ble_archive_io_context_t *context,
    cerberus_ble_archive_io_state_t state, esp_err_t error, size_t bytes)
{
    portENTER_CRITICAL(&archive_io_lock);
    /* Cancellation racing LOAD completion still prevents result publication.
     * A completed SAVE reports actual storage outcome; no rollback is implied. */
    if (state == CERBERUS_BLE_ARCHIVE_IO_COMPLETE &&
        archive_io_status.operation == CERBERUS_BLE_ARCHIVE_IO_LOAD &&
        archive_io_status.cancel_requested) state = CERBERUS_BLE_ARCHIVE_IO_CANCELED;
    archive_io_status.state = state;
    archive_io_status.error = error;
    archive_io_status.bytes = (uint32_t)bytes;
    archive_io_status.count = state == CERBERUS_BLE_ARCHIVE_IO_COMPLETE ? context->document.count : 0U;
    memcpy(archive_io_status.file_name, context->file_name, sizeof(context->file_name));
    archive_io_status.worker_active = false;
    bool dispose = archive_io_release_requested || state != CERBERUS_BLE_ARCHIVE_IO_COMPLETE;
    archive_io_status.result_ready = !dispose;
    if (dispose) archive_io_context = NULL;
    portEXIT_CRITICAL(&archive_io_lock);
    if (dispose) heap_caps_free(context);
}

static void ble_archive_io_worker(void *argument)
{
    ble_archive_io_context_t *context = argument;
    portENTER_CRITICAL(&archive_io_lock);
    bool save = archive_io_status.operation == CERBERUS_BLE_ARCHIVE_IO_SAVE;
    archive_io_status.state = save ? CERBERUS_BLE_ARCHIVE_IO_SAVING : CERBERUS_BLE_ARCHIVE_IO_LOADING;
    portEXIT_CRITICAL(&archive_io_lock);
    esp_err_t error = ESP_OK;
    size_t length = 0U;
    bool canceled = archive_io_canceled();
    if (!canceled && save && !cerberus_ble_archive_encode(&context->document,
            context->bytes, sizeof(context->bytes), &length)) error = ESP_ERR_INVALID_RESPONSE;
    canceled = canceled || archive_io_canceled();
    if (!canceled && error == ESP_OK) {
        if (save) error = cerberus_storage_ble_archive_save(context->bytes, length,
            context->file_name, sizeof(context->file_name));
        else error = cerberus_storage_ble_archive_load(context->relative_path,
            context->file_name, context->bytes, sizeof(context->bytes), &length);
        if (!save && archive_io_canceled()) canceled = true;
        if (!save && !canceled && error == ESP_OK &&
            !cerberus_ble_archive_decode(context->bytes, length, &context->document))
            error = ESP_ERR_INVALID_RESPONSE;
    }
    archive_io_finish(context, error != ESP_OK ? CERBERUS_BLE_ARCHIVE_IO_ERROR :
        canceled ? CERBERUS_BLE_ARCHIVE_IO_CANCELED : CERBERUS_BLE_ARCHIVE_IO_COMPLETE,
        error, error == ESP_OK && !canceled ? length : 0U);
    vTaskDelete(NULL);
}

static esp_err_t archive_io_start(const cerberus_ble_archive_t *document,
    const char *relative_path, const char *file_name, uint64_t *job_id)
{
    if (job_id != NULL) *job_id = 0U;
    bool save = document != NULL;
    if (job_id == NULL || (!save && (!archive_io_string_fits(relative_path, CERBERUS_BLE_ARCHIVE_IO_PATH_CAPACITY) ||
        !archive_io_string_fits(file_name, CERBERUS_BLE_ARCHIVE_IO_NAME_CAPACITY))))
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&archive_io_lock);
    if (archive_io_status.worker_active || archive_io_context != NULL || archive_io_readers ||
        archive_io_last_job == UINT64_MAX) {
        portEXIT_CRITICAL(&archive_io_lock);
        return ESP_ERR_INVALID_STATE;
    }
    memset(&archive_io_status, 0, sizeof(archive_io_status));
    archive_io_status.job_id = ++archive_io_last_job;
    *job_id = archive_io_status.job_id;
    archive_io_status.operation = save ? CERBERUS_BLE_ARCHIVE_IO_SAVE : CERBERUS_BLE_ARCHIVE_IO_LOAD;
    archive_io_status.state = CERBERUS_BLE_ARCHIVE_IO_STARTING;
    archive_io_status.worker_active = true;
    archive_io_release_requested = false;
    const char *path = save ? CERBERUS_STORAGE_BLE_ARCHIVE_PATH : relative_path;
    memcpy(archive_io_status.relative_path, path, strlen(path) + 1U);
    if (!save) memcpy(archive_io_status.file_name, file_name, strlen(file_name) + 1U);
    portEXIT_CRITICAL(&archive_io_lock);

    ble_archive_io_context_t *context = NULL;
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= BLE_ARCHIVE_IO_MIN_FREE_INTERNAL &&
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= BLE_ARCHIVE_IO_MIN_LARGEST_INTERNAL)
        context = heap_caps_calloc(1U, sizeof(*context), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (context != NULL) {
        if (save) context->document = *document;
        memcpy(context->relative_path, path, strlen(path) + 1U);
        if (!save) memcpy(context->file_name, file_name, strlen(file_name) + 1U);
        portENTER_CRITICAL(&archive_io_lock);
        archive_io_context = context;
        portEXIT_CRITICAL(&archive_io_lock);
        if (xTaskCreate(ble_archive_io_worker, "ble_archive", BLE_ARCHIVE_IO_STACK_BYTES,
                context, 2U, NULL) == pdPASS) return ESP_OK;
    }
    portENTER_CRITICAL(&archive_io_lock);
    archive_io_context = NULL;
    archive_io_status.worker_active = false;
    archive_io_status.state = CERBERUS_BLE_ARCHIVE_IO_ERROR;
    archive_io_status.error = ESP_ERR_NO_MEM;
    portEXIT_CRITICAL(&archive_io_lock);
    heap_caps_free(context);
    return ESP_ERR_NO_MEM;
}

esp_err_t cerberus_ble_archive_io_save(const cerberus_ble_archive_t *document, uint64_t *job_id)
{
    if (document == NULL) { if (job_id != NULL) *job_id = 0U; return ESP_ERR_INVALID_ARG; }
    return archive_io_start(document, NULL, NULL, job_id);
}
esp_err_t cerberus_ble_archive_io_load(const char *relative_path, const char *file_name, uint64_t *job_id)
{ return archive_io_start(NULL, relative_path, file_name, job_id); }

void cerberus_ble_archive_io_get_status(cerberus_ble_archive_io_status_t *status)
{
    if (status == NULL) return;
    portENTER_CRITICAL(&archive_io_lock);
    *status = archive_io_status;
    portEXIT_CRITICAL(&archive_io_lock);
}
esp_err_t cerberus_ble_archive_io_copy_result(uint64_t job_id, cerberus_ble_archive_t *document)
{
    if (document == NULL) return ESP_ERR_INVALID_ARG;
    ble_archive_io_context_t *borrowed = NULL;
    portENTER_CRITICAL(&archive_io_lock);
    if (job_id && job_id == archive_io_status.job_id && !archive_io_status.worker_active &&
        archive_io_status.result_ready && archive_io_context != NULL &&
        archive_io_status.state == CERBERUS_BLE_ARCHIVE_IO_COMPLETE && archive_io_readers != UINT16_MAX) {
        borrowed = archive_io_context;
        ++archive_io_readers;
    }
    portEXIT_CRITICAL(&archive_io_lock);
    if (borrowed == NULL) return ESP_ERR_INVALID_STATE;

    /* The completed document is immutable. Pin it while copying outside the
     * interrupt-disabled lock; release cannot free it or admit another job. */
    memcpy(document, &borrowed->document, sizeof(*document));
    ble_archive_io_context_t *dispose = NULL;
    portENTER_CRITICAL(&archive_io_lock);
    --archive_io_readers;
    if (!archive_io_readers && archive_io_release_requested && archive_io_context == borrowed) {
        dispose = archive_io_context;
        archive_io_context = NULL;
        archive_io_status.result_ready = false;
    }
    portEXIT_CRITICAL(&archive_io_lock);
    if (dispose != NULL) heap_caps_free(dispose);
    return ESP_OK;
}
void cerberus_ble_archive_io_cancel(uint64_t job_id)
{
    portENTER_CRITICAL(&archive_io_lock);
    if (job_id && job_id == archive_io_status.job_id && archive_io_status.worker_active)
        archive_io_status.cancel_requested = true;
    portEXIT_CRITICAL(&archive_io_lock);
}
void cerberus_ble_archive_io_release(uint64_t job_id)
{
    ble_archive_io_context_t *owned = NULL;
    portENTER_CRITICAL(&archive_io_lock);
    if (job_id && job_id == archive_io_status.job_id) {
        if (archive_io_status.worker_active) {
            archive_io_release_requested = true;
            archive_io_status.cancel_requested = true;
        } else if (archive_io_readers) {
            archive_io_release_requested = true;
            archive_io_status.result_ready = false;
        } else {
            owned = archive_io_context;
            archive_io_context = NULL;
            archive_io_status.result_ready = false;
        }
    }
    portEXIT_CRITICAL(&archive_io_lock);
    if (owned != NULL) heap_caps_free(owned);
}
const char *cerberus_ble_archive_io_state_name(cerberus_ble_archive_io_state_t state)
{
    static const char *const names[] = {"IDLE", "STARTING", "SAVING", "LOADING", "COMPLETE", "ERROR", "CANCELED"};
    return (unsigned)state < sizeof(names) / sizeof(names[0]) ? names[state] : "UNKNOWN";
}
