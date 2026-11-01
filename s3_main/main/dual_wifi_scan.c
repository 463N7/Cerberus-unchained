#include "dual_wifi_scan.h"
#include "s3_wifi_scanner.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>

#define DUAL_WIFI_DEADLINE_MS 20000U
#define DUAL_WIFI_POLL_MS 100U
enum { DUAL_PHASE_IDLE, DUAL_PHASE_LOCAL, DUAL_PHASE_START, DUAL_PHASE_RESOLVE_START, DUAL_PHASE_WAIT,
    DUAL_PHASE_PROOF, DUAL_PHASE_ROWS, DUAL_PHASE_STATUS, DUAL_PHASE_FINAL_PROOF,
    DUAL_PHASE_FINAL_OWNER, DUAL_PHASE_LOCAL_WAIT, DUAL_PHASE_CLEANUP };

typedef struct {
    cerberus_dual_wifi_row_t retained[CERBERUS_DUAL_WIFI_CAPACITY];
    cerberus_dual_wifi_row_t pending[CERBERUS_DUAL_WIFI_CAPACITY];
    cerberus_dual_wifi_row_t target;
    cerberus_dual_wifi_status_t status;
    cerberus_wifi_scan_status_payload_t c5_status;
    cerberus_c5_wifi_relation_bank_t proof;
    cerberus_c5_wifi_relation_reader_t proof_reader;
    cerberus_c5_wifi_scan_reader_t row_reader;
    uint64_t c5_session;
    uint32_t started_ms, deadline_ms, poll_ms, s3_job;
    uint8_t phase, band, c5_index, matched_index;
    bool s3_started, s3_done, s3_owned, c5_possible, c5_stop_next, stopping;
} dual_wifi_store_t;
static dual_wifi_store_t *store;

static uint32_t dual_now(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static bool dual_active(void) { return store && store->phase != DUAL_PHASE_IDLE; }
static void dual_cleanup(esp_err_t error)
{
    if (error != ESP_OK && store->status.error == ESP_OK) store->status.error = error;
    store->status.rebind_ready = false;
    c5_spi_link_wifi_scan_result_cancel(&store->row_reader);
    c5_spi_link_wifi_relation_cancel(&store->proof_reader);
    store->phase = DUAL_PHASE_CLEANUP;
    store->status.cleanup_pending = store->s3_owned || store->c5_possible;
    store->status.state = error == ESP_OK ? CERBERUS_DUAL_WIFI_STOPPING : CERBERUS_DUAL_WIFI_ERROR;
    store->poll_ms = dual_now() - DUAL_WIFI_POLL_MS;
    if (!store->status.cleanup_pending) {
        store->status.elapsed_ms = dual_now() - store->started_ms;
        store->status.state = store->status.error == ESP_OK ? CERBERUS_DUAL_WIFI_IDLE : CERBERUS_DUAL_WIFI_ERROR;
        store->status.rebinding = false;
        store->phase = DUAL_PHASE_IDLE;
    }
}

static esp_err_t dual_begin(bool rebinding)
{
    if (dual_active()) return ESP_ERR_INVALID_STATE;
    if (!store) {
        store = heap_caps_calloc(1U, sizeof(*store), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!store) return ESP_ERR_NO_MEM;
    }
    store->status.rebind_ready = false;
    if (!rebinding && store->status.generation == UINT32_MAX) return ESP_ERR_INVALID_STATE;
    cerberus_wifi_scan_band_response_t info;
    esp_err_t error = c5_spi_link_wifi_scan_band_info(&info);
    if (error != ESP_OK || info.session_id == UINT64_MAX) return error != ESP_OK ? error : ESP_ERR_INVALID_STATE;
    uint8_t count = store->status.count;
    uint32_t generation = store->status.generation, completed_ms = store->status.completed_ms;
    memset(&store->status, 0, sizeof(store->status));
    store->status.count = count; store->status.generation = generation; store->status.completed_ms = completed_ms;
    store->status.state = CERBERUS_DUAL_WIFI_RUNNING;
    store->status.rebinding = rebinding;
    memset(store->pending, 0, sizeof(store->pending));
    memset(&store->c5_status, 0, sizeof(store->c5_status));
    memset(&store->proof, 0, sizeof(store->proof));
    memset(&store->proof_reader, 0, sizeof(store->proof_reader));
    memset(&store->row_reader, 0, sizeof(store->row_reader));
    store->started_ms = dual_now(); store->deadline_ms = store->started_ms + DUAL_WIFI_DEADLINE_MS;
    store->poll_ms = store->started_ms - DUAL_WIFI_POLL_MS;
    store->c5_session = info.session_id + 1U;
    store->band = CERBERUS_WIFI_SCAN_BAND_ALL;
    store->s3_job = 0U; store->c5_index = 0U; store->matched_index = 0U;
    store->s3_started = store->s3_owned = store->c5_possible = store->c5_stop_next = store->stopping = false;
    store->s3_done = rebinding;
    store->phase = DUAL_PHASE_LOCAL;
    return ESP_OK;
}

esp_err_t cerberus_dual_wifi_scan_start(void) { return dual_begin(false); }

void cerberus_dual_wifi_scan_get_status(cerberus_dual_wifi_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!store) return;
    *out = store->status;
    if (dual_active()) out->elapsed_ms = dual_now() - store->started_ms;
}

esp_err_t cerberus_dual_wifi_scan_get_row(uint8_t index, cerberus_dual_wifi_row_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    if (!store || index >= store->status.count) return ESP_ERR_NOT_FOUND;
    *out = store->retained[index];
    return ESP_OK;
}

void cerberus_dual_wifi_scan_stop(void)
{
    if (!dual_active()) return;
    store->stopping = true;
    dual_cleanup(ESP_OK);
}

esp_err_t cerberus_dual_wifi_scan_begin_rebind(const cerberus_dual_wifi_row_t *target)
{
    if (!target || !target->ap.channel || target->ap.security > CERBERUS_WIFI_SECURITY_OTHER ||
        (target->source != CERBERUS_DUAL_WIFI_SOURCE_S3 && target->source != CERBERUS_DUAL_WIFI_SOURCE_C5) ||
        !memchr(target->ap.ssid, 0, sizeof(target->ap.ssid))) return ESP_ERR_INVALID_ARG;
    cerberus_dual_wifi_row_t copy = *target;
    esp_err_t error = dual_begin(true);
    if (error == ESP_OK) store->target = copy;
    return error;
}

static bool dual_same_ap(const cerberus_c5_wifi_result_t *a, const cerberus_c5_wifi_result_t *b)
{
    return a->channel == b->channel && a->security == b->security &&
        !memcmp(a->bssid, b->bssid, 6U) && !strcmp(a->ssid, b->ssid);
}

static bool dual_same_status(const cerberus_wifi_scan_status_payload_t *a,
    const cerberus_wifi_scan_status_payload_t *b)
{
    return a->state == CERBERUS_WIFI_SCAN_COMPLETE && b->state == a->state &&
        a->generation == b->generation && a->stored_count == b->stored_count &&
        a->total_count == b->total_count && a->hidden_count == b->hidden_count &&
        a->error_code == ESP_OK && b->error_code == ESP_OK;
}

static esp_err_t dual_collect_local(void)
{
    if (!store->s3_started || store->s3_done) return ESP_OK;
    cerberus_s3_wifi_scan_status_t current;
    esp_err_t error = cerberus_s3_wifi_scanner_get_status(&current);
    if (error != ESP_OK) return error;
    if (current.job_id != store->s3_job) {
        store->s3_owned = false; /* A foreign job must never receive our Stop. */
        return ESP_ERR_INVALID_STATE;
    }
    store->s3_owned = current.radio_owned;
    if (current.cleanup_error != ESP_OK) return current.cleanup_error;
    if (current.state == CERBERUS_S3_WIFI_SCAN_ERROR || current.state == CERBERUS_S3_WIFI_SCAN_CANCELLED)
        return current.error_code != ESP_OK ? current.error_code : ESP_ERR_INVALID_STATE;
    if (current.state != CERBERUS_S3_WIFI_SCAN_COMPLETE || current.radio_owned) return ESP_OK;
    if (current.count > CERBERUS_S3_WIFI_SCAN_CAPACITY || current.generation != store->s3_job)
        return ESP_ERR_INVALID_RESPONSE;
    for (uint8_t i = 0; i < current.count; ++i) {
        cerberus_s3_wifi_scan_row_t local;
        error = cerberus_s3_wifi_scanner_get_result(current.generation, i, &local);
        if (error != ESP_OK) return error;
        if (!local.channel || local.channel > 14U || local.ssid_length > 32U ||
            local.source != CERBERUS_S3_WIFI_SCAN_SOURCE_S3) return ESP_ERR_INVALID_RESPONSE;
        cerberus_dual_wifi_row_t *row = &store->pending[i];
        memset(row, 0, sizeof(*row));
        memcpy(row->ap.ssid, local.ssid, local.ssid_length);
        memcpy(row->ap.bssid, local.bssid, 6U);
        row->ap.channel = local.channel; row->ap.rssi = local.rssi; row->ap.security = local.security;
        row->source = CERBERUS_DUAL_WIFI_SOURCE_S3; row->source_index = i;
        row->source_generation = current.generation; row->received_ms = current.completed_ms;
        /* No C5 source-time or beacon-presence claim is fabricated here. */
    }
    cerberus_s3_wifi_scan_status_t after;
    error = cerberus_s3_wifi_scanner_get_status(&after);
    if (error != ESP_OK || after.job_id != current.job_id || after.generation != current.generation ||
        after.state != current.state || after.count != current.count || after.radio_owned ||
        after.completed_ms != current.completed_ms) return ESP_ERR_INVALID_STATE;
    store->status.s3_count = current.count; store->status.s3_total = current.total_count;
    store->s3_done = true;
    return ESP_OK;
}

static void dual_finish(void)
{
    store->status.elapsed_ms = dual_now() - store->started_ms;
    store->status.cleanup_pending = false;
    if (store->status.rebinding) {
        unsigned matches = 0;
        for (uint8_t i = 0; i < store->c5_status.stored_count; ++i) {
            if (dual_same_ap(&store->target.ap, &store->pending[32U + i].ap)) {
                store->matched_index = i; ++matches;
            }
        }
        if (matches != 1U) {
            store->status.error = matches ? ESP_ERR_INVALID_RESPONSE : ESP_ERR_NOT_FOUND;
            store->status.state = CERBERUS_DUAL_WIFI_ERROR;
        } else {
            store->status.rebind_ready = true;
            store->status.state = CERBERUS_DUAL_WIFI_COMPLETE;
        }
    } else {
        uint8_t count = 0;
        for (uint8_t i = 0; i < store->status.s3_count; ++i) store->retained[count++] = store->pending[i];
        for (uint8_t i = 0; i < store->status.c5_count; ++i) store->retained[count++] = store->pending[32U + i];
        memset(store->retained + count, 0, (CERBERUS_DUAL_WIFI_CAPACITY - count) * sizeof(store->retained[0]));
        store->status.count = count; ++store->status.generation;
        store->status.completed_ms = dual_now();
        store->status.state = CERBERUS_DUAL_WIFI_COMPLETE;
    }
    store->status.rebinding = false;
    store->phase = DUAL_PHASE_IDLE;
}

static void dual_cleanup_step(void)
{
    if (store->s3_owned) {
        cerberus_s3_wifi_scan_status_t local;
        if (cerberus_s3_wifi_scanner_get_status(&local) == ESP_OK) {
            if (local.job_id != store->s3_job || !local.radio_owned) store->s3_owned = false;
            else if (local.cleanup_error != ESP_OK) store->status.s3_error = local.cleanup_error;
            else if (store->s3_job) (void)cerberus_s3_wifi_scanner_stop(store->s3_job);
        }
    }
    if (store->c5_possible && (uint32_t)(dual_now() - store->poll_ms) >= DUAL_WIFI_POLL_MS) {
        store->poll_ms = dual_now();
        cerberus_wifi_scan_status_payload_t current;
        esp_err_t error = store->c5_stop_next
            ? c5_spi_link_stop_wifi_scan_band(store->band, store->c5_session, &current)
            : c5_spi_link_wifi_scan_band_status(store->band, store->c5_session, &current);
        store->c5_stop_next = false;
        if (error == ESP_ERR_NOT_FOUND) store->c5_possible = false;
        else if (error != ESP_OK) store->status.c5_error = error;
        else if (current.state != CERBERUS_WIFI_SCAN_RUNNING) store->c5_possible = false;
        else { store->c5_stop_next = true;
            if (current.error_code != ESP_OK) store->status.c5_error = current.error_code; }
    }
    store->status.cleanup_pending = store->s3_owned || store->c5_possible;
    if (!store->status.cleanup_pending) {
        store->status.elapsed_ms = dual_now() - store->started_ms;
        store->status.state = store->status.error == ESP_OK ? CERBERUS_DUAL_WIFI_IDLE : CERBERUS_DUAL_WIFI_ERROR;
        store->status.rebinding = false;
        store->phase = DUAL_PHASE_IDLE;
    }
}

void cerberus_dual_wifi_scan_step(void)
{
    if (!dual_active()) return;
    if (store->phase == DUAL_PHASE_CLEANUP) { dual_cleanup_step(); return; }
    if ((uint32_t)(dual_now() - store->started_ms) >= DUAL_WIFI_DEADLINE_MS) {
        dual_cleanup(ESP_ERR_TIMEOUT); return;
    }
    esp_err_t error = dual_collect_local();
    if (error != ESP_OK) { store->status.s3_error = error; dual_cleanup(error); return; }
    if (store->phase == DUAL_PHASE_LOCAL) {
        if (!store->status.rebinding) {
            cerberus_s3_wifi_scan_status_t local;
            error = cerberus_s3_wifi_scanner_start(&local);
            store->s3_job = local.job_id;
            store->s3_owned = local.radio_owned;
            if (error == ESP_OK) {
                store->s3_started = true; store->status.split = true;
                store->band = CERBERUS_WIFI_SCAN_BAND_5G;
            } else {
                store->status.s3_error = error;
                if (local.radio_owned || local.cleanup_error != ESP_OK) { dual_cleanup(error); return; }
                store->status.fallback = true; store->s3_done = true;
            }
        }
        store->phase = DUAL_PHASE_START; return;
    }
    if (store->phase == DUAL_PHASE_RESOLVE_START) {
        if ((uint32_t)(dual_now() - store->poll_ms) < DUAL_WIFI_POLL_MS) return;
        store->poll_ms = dual_now();
        error = c5_spi_link_wifi_scan_band_status(store->band, store->c5_session, &store->c5_status);
        if (error == ESP_ERR_TIMEOUT || error == ESP_ERR_INVALID_RESPONSE || error == ESP_ERR_INVALID_CRC)
            return; /* No blind restart: resolve the one possibly accepted start. */
        if (error != ESP_OK) {
            if (error == ESP_ERR_NOT_FOUND) store->c5_possible = false;
            store->status.c5_error = error; dual_cleanup(error); return;
        }
        store->status.c5_error = ESP_OK;
        store->phase = DUAL_PHASE_WAIT;
        store->poll_ms = dual_now() - DUAL_WIFI_POLL_MS;
        return;
    }
    if (store->phase == DUAL_PHASE_START) {
        store->c5_possible = true; /* Lost reply may still mean accepted RF start. */
        error = c5_spi_link_start_wifi_scan_band(store->band, store->c5_session, &store->c5_status);
        if (error != ESP_OK) {
            store->status.c5_error = error;
            if (error == ESP_ERR_TIMEOUT || error == ESP_ERR_INVALID_RESPONSE || error == ESP_ERR_INVALID_CRC) {
                store->phase = DUAL_PHASE_RESOLVE_START;
                store->poll_ms = dual_now() - DUAL_WIFI_POLL_MS;
                return;
            }
            if (error == ESP_ERR_NOT_SUPPORTED || error == ESP_ERR_INVALID_ARG) store->c5_possible = false;
            dual_cleanup(error); return;
        }
        store->phase = DUAL_PHASE_WAIT;
        store->poll_ms = dual_now() - DUAL_WIFI_POLL_MS;
        return;
    }
    if (store->phase == DUAL_PHASE_WAIT) {
        if ((uint32_t)(dual_now() - store->poll_ms) < DUAL_WIFI_POLL_MS) return;
        store->poll_ms = dual_now();
        error = c5_spi_link_wifi_scan_band_status(store->band, store->c5_session, &store->c5_status);
        if (error == ESP_ERR_TIMEOUT) return;
        if (error != ESP_OK) { if (error == ESP_ERR_NOT_FOUND) store->c5_possible = false;
            store->status.c5_error = error; dual_cleanup(error); return; }
        if (store->c5_status.state == CERBERUS_WIFI_SCAN_RUNNING) return;
        store->c5_possible = false;
        if (store->c5_status.state != CERBERUS_WIFI_SCAN_COMPLETE || store->c5_status.error_code != ESP_OK) {
            error = store->c5_status.error_code != ESP_OK ? store->c5_status.error_code : ESP_ERR_INVALID_STATE;
            store->status.c5_error = error; dual_cleanup(error); return;
        }
        store->status.c5_count = store->c5_status.stored_count;
        store->status.c5_total = store->c5_status.total_count;
        store->status.state = CERBERUS_DUAL_WIFI_READING;
        error = c5_spi_link_wifi_relation_begin_check(&store->proof_reader,
            store->c5_status.generation, store->deadline_ms);
        if (error != ESP_OK) { dual_cleanup(error); return; }
        store->phase = DUAL_PHASE_PROOF; return;
    }
    if (store->phase == DUAL_PHASE_PROOF || store->phase == DUAL_PHASE_FINAL_PROOF) {
        bool complete = false;
        cerberus_c5_wifi_relation_bank_t proof;
        error = c5_spi_link_wifi_relation_step(&store->proof_reader, &proof, &complete, NULL, NULL);
        if (error != ESP_OK) { store->status.c5_error = error; dual_cleanup(error); return; }
        if (!complete) return;
        if (store->phase == DUAL_PHASE_FINAL_PROOF) { store->phase = DUAL_PHASE_FINAL_OWNER; return; }
        store->proof = proof; store->c5_index = 0U;
        if (!store->status.c5_count) { store->phase = DUAL_PHASE_STATUS; return; }
        error = c5_spi_link_wifi_scan_result_begin(&store->row_reader,
            store->c5_status.generation, 0U, store->deadline_ms);
        if (error != ESP_OK) { dual_cleanup(error); return; }
        store->phase = DUAL_PHASE_ROWS; return;
    }
    if (store->phase == DUAL_PHASE_ROWS) {
        cerberus_dual_wifi_row_t *row = &store->pending[32U + store->c5_index];
        bool complete = false;
        error = c5_spi_link_wifi_scan_result_step(&store->row_reader, &row->ap, &row->observation, &complete);
        if (error != ESP_OK) { store->status.c5_error = error; dual_cleanup(error); return; }
        if (!complete) return;
        if (store->status.split && row->ap.channel <= 14U) { dual_cleanup(ESP_ERR_INVALID_RESPONSE); return; }
        row->source = CERBERUS_DUAL_WIFI_SOURCE_C5; row->source_index = store->c5_index;
        row->source_generation = store->c5_status.generation; row->received_ms = dual_now();
        if (++store->c5_index == store->status.c5_count) store->phase = DUAL_PHASE_STATUS;
        else {
            error = c5_spi_link_wifi_scan_result_begin(&store->row_reader,
                store->c5_status.generation, store->c5_index, store->deadline_ms);
            if (error != ESP_OK) dual_cleanup(error);
        }
        return;
    }
    if (store->phase == DUAL_PHASE_STATUS) {
        cerberus_wifi_scan_status_payload_t current;
        error = c5_spi_link_get_wifi_scan_status_once(store->deadline_ms, &current);
        if (error != ESP_OK || !dual_same_status(&store->c5_status, &current)) {
            dual_cleanup(error != ESP_OK ? error : ESP_ERR_INVALID_STATE); return;
        }
        error = c5_spi_link_wifi_relation_begin_check_expected(&store->proof_reader, &store->proof, store->deadline_ms);
        if (error != ESP_OK) { dual_cleanup(error); return; }
        store->phase = DUAL_PHASE_FINAL_PROOF; return;
    }
    if (store->phase == DUAL_PHASE_FINAL_OWNER) {
        cerberus_wifi_scan_status_payload_t current;
        error = c5_spi_link_wifi_scan_band_status(store->band, store->c5_session, &current);
        if (error != ESP_OK || !dual_same_status(&store->c5_status, &current)) {
            dual_cleanup(error != ESP_OK ? error : ESP_ERR_INVALID_STATE); return;
        }
        if (!store->s3_done || store->s3_owned) store->phase = DUAL_PHASE_LOCAL_WAIT;
        else dual_finish();
        return;
    }
    if (store->phase == DUAL_PHASE_LOCAL_WAIT && store->s3_done && !store->s3_owned)
        store->phase = DUAL_PHASE_STATUS; /* Re-fence C5 after waiting on local cleanup. */
}

esp_err_t cerberus_dual_wifi_scan_copy_legacy_bank(cerberus_c5_wifi_result_t *rows,
    cerberus_s3_wifi_observation_t *observations, size_t capacity,
    cerberus_wifi_scan_status_payload_t *status, cerberus_c5_wifi_relation_bank_t *proof,
    uint8_t *selected_index)
{
    if (!rows || !observations || !status || !proof || !selected_index || capacity < CERBERUS_WIFI_SCAN_MAX_RESULTS)
        return ESP_ERR_INVALID_ARG;
    memset(rows, 0, CERBERUS_WIFI_SCAN_MAX_RESULTS * sizeof(*rows));
    memset(observations, 0, CERBERUS_WIFI_SCAN_MAX_RESULTS * sizeof(*observations));
    memset(status, 0, sizeof(*status)); memset(proof, 0, sizeof(*proof)); *selected_index = 0U;
    if (!store || dual_active() || !store->status.rebind_ready || store->status.cleanup_pending)
        return ESP_ERR_INVALID_STATE;
    cerberus_wifi_scan_status_payload_t current;
    esp_err_t error = c5_spi_link_wifi_scan_band_status(store->band, store->c5_session, &current);
    if (error != ESP_OK || !dual_same_status(&store->c5_status, &current)) {
        store->status.rebind_ready = false;
        return error != ESP_OK ? error : ESP_ERR_INVALID_STATE;
    }
    for (uint8_t i = 0; i < store->c5_status.stored_count; ++i) {
        rows[i] = store->pending[32U + i].ap; observations[i] = store->pending[32U + i].observation;
    }
    *status = store->c5_status; *proof = store->proof; *selected_index = store->matched_index;
    return ESP_OK;
}
