#ifdef NDEBUG
#undef NDEBUG
#endif
#include "c5_spi_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define UI_WIFI_VISIBLE_ROWS 6U
#define UI_WIFI_HISTORY_SAMPLES 6U
static cerberus_c5_wifi_result_t wifi_results[UI_WIFI_VISIBLE_ROWS];
static cerberus_wifi_scan_status_payload_t wifi_scan_status;
static size_t wifi_result_count, wifi_page_offset;
static uint8_t wifi_channel_counts[256], wifi_channel_generation;
static int8_t wifi_channel_strongest[256];
static uint8_t wifi_channel_history_counts[UI_WIFI_HISTORY_SAMPLES][256];
static int8_t wifi_channel_history_strongest[UI_WIFI_HISTORY_SAMPLES][256];
static uint8_t wifi_channel_history_next_sample, wifi_channel_history_sample_count;
static uint8_t wifi_channel_history_last_generation, wifi_observed_generation;
static uint32_t wifi_observed_at_seconds;
static bool wifi_observation_valid, wifi_scan_checked, wifi_radio_stats_checked;
static esp_err_t wifi_scan_error, wifi_channel_error, wifi_analysis_error;
static struct { bool checked; } wifi_baseline;
static int64_t now_us;
static int64_t esp_timer_get_time(void) { return now_us; }
#include "cache_impl.h"

int main(void)
{
    cerberus_c5_wifi_result_t rows[CERBERUS_WIFI_SCAN_MAX_RESULTS] = {0}, before[CERBERUS_WIFI_SCAN_MAX_RESULTS];
    for (unsigned i = 0; i < CERBERUS_WIFI_SCAN_MAX_RESULTS; ++i) {
        rows[i].channel = i < 8 ? 6 : 36;
        rows[i].rssi = -90 + (int)i;
        rows[i].bssid[5] = (uint8_t)i;
        memcpy(rows[i].ssid, "AP\001", 4);
    }
    memcpy(before, rows, sizeof(rows));
    wifi_scan_status.generation = 255;
    wifi_page_offset = 12;
    wifi_baseline.checked = wifi_radio_stats_checked = true;
    now_us = INT64_C(5000000000000); /* Beyond the uint32 millisecond wrap. */
    wifi_commit_cached_results(rows, CERBERUS_WIFI_SCAN_MAX_RESULTS, 255);
    assert(!memcmp(before, rows, sizeof(rows)));
    assert(wifi_result_count == 6 && wifi_page_offset == 12);
    for (unsigned i = 0; i < 6; ++i) {
        assert(wifi_results[i].bssid[5] == i + 12);
        assert(!strcmp(wifi_results[i].ssid, "AP?"));
    }
    assert(wifi_channel_counts[6] == 8 && wifi_channel_counts[36] == CERBERUS_WIFI_SCAN_MAX_RESULTS - 8);
    assert(wifi_channel_strongest[6] == -83 && wifi_channel_strongest[36] == -90 + (int)CERBERUS_WIFI_SCAN_MAX_RESULTS - 1);
    assert(wifi_channel_strongest[1] == INT8_MIN);
    assert(wifi_observed_at_seconds == 5000000 && wifi_observed_generation == 255);
    assert(wifi_observation_valid && wifi_scan_checked && wifi_scan_error == ESP_OK && wifi_channel_error == ESP_OK);
    assert(!wifi_baseline.checked && !wifi_radio_stats_checked && wifi_analysis_error == ESP_ERR_INVALID_STATE);
    assert(wifi_channel_history_sample_count == 1);
    record_wifi_channel_history(false); /* Same legacy page must not add a sample. */
    assert(wifi_channel_history_sample_count == 1);
    /* A new publication with the same gen8 is distinct even after wrap. */
    rows[0].channel = 1;
    wifi_commit_cached_results(rows, 1, 255);
    assert(wifi_channel_history_sample_count == 2 && wifi_channel_history_last_generation == 255);
    assert(wifi_result_count == 1 && wifi_page_offset == 0 && wifi_channel_counts[1] == 1);
    assert(wifi_channel_counts[6] == 0 && wifi_channel_counts[36] == 0);
    for (unsigned i = 1; i < 6; ++i) {
        cerberus_c5_wifi_result_t zero = {0};
        assert(!memcmp(&wifi_results[i], &zero, sizeof(zero)));
    }
    wifi_scan_status.generation = 0;
    wifi_commit_cached_results(NULL, 0, 0);
    assert(wifi_result_count == 0 && wifi_page_offset == 0 && wifi_channel_generation == 0);
    for (unsigned i = 0; i < 256; ++i) assert(wifi_channel_counts[i] == 0 && wifi_channel_strongest[i] == INT8_MIN);
    for (unsigned i = 0; i < 8; ++i) wifi_commit_cached_results(rows, 1, 0);
    assert(wifi_channel_history_sample_count == UI_WIFI_HISTORY_SAMPLES && wifi_channel_history_next_sample == 5);
    puts("Wi-Fi cached publication: pagination, immutable SSIDs, channels, empty bank, gen8 reuse and uptime passed; no I/O linked.");
    return 0;
}
