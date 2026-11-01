#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

typedef int esp_err_t;
enum { ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_NO_MEM = 0x101,
    ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_ERR_TIMEOUT,
    ESP_ERR_INVALID_RESPONSE, ESP_ERR_NOT_SUPPORTED, ESP_ERR_WIFI_NOT_STARTED };
#define CONFIG_BT_NIMBLE_ENABLED 1
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
enum { MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2, MALLOC_CAP_SPIRAM = 4 };
typedef const void *esp_event_base_t;
typedef void *esp_event_handler_instance_t;
static const char wifi_event_base;
#define WIFI_EVENT ((const void *)&wifi_event_base)
enum { WIFI_EVENT_SCAN_DONE = 1, WIFI_STORAGE_RAM = 1, WIFI_MODE_STA = 1,
    WIFI_SCAN_TYPE_PASSIVE = 1 };
typedef struct { uint32_t status; } wifi_event_sta_scan_done_t;
typedef struct {
    int static_rx_buf_num, dynamic_rx_buf_num, dynamic_tx_buf_num, rx_ba_win;
    int ampdu_rx_enable, ampdu_tx_enable, amsdu_tx_enable;
} wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
    uint8_t channel;
    bool show_hidden;
    int scan_type;
    struct { unsigned passive; } scan_time;
} wifi_scan_config_t;
typedef enum {
    WIFI_AUTH_OPEN = 0, WIFI_AUTH_WEP = 1, WIFI_AUTH_WPA_PSK = 2,
    WIFI_AUTH_WPA2_PSK = 3, WIFI_AUTH_WPA_WPA2_PSK = 4, WIFI_AUTH_ENTERPRISE = 5,
    WIFI_AUTH_WPA3_PSK = 6, WIFI_AUTH_WPA2_WPA3_PSK = 7, WIFI_AUTH_OWE = 9,
    WIFI_AUTH_WPA3_ENT_192 = 10, WIFI_AUTH_WPA3_ENTERPRISE = 11,
    WIFI_AUTH_WPA2_WPA3_ENTERPRISE = 12, WIFI_AUTH_WPA_ENTERPRISE = 13,
} wifi_auth_mode_t;
typedef struct {
    uint8_t bssid[6], ssid[33], primary;
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_ap_record_t;

#include "s3_wifi_scanner.h"
#include "s3_wifi_owner.h"
#include "s3_ble_host.h"

static uint32_t mock_ms;
static unsigned alloc_count, alloc_calls, fail_alloc_call, init_calls, start_calls;
static unsigned stop_calls, scan_stop_calls, clear_calls, deinit_calls, delete_calls;
static unsigned ble_init_calls, ble_wifi_queries;
static size_t internal_heap = 131072U;
static bool task_fail, driver_initialized, driver_started, driver_scanning;
static bool emit_done, emitted_done, inject_stale, stale_emitted;
static bool cancel_in_wait, claim_wifi_on_second_ble_query;
static uint32_t done_status;
static const char *fail_stage;
static void (*queued_worker)(void *);
static void (*event_callback)(void *, esp_event_base_t, int32_t, void *);
static void *event_context;
static wifi_ap_record_t mock_records[32];
static uint16_t mock_total, mock_count;
static struct { void (*reset_cb)(int); void (*sync_cb)(void); } ble_hs_cfg;

static bool failing(const char *stage) { return fail_stage && !strcmp(stage, fail_stage); }
static int64_t esp_timer_get_time(void) { return (int64_t)mock_ms * 1000; }
static esp_err_t esp_netif_init(void) { return failing("netif") ? ESP_FAIL : ESP_OK; }
static esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
static size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return internal_heap; }
static void *heap_caps_calloc(size_t n, size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (++alloc_calls == fail_alloc_call) return NULL;
    void *p = calloc(n, size);
    assert(p);
    ++alloc_count;
    return p;
}
static void heap_caps_free(void *p) { if (p) { assert(alloc_count); --alloc_count; free(p); } }
static int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack,
    void *arg, unsigned priority, void *handle)
{
    assert(!strcmp(name, "s3_wifi_scan") && stack == 4096 && priority == 3 && !arg && !handle);
    if (task_fail) return 0;
    assert(!queued_worker);
    queued_worker = fn;
    return pdPASS;
}
static void vTaskDelete(void *task) { assert(!task); ++delete_calls; }
static void vTaskDelay(unsigned ticks)
{
    mock_ms += ticks;
    if (!driver_scanning) return;
    if (cancel_in_wait) {
        cerberus_s3_wifi_scan_status_t status;
        assert(cerberus_s3_wifi_scanner_get_status(&status) == ESP_OK);
        assert(cerberus_s3_wifi_scanner_stop(status.job_id) == ESP_OK);
        cancel_in_wait = false;
    }
    if (inject_stale && !stale_emitted) {
        wifi_event_sta_scan_done_t event = {0};
        event_callback((void *)((uintptr_t)event_context + 1U), WIFI_EVENT,
            WIFI_EVENT_SCAN_DONE, &event);
        stale_emitted = true;
    } else if (emit_done && !emitted_done) {
        wifi_event_sta_scan_done_t event = {done_status};
        event_callback(event_context, WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &event);
        emitted_done = true;
    }
}
static esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
    ++init_calls;
    assert(config->static_rx_buf_num == 4 && config->dynamic_rx_buf_num == 8 &&
        config->dynamic_tx_buf_num == 4 && config->rx_ba_win == 4 &&
        !config->ampdu_rx_enable && !config->ampdu_tx_enable && !config->amsdu_tx_enable);
    if (failing("init")) return ESP_ERR_NO_MEM;
    assert(!driver_initialized);
    driver_initialized = true;
    return ESP_OK;
}
static esp_err_t esp_wifi_set_storage(int value)
{ assert(value == WIFI_STORAGE_RAM); return failing("storage") ? ESP_FAIL : ESP_OK; }
static esp_err_t esp_wifi_set_mode(int value)
{ assert(value == WIFI_MODE_STA); return failing("mode") ? ESP_FAIL : ESP_OK; }
static esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
    void (*callback)(void *, esp_event_base_t, int32_t, void *), void *context,
    esp_event_handler_instance_t *handler)
{
    assert(base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE);
    if (failing("register")) return ESP_FAIL;
    event_callback = callback;
    event_context = context;
    *handler = (void *)(uintptr_t)1;
    return ESP_OK;
}
static esp_err_t esp_wifi_start(void)
{
    ++start_calls;
    if (failing("start")) return ESP_FAIL;
    driver_started = true;
    return ESP_OK;
}
static esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block)
{
    assert(!block && config->channel == 0 && config->show_hidden &&
        config->scan_type == WIFI_SCAN_TYPE_PASSIVE && config->scan_time.passive == 120);
    if (failing("scan")) return ESP_FAIL;
    driver_scanning = true;
    return ESP_OK;
}
static esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count)
{ if (failing("num")) return ESP_FAIL; *count = mock_total; return ESP_OK; }
static esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *rows)
{
    if (failing("records")) return ESP_FAIL;
    assert(*count == 32);
    memcpy(rows, mock_records, mock_count * sizeof(*rows));
    *count = mock_count;
    return ESP_OK;
}
static esp_err_t esp_wifi_scan_stop(void)
{ ++scan_stop_calls; driver_scanning = false; return failing("scan_stop") ? ESP_FAIL : ESP_OK; }
static esp_err_t esp_wifi_clear_ap_list(void)
{ ++clear_calls; return failing("clear") ? ESP_FAIL : ESP_OK; }
static esp_err_t esp_wifi_stop(void)
{
    ++stop_calls;
    if (failing("stop")) return ESP_FAIL;
    driver_started = driver_scanning = false;
    return ESP_OK;
}
static esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
    esp_event_handler_instance_t handler)
{
    assert(base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE && handler == (void *)(uintptr_t)1);
    if (failing("unregister")) return ESP_FAIL;
    event_callback = NULL;
    return ESP_OK;
}
static esp_err_t esp_wifi_deinit(void)
{
    ++deinit_calls;
    if (failing("deinit")) return ESP_FAIL;
    driver_initialized = driver_started = driver_scanning = false;
    return ESP_OK;
}
static int ble_hs_util_ensure_addr(int unused) { (void)unused; return 0; }
static void nimble_port_run(void) {}
static void nimble_port_freertos_deinit(void) {}
static esp_err_t nimble_port_init(void) { ++ble_init_calls; return ESP_OK; }
static void nimble_port_freertos_init(void (*task)(void *))
{ (void)task; ble_hs_cfg.sync_cb(); }

#include "../../s3_main/main/s3_wifi_owner.c"
static cerberus_s3_wifi_owner_t test_wifi_owner(void)
{
    ++ble_wifi_queries;
    if (claim_wifi_on_second_ble_query && ble_wifi_queries == 2U)
        assert(cerberus_s3_wifi_claim(CERBERUS_S3_WIFI_SCAN) == ESP_OK);
    return cerberus_s3_wifi_owner();
}
#define cerberus_s3_wifi_owner test_wifi_owner
#include "../../s3_main/main/s3_ble_host.c"
#undef cerberus_s3_wifi_owner
#include "../../s3_main/main/s3_wifi_scanner.c"

static void reset_all(void)
{
    assert(!scan_active && !queued_worker);
    heap_caps_free(scan_retained);
    heap_caps_free(scan_pending);
    heap_caps_free(scan_sdk_rows);
    scan_retained = scan_pending = NULL;
    scan_sdk_rows = NULL;
    memset(&scan_status, 0, sizeof(scan_status));
    scan_launching = scan_cancelled = scan_done = false;
    scan_next_job = scan_started_ms = scan_event_status = 0;
    atomic_store(&owner, CERBERUS_S3_WIFI_NONE);
    atomic_store(&current_owner, CERBERUS_S3_BLE_OWNER_NONE);
    atomic_store(&initialized, false);
    atomic_store(&synced, false);
    netif_ready = event_ready = false;
    mock_ms = alloc_calls = fail_alloc_call = 0;
    init_calls = start_calls = stop_calls = scan_stop_calls = clear_calls = deinit_calls = delete_calls = 0;
    ble_init_calls = ble_wifi_queries = 0;
    internal_heap = 131072;
    driver_initialized = driver_started = driver_scanning = task_fail = false;
    emit_done = true;
    emitted_done = inject_stale = stale_emitted = cancel_in_wait = claim_wifi_on_second_ble_query = false;
    done_status = 0;
    fail_stage = NULL;
    event_callback = NULL;
    event_context = NULL;
    memset(mock_records, 0, sizeof(mock_records));
    mock_total = mock_count = 1;
    mock_records[0] = (wifi_ap_record_t){.bssid = {2, 1, 2, 3, 4, 5},
        .ssid = "Example", .primary = 6, .rssi = -42, .authmode = 3};
    assert(alloc_count == 0);
}

static cerberus_s3_wifi_scan_status_t run_worker(void)
{
    assert(queued_worker);
    void (*fn)(void *) = queued_worker;
    queued_worker = NULL;
    fn(NULL);
    cerberus_s3_wifi_scan_status_t result;
    assert(cerberus_s3_wifi_scanner_get_status(&result) == ESP_OK);
    return result;
}

static void start_and_complete(void)
{
    cerberus_s3_wifi_scan_status_t status;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
    assert(status.state == CERBERUS_S3_WIFI_SCAN_RUNNING && status.radio_owned);
    status = run_worker();
    assert(status.state == CERBERUS_S3_WIFI_SCAN_COMPLETE && !status.radio_owned &&
        status.generation == status.job_id && status.count == 1 && status.total_count == 1);
    assert(!status.error_code && !status.cleanup_error);
    assert(cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_NONE && alloc_count == 1);
}

static void ownership_and_admission(void)
{
    reset_all();
    cerberus_s3_wifi_scan_status_t status;
    assert(cerberus_s3_wifi_scanner_start(NULL) == ESP_ERR_INVALID_ARG);
    const cerberus_s3_wifi_owner_t other[] = {CERBERUS_S3_WIFI_WEB,
        CERBERUS_S3_WIFI_PORTAL, CERBERUS_S3_WIFI_RECEIVER};
    for (unsigned i = 0; i < sizeof(other) / sizeof(other[0]); ++i) {
        assert(cerberus_s3_wifi_claim(other[i]) == ESP_OK);
        assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_INVALID_STATE);
        assert(cerberus_s3_wifi_owner() == other[i] && !init_calls);
        assert(cerberus_s3_wifi_release(other[i]) == ESP_OK);
    }
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_NONE && !init_calls);
    assert(cerberus_s3_ble_host_release(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_OK);
    internal_heap = 65535;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_NO_MEM);
    assert(!alloc_count && cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_NONE);
    internal_heap = 131072;
    for (unsigned i = 1; i <= 2; ++i) {
        alloc_calls = 0;
        fail_alloc_call = i;
        assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_NO_MEM);
        assert(!alloc_count && cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_NONE);
    }
    fail_alloc_call = 0;
    task_fail = true;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_NO_MEM);
    assert(status.state == CERBERUS_S3_WIFI_SCAN_ERROR && !status.radio_owned && !alloc_count);
    task_fail = false;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
    status = run_worker();
    assert(status.state == CERBERUS_S3_WIFI_SCAN_COMPLETE);
    reset_all();
    claim_wifi_on_second_ble_query = true;
    assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_INVALID_STATE);
    assert(!ble_init_calls && cerberus_s3_ble_host_owner() == CERBERUS_S3_BLE_OWNER_NONE);
    assert(cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_SCAN);
    assert(cerberus_s3_wifi_release(CERBERUS_S3_WIFI_SCAN) == ESP_OK);
}

static void successful_banks_and_cancel(void)
{
    reset_all();
    start_and_complete();
    cerberus_s3_wifi_scan_status_t first, current;
    cerberus_s3_wifi_scan_row_t row;
    assert(cerberus_s3_wifi_scanner_get_status(&first) == ESP_OK);
    assert(cerberus_s3_wifi_scanner_get_result(first.generation, 0, &row) == ESP_OK);
    assert(row.source == CERBERUS_S3_WIFI_SCAN_SOURCE_S3 && row.channel == 6 && row.rssi == -42 &&
        row.ssid_length == 7 && !memcmp(row.ssid, "Example", 7));
    assert(cerberus_s3_wifi_scanner_get_result(first.generation + 1, 0, &row) == ESP_ERR_INVALID_STATE);
    assert(!row.source && !row.ssid_length);
    assert(cerberus_s3_wifi_scanner_start(&current) == ESP_OK);
    assert(current.generation == first.generation && current.completed_ms == first.completed_ms);
    assert(cerberus_s3_wifi_scanner_stop(first.job_id) == ESP_ERR_INVALID_STATE);
    assert(cerberus_s3_wifi_scanner_stop(current.job_id) == ESP_OK);
    current = run_worker();
    assert(current.state == CERBERUS_S3_WIFI_SCAN_CANCELLED && current.generation == first.generation &&
        current.completed_ms == first.completed_ms && !current.radio_owned);
    assert(init_calls == 1 && alloc_count == 1);
    emitted_done = false;
    cancel_in_wait = true;
    assert(cerberus_s3_wifi_scanner_start(&current) == ESP_OK);
    current = run_worker();
    assert(current.state == CERBERUS_S3_WIFI_SCAN_CANCELLED && current.generation == first.generation);
    assert(scan_stop_calls == 1 && alloc_count == 1);
    emitted_done = false;
    inject_stale = true;
    mock_total = 9;
    mock_count = 4;
    mock_records[1] = mock_records[0]; /* Duplicate identity must not consume a row. */
    mock_records[2] = mock_records[0]; mock_records[2].primary = 36;
    mock_records[3] = mock_records[0]; mock_records[3].bssid[5] = 9;
    memcpy(mock_records[3].ssid, (uint8_t[]){1, '\\', 255, 0}, 4);
    assert(cerberus_s3_wifi_scanner_start(&current) == ESP_OK);
    current = run_worker();
    assert(stale_emitted && current.state == CERBERUS_S3_WIFI_SCAN_COMPLETE &&
        current.generation != first.generation && current.count == 2 &&
        current.total_count == 9 && current.dropped_count == 7 && current.truncated);
    assert(alloc_count == 1 && cerberus_s3_wifi_scanner_get_result(current.generation, 1, &row) == ESP_OK);
    char display[20], tiny[7];
    assert(cerberus_s3_wifi_scanner_ssid_display(&row, display, sizeof(display)) == 12);
    assert(!strcmp(display, "\\x01\\x5C\\xFF"));
    assert(cerberus_s3_wifi_scanner_ssid_display(&row, tiny, sizeof(tiny)) == 12 && !strcmp(tiny, "\\x01"));
    assert(cerberus_s3_wifi_scanner_ssid_display(&row, NULL, 0) == 12);
    assert(cerberus_s3_wifi_scanner_get_result(first.generation, 0, &row) == ESP_ERR_INVALID_STATE);
    emitted_done = false;
    inject_stale = false;
    mock_total = mock_count = 0;
    assert(cerberus_s3_wifi_scanner_start(&current) == ESP_OK);
    current = run_worker();
    assert(current.state == CERBERUS_S3_WIFI_SCAN_COMPLETE && current.generation && !current.count &&
        !current.total_count && !current.dropped_count && !current.truncated);
}

static void failure_and_cleanup(void)
{
    const char *setup[] = {"netif", "init", "storage", "mode", "register", "start", "scan", "num", "records"};
    for (unsigned i = 0; i < sizeof(setup) / sizeof(setup[0]); ++i) {
        reset_all();
        start_and_complete();
        uint32_t generation = scan_retained->generation;
        if (!strcmp(setup[i], "netif")) netif_ready = false;
        fail_stage = setup[i];
        emitted_done = false;
        cerberus_s3_wifi_scan_status_t status;
        assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
        status = run_worker();
        assert(status.state == CERBERUS_S3_WIFI_SCAN_ERROR && status.error_code && !status.cleanup_error);
        assert(status.generation == generation && alloc_count == 1 && !status.radio_owned &&
            cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_NONE);
    }
    reset_all();
    start_and_complete();
    uint32_t generation = scan_retained->generation;
    emit_done = false;
    cerberus_s3_wifi_scan_status_t status;
    mock_ms = UINT32_MAX - 100U;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
    status = run_worker();
    assert(status.error_code == ESP_ERR_TIMEOUT && status.elapsed_ms == CERBERUS_S3_WIFI_SCAN_WAIT_MS &&
        status.generation == generation && scan_stop_calls == 1 && !status.radio_owned);
    const char *cleanup[] = {"clear", "stop", "unregister", "deinit", "scan_stop"};
    for (unsigned i = 0; i < sizeof(cleanup) / sizeof(cleanup[0]); ++i) {
        reset_all();
        start_and_complete();
        generation = scan_retained->generation;
        fail_stage = cleanup[i];
        emitted_done = false;
        cancel_in_wait = !strcmp(cleanup[i], "scan_stop");
        assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
        status = run_worker();
        assert(status.state == CERBERUS_S3_WIFI_SCAN_ERROR && status.cleanup_error && status.radio_owned &&
            status.generation == generation && alloc_count == 1);
        assert(cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_SCAN);
        assert(cerberus_s3_wifi_scanner_stop(status.job_id) == status.cleanup_error);
        assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_INVALID_STATE);
        assert(cerberus_s3_ble_host_claim(CERBERUS_S3_BLE_OWNER_SCANNER) == ESP_ERR_INVALID_STATE);
    }
    reset_all();
    done_status = 1;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
    status = run_worker();
    assert(status.state == CERBERUS_S3_WIFI_SCAN_ERROR && status.error_code == ESP_FAIL && !status.generation);
    reset_all();
}

static void security_and_token_bounds(void)
{
    reset_all();
    const wifi_auth_mode_t modes[] = { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK,
        WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA3_PSK,
        WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_OWE, WIFI_AUTH_ENTERPRISE,
        WIFI_AUTH_WPA3_ENT_192, WIFI_AUTH_WPA3_ENTERPRISE, WIFI_AUTH_WPA2_WPA3_ENTERPRISE,
        WIFI_AUTH_WPA_ENTERPRISE, (wifi_auth_mode_t)255 };
    const uint8_t expected[] = { CERBERUS_WIFI_SECURITY_OPEN, CERBERUS_WIFI_SECURITY_WEP,
        CERBERUS_WIFI_SECURITY_WPA, CERBERUS_WIFI_SECURITY_WPA2, CERBERUS_WIFI_SECURITY_WPA_WPA2,
        CERBERUS_WIFI_SECURITY_WPA3, CERBERUS_WIFI_SECURITY_WPA2_WPA3, CERBERUS_WIFI_SECURITY_OWE,
        CERBERUS_WIFI_SECURITY_ENTERPRISE, CERBERUS_WIFI_SECURITY_ENTERPRISE,
        CERBERUS_WIFI_SECURITY_ENTERPRISE, CERBERUS_WIFI_SECURITY_ENTERPRISE,
        CERBERUS_WIFI_SECURITY_ENTERPRISE, CERBERUS_WIFI_SECURITY_OTHER };
    mock_count = mock_total = sizeof(modes) / sizeof(modes[0]);
    for (unsigned i = 0; i < mock_count; ++i) {
        mock_records[i] = mock_records[0];
        mock_records[i].bssid[5] = (uint8_t)i;
        mock_records[i].authmode = modes[i];
    }
    scan_next_job = UINT32_MAX - 1U;
    cerberus_s3_wifi_scan_status_t status;
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_OK);
    assert(status.job_id == UINT32_MAX);
    status = run_worker();
    assert(status.generation == UINT32_MAX && status.count == mock_count);
    for (unsigned i = 0; i < mock_count; ++i) {
        cerberus_s3_wifi_scan_row_t row;
        assert(cerberus_s3_wifi_scanner_get_result(status.generation, (uint8_t)i, &row) == ESP_OK);
        assert(row.security == expected[i]);
    }
    assert(cerberus_s3_wifi_scanner_start(&status) == ESP_ERR_INVALID_STATE);
    assert(scan_next_job == UINT32_MAX && cerberus_s3_wifi_owner() == CERBERUS_S3_WIFI_NONE);
    reset_all();
}

int main(void)
{
    ownership_and_admission();
    successful_banks_and_cancel();
    failure_and_cleanup();
    security_and_token_bounds();
    printf("S3 passive scan: ownership/race fence, cancellation, timeout/wrap, stable banks, SDK faults, cleanup and bounded SSID display passed. row=%zu bank=%zu status=%zu\n",
        sizeof(cerberus_s3_wifi_scan_row_t), sizeof(scan_bank_t), sizeof(cerberus_s3_wifi_scan_status_t));
    return 0;
}
