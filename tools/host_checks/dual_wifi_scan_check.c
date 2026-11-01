#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "dual_wifi_scan.c"

static uint32_t clock_ms, local_job, local_complete_at, c5_complete_at;
static uint64_t epoch, lease;
static uint8_t requested_band;
static cerberus_s3_wifi_scan_status_t local_status;
static cerberus_wifi_scan_status_payload_t remote_status;
static cerberus_s3_wifi_scan_row_t local_rows[32];
static cerberus_c5_wifi_result_t remote_rows[32];
static unsigned local_starts, local_stops, remote_starts, remote_stops, info_reads, status_reads;
static esp_err_t info_error, local_start_error, start_reply_error, status_error, row_error, proof_error;
static bool allocation_fails, reject_start, fail_local_cleanup, keep_remote_running;
static bool replace_local_on_row, replace_bank_on_proof, change_status_fence;

int64_t esp_timer_get_time(void) { return (int64_t)clock_ms * 1000; }
cerberus_s3_wifi_owner_t cerberus_s3_wifi_owner(void)
{ return local_status.radio_owned ? CERBERUS_S3_WIFI_SCAN : CERBERUS_S3_WIFI_NONE; }
/* Aggregate publication is covered with the real module by the diagnostics suite. */
void cerberus_dual_wifi_diagnostics_publish(
    const cerberus_dual_wifi_diagnostic_values_t *value, int64_t now_us)
{ assert(value && now_us == (int64_t)clock_ms * 1000); }
void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{ assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); return allocation_fails ? NULL : calloc(count, size); }

static void refresh_local(void)
{
    if (local_status.state == CERBERUS_S3_WIFI_SCAN_RUNNING && clock_ms >= local_complete_at) {
        local_status.state = CERBERUS_S3_WIFI_SCAN_COMPLETE;
        local_status.generation = local_status.job_id;
        local_status.completed_ms = local_complete_at;
        local_status.radio_owned = false;
    }
}
esp_err_t cerberus_s3_wifi_scanner_start(cerberus_s3_wifi_scan_status_t *out)
{
    ++local_starts; memset(out, 0, sizeof(*out));
    if (local_start_error != ESP_OK) return local_start_error;
    local_status.job_id = ++local_job;
    local_status.state = CERBERUS_S3_WIFI_SCAN_RUNNING;
    local_status.radio_owned = true;
    local_status.cleanup_error = ESP_OK;
    *out = local_status; return ESP_OK;
}
esp_err_t cerberus_s3_wifi_scanner_get_status(cerberus_s3_wifi_scan_status_t *out)
{ refresh_local(); *out = local_status; return ESP_OK; }
esp_err_t cerberus_s3_wifi_scanner_stop(uint32_t job)
{
    assert(job == local_status.job_id); ++local_stops;
    if (fail_local_cleanup) { local_status.cleanup_error = ESP_FAIL; return ESP_FAIL; }
    local_status.state = CERBERUS_S3_WIFI_SCAN_CANCELLED;
    local_status.radio_owned = false; return ESP_OK;
}
esp_err_t cerberus_s3_wifi_scanner_get_result(uint32_t generation, uint8_t index,
    cerberus_s3_wifi_scan_row_t *out)
{
    if (generation != local_status.generation || index >= local_status.count) return ESP_ERR_INVALID_STATE;
    *out = local_rows[index];
    if (replace_local_on_row) { ++local_status.job_id; ++local_status.generation; }
    return ESP_OK;
}
static void refresh_remote(void)
{
    if (remote_status.state == CERBERUS_WIFI_SCAN_RUNNING && !keep_remote_running && clock_ms >= c5_complete_at)
        remote_status.state = CERBERUS_WIFI_SCAN_COMPLETE;
}
esp_err_t c5_spi_link_wifi_scan_band_info(cerberus_wifi_scan_band_response_t *out)
{
    ++info_reads; memset(out, 0, sizeof(*out));
    out->session_id = epoch; out->status = remote_status;
    return info_error;
}
esp_err_t c5_spi_link_start_wifi_scan_band(uint8_t band, uint64_t session,
    cerberus_wifi_scan_status_payload_t *out)
{
    ++remote_starts; assert(local_starts || store->status.rebinding);
    assert(session == epoch + 1U);
    requested_band = band; memset(out, 0, sizeof(*out));
    if (reject_start) return start_reply_error;
    epoch = lease = session; ++remote_status.generation;
    remote_status.state = CERBERUS_WIFI_SCAN_RUNNING;
    *out = remote_status; return start_reply_error;
}
esp_err_t c5_spi_link_wifi_scan_band_status(uint8_t band, uint64_t session,
    cerberus_wifi_scan_status_payload_t *out)
{
    ++status_reads; memset(out, 0, sizeof(*out));
    if (status_error) return status_error;
    if (session != lease || session != epoch || band != requested_band) return ESP_ERR_NOT_FOUND;
    refresh_remote(); *out = remote_status; return ESP_OK;
}
esp_err_t c5_spi_link_stop_wifi_scan_band(uint8_t band, uint64_t session,
    cerberus_wifi_scan_status_payload_t *out)
{
    assert(session == lease && session == epoch && band == requested_band);
    ++remote_stops;
    remote_status.state = CERBERUS_WIFI_SCAN_IDLE;
    *out = remote_status; return ESP_OK;
}
esp_err_t c5_spi_link_get_wifi_scan_status_once(uint32_t deadline,
    cerberus_wifi_scan_status_payload_t *out)
{
    assert(clock_ms < deadline); *out = remote_status;
    if (change_status_fence) ++out->generation;
    return ESP_OK;
}
esp_err_t c5_spi_link_wifi_relation_begin_check(cerberus_c5_wifi_relation_reader_t *reader,
    uint8_t generation, uint32_t deadline)
{
    memset(reader, 0, sizeof(*reader)); reader->generation = generation; reader->deadline_ms = deadline;
    reader->pending.status.epoch = epoch; return ESP_OK;
}
esp_err_t c5_spi_link_wifi_relation_begin_check_expected(cerberus_c5_wifi_relation_reader_t *reader,
    const cerberus_c5_wifi_relation_bank_t *expected, uint32_t deadline)
{
    memset(reader, 0, sizeof(*reader)); reader->deadline_ms = deadline;
    reader->expect_bank = true; reader->expected = *expected; return ESP_OK;
}
esp_err_t c5_spi_link_wifi_relation_step(cerberus_c5_wifi_relation_reader_t *reader,
    cerberus_c5_wifi_relation_bank_t *proof, bool *complete,
    cerberus_c5_relation_continue_fn callback, void *context)
{
    (void)callback; (void)context;
    assert(clock_ms < reader->deadline_ms); *complete = false;
    if (proof_error) return proof_error;
    if (reader->expect_bank) {
        if (replace_bank_on_proof) ++epoch;
        if (reader->expected.status.epoch != epoch) return ESP_ERR_INVALID_STATE;
        *proof = reader->expected;
    } else *proof = reader->pending;
    *complete = true; return ESP_OK;
}
void c5_spi_link_wifi_relation_cancel(cerberus_c5_wifi_relation_reader_t *reader)
{ memset(reader, 0, sizeof(*reader)); }
esp_err_t c5_spi_link_wifi_scan_result_begin(cerberus_c5_wifi_scan_reader_t *reader,
    uint8_t generation, uint8_t index, uint32_t deadline)
{
    memset(reader, 0, sizeof(*reader)); reader->generation = generation; reader->index = index;
    reader->deadline_ms = deadline; return ESP_OK;
}
esp_err_t c5_spi_link_wifi_scan_result_step(cerberus_c5_wifi_scan_reader_t *reader,
    cerberus_c5_wifi_result_t *out, cerberus_s3_wifi_observation_t *observation, bool *complete)
{
    assert(clock_ms < reader->deadline_ms && reader->index < remote_status.stored_count);
    *complete = false; if (row_error) return row_error;
    if (++reader->phase < 3U) return ESP_OK; /* Incremental, not one blocking bank read. */
    *out = remote_rows[reader->index];
    observation->valid = true; observation->request_started_us = (uint64_t)clock_ms * 1000;
    *complete = true; return ESP_OK;
}
void c5_spi_link_wifi_scan_result_cancel(cerberus_c5_wifi_scan_reader_t *reader)
{ memset(reader, 0, sizeof(*reader)); }

static void reset(void)
{
    free(store); store = NULL; clock_ms = 1000; epoch = 100; lease = 0; local_job = 100;
    local_complete_at = 1300; c5_complete_at = 1400;
    requested_band = 255;
    local_starts = local_stops = remote_starts = remote_stops = info_reads = status_reads = 0;
    info_error = local_start_error = start_reply_error = status_error = row_error = proof_error = ESP_OK;
    allocation_fails = reject_start = fail_local_cleanup = keep_remote_running = false;
    replace_local_on_row = replace_bank_on_proof = change_status_fence = false;
    memset(&local_status, 0, sizeof(local_status)); memset(&remote_status, 0, sizeof(remote_status));
    memset(local_rows, 0, sizeof(local_rows)); memset(remote_rows, 0, sizeof(remote_rows));
    local_status.count = remote_status.stored_count = 32;
    local_status.total_count = remote_status.total_count = 40;
    for (uint8_t i = 0; i < 32; ++i) {
        local_rows[i].bssid[0] = 2; local_rows[i].bssid[5] = i;
        local_rows[i].ssid_length = 3; memcpy(local_rows[i].ssid, "S3x", 3);
        local_rows[i].channel = 1; local_rows[i].security = CERBERUS_WIFI_SECURITY_WPA2;
        local_rows[i].source = CERBERUS_S3_WIFI_SCAN_SOURCE_S3;
        remote_rows[i].bssid[0] = 4; remote_rows[i].bssid[5] = i;
        strcpy(remote_rows[i].ssid, "C5x"); remote_rows[i].channel = 36;
        remote_rows[i].security = CERBERUS_WIFI_SECURITY_WPA2;
    }
}
static void tick(void) { clock_ms += 20; cerberus_dual_wifi_scan_step(); }
static void drain(void) { unsigned limit = 1500; while (dual_active() && limit--) tick(); assert(!dual_active()); }
static void until_phase(uint8_t phase)
{ unsigned limit = 1500; while (store->phase != phase && dual_active() && limit--) tick(); assert(store->phase == phase); }
static cerberus_dual_wifi_status_t status(void)
{ cerberus_dual_wifi_status_t result; cerberus_dual_wifi_scan_get_status(&result); return result; }
static void verify_retained(uint32_t generation, unsigned count)
{ assert(status().generation == generation && status().count == count); }

int main(void)
{
    reset(); allocation_fails = true;
    assert(cerberus_dual_wifi_scan_start() == ESP_ERR_NO_MEM && !info_reads && !local_starts && !remote_starts);
    reset(); info_error = ESP_ERR_NOT_SUPPORTED;
    assert(cerberus_dual_wifi_scan_start() == ESP_ERR_NOT_SUPPORTED && !local_starts && !remote_starts);
    reset(); assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().state == CERBERUS_DUAL_WIFI_COMPLETE && status().split && !status().cleanup_pending);
    assert(requested_band == CERBERUS_WIFI_SCAN_BAND_5G && local_starts == 1 && remote_starts == 1);
    assert(status().s3_count == 32 && status().c5_count == 32 && status().s3_total == 40 && status().c5_total == 40);
    verify_retained(1, 64);
    for (uint8_t i = 0; i < 64; ++i) {
        cerberus_dual_wifi_row_t row; assert(cerberus_dual_wifi_scan_get_row(i, &row) == ESP_OK);
        assert(row.source == (i < 32 ? CERBERUS_DUAL_WIFI_SOURCE_S3 : CERBERUS_DUAL_WIFI_SOURCE_C5));
        assert(row.source_index == i % 32 && row.observation.valid == (i >= 32));
        if (i < 32) assert(row.received_ms == local_complete_at);
    }
    cerberus_dual_wifi_row_t selected, retained;
    assert(cerberus_dual_wifi_scan_get_row(4, &selected) == ESP_OK); retained = selected;
    remote_rows[7] = selected.ap; /* Fresh C5 bank has an independently obtained index. */
    assert(cerberus_dual_wifi_scan_begin_rebind(&selected) == ESP_OK); drain();
    assert(status().rebind_ready && !status().rebinding && !status().cleanup_pending);
    assert(local_starts == 1 && requested_band == CERBERUS_WIFI_SCAN_BAND_ALL); verify_retained(1, 64);
    assert(cerberus_dual_wifi_scan_get_row(4, &selected) == ESP_OK && !memcmp(&selected, &retained, sizeof(selected)));
    cerberus_c5_wifi_result_t rows[32]; cerberus_s3_wifi_observation_t observations[32];
    cerberus_wifi_scan_status_payload_t bank; cerberus_c5_wifi_relation_bank_t proof; uint8_t selected_index;
    assert(cerberus_dual_wifi_scan_copy_legacy_bank(rows, observations, 32, &bank, &proof, &selected_index) == ESP_OK);
    assert(selected_index == 7 && observations[7].valid && !memcmp(&rows[7], &selected.ap, sizeof(rows[7])));
    ++epoch; /* Copy fence must not export a replaced lease even with gen8 unchanged. */
    assert(cerberus_dual_wifi_scan_copy_legacy_bank(rows, observations, 32, &bank, &proof, &selected_index) == ESP_ERR_NOT_FOUND);
    assert(!status().rebind_ready && !bank.stored_count && !observations[7].valid);
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); cerberus_dual_wifi_scan_stop();
    assert(!dual_active() && !status().cleanup_pending); verify_retained(1, 64);
    store->status.generation = UINT32_MAX; unsigned old_starts = remote_starts;
    assert(cerberus_dual_wifi_scan_start() == ESP_ERR_INVALID_STATE && remote_starts == old_starts);

    reset(); local_start_error = ESP_ERR_INVALID_STATE;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().fallback && !status().split && requested_band == CERBERUS_WIFI_SCAN_BAND_ALL);
    verify_retained(1, 32); assert(!local_stops);

    reset(); start_reply_error = ESP_ERR_TIMEOUT;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().state == CERBERUS_DUAL_WIFI_COMPLETE && remote_starts == 1 && !remote_stops);
    assert(status().c5_error == ESP_OK); verify_retained(1, 64);
    reset(); start_reply_error = ESP_ERR_INVALID_RESPONSE;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain(); verify_retained(1, 64);
    assert(remote_starts == 1 && !remote_stops);
    reset(); start_reply_error = ESP_ERR_TIMEOUT; reject_start = true;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().state == CERBERUS_DUAL_WIFI_ERROR && !remote_stops && local_stops == 1); verify_retained(0, 0);

    reset(); assert(cerberus_dual_wifi_scan_start() == ESP_OK); until_phase(DUAL_PHASE_ROWS);
    cerberus_dual_wifi_scan_stop(); drain(); verify_retained(0, 0);
    assert(!remote_stops && !status().cleanup_pending);
    reset(); assert(cerberus_dual_wifi_scan_start() == ESP_OK); until_phase(DUAL_PHASE_WAIT);
    ++epoch; /* Foreign scan may be RUNNING; our stale Stop must never reach it. */
    cerberus_dual_wifi_scan_stop(); drain(); assert(!remote_stops && local_stops == 1);

    reset(); local_complete_at = UINT32_MAX; fail_local_cleanup = true; keep_remote_running = true;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); until_phase(DUAL_PHASE_WAIT);
    cerberus_dual_wifi_scan_stop(); for (unsigned i = 0; i < 30; ++i) tick();
    assert(status().cleanup_pending && status().s3_error == ESP_FAIL && remote_stops == 1);
    assert(cerberus_dual_wifi_scan_start() == ESP_ERR_INVALID_STATE);

    reset(); replace_local_on_row = true;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().state == CERBERUS_DUAL_WIFI_ERROR && !local_stops); verify_retained(0, 0);
    reset(); replace_bank_on_proof = true;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().state == CERBERUS_DUAL_WIFI_ERROR); verify_retained(0, 0);
    reset(); change_status_fence = true;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(status().state == CERBERUS_DUAL_WIFI_ERROR); verify_retained(0, 0);
    reset(); row_error = ESP_ERR_INVALID_RESPONSE;
    assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain(); verify_retained(0, 0);
    assert(status().state == CERBERUS_DUAL_WIFI_ERROR);

    reset(); assert(cerberus_dual_wifi_scan_start() == ESP_OK); drain();
    assert(cerberus_dual_wifi_scan_get_row(0, &selected) == ESP_OK);
    assert(cerberus_dual_wifi_scan_begin_rebind(&selected) == ESP_OK); drain();
    assert(status().error == ESP_ERR_NOT_FOUND && !status().rebind_ready && !status().rebinding); verify_retained(1, 64);
    remote_rows[2] = remote_rows[3] = selected.ap;
    assert(cerberus_dual_wifi_scan_begin_rebind(&selected) == ESP_OK); drain();
    assert(status().error == ESP_ERR_INVALID_RESPONSE && !status().rebind_ready); verify_retained(1, 64);

    puts("Dual Wi-Fi coordinator: split64, fallback, provenance, lost START recovery, owned cleanup, stale fences, retained banks and exact AP rebind passed.");
    free(store); return 0;
}
