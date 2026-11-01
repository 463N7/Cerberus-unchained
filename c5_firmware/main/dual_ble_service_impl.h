/* Included once after the ordinary scanner. All state below uses its mutex. */
static uint32_t dual_now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000LL); }

static void dual_expire_locked(void)
{
    /* A disconnected host cannot retain a completed bank indefinitely. RF is
     * separately bounded by the existing eight-second scan callout. */
    if (dual_attached && !dual_radio_active && scan_status.state != CERBERUS_BLE_SCAN_RUNNING &&
        (uint32_t)(dual_now_ms() - dual_last_contact_ms) >= CERBERUS_DUAL_BLE_ORPHAN_MS) {
        dual_attached = false;
        dual_track_seen = false;
    }
}

static bool dual_matches_locked(const cerberus_dual_ble_binding_t *b)
{
    return b->boot_id == dual_boot_id && dual_attached && b->job_id == dual_job_id;
}

static void dual_status_locked(uint32_t requested, int32_t error, cerberus_dual_ble_status_t *out)
{
    memset(out, 0, sizeof(*out));
    out->version = CERBERUS_DUAL_BLE_VERSION;
    out->job_id = requested;
    out->state = CERBERUS_BLE_SCAN_ERROR;
    out->error_code = error;
    if (!error && dual_attached && dual_job_id == requested) {
        out->epoch = dual_epoch;
        out->generation = dual_generation;
        out->count = scan_status.stored_count;
        out->state = scan_status.state;
        out->error_code = dual_error ? dual_error : scan_status.error_code;
        out->flags = CERBERUS_DUAL_BLE_ATTACHED |
            ((scan_status.state == CERBERUS_BLE_SCAN_RUNNING || dual_radio_active) ? CERBERUS_DUAL_BLE_OWNED : 0U) |
            (MYNEWT_VAL(BLE_EXT_ADV) ? CERBERUS_DUAL_BLE_EXTENDED : 0U);
    }
}

static void dual_start(const cerberus_dual_ble_start_t *r, cerberus_dual_ble_status_t *out)
{
    if (!scanner_mutex) {
        dual_status_locked(r->job_id, ESP_ERR_INVALID_STATE, out);
        return;
    }
    xSemaphoreTake(scanner_mutex, portMAX_DELAY);
    dual_expire_locked();
    int32_t error = !cerberus_dual_ble_start_valid(r) ? ESP_ERR_INVALID_ARG :
        r->boot_id != dual_boot_id ? ESP_ERR_NOT_FOUND : 0;
    if (!error && dual_attached) {
        bool exact = r->job_id == dual_job_id && r->mode == dual_mode &&
            r->address_type == dual_address_type && !memcmp(r->address, dual_address, 6U);
        if (exact) dual_last_contact_ms = dual_now_ms();
        dual_status_locked(r->job_id, exact ? 0 : ESP_ERR_INVALID_STATE, out);
        xSemaphoreGive(scanner_mutex);
        return; /* Lost START replies can be retried without another scan. */
    }
    if (!error && (scan_status.state == CERBERUS_BLE_SCAN_RUNNING || dual_radio_active ||
        dual_epoch_counter == UINT64_MAX)) error = ESP_ERR_INVALID_STATE;
    if (!error && (dual_job_highwater == UINT32_MAX || r->job_id != dual_job_highwater + 1U))
        error = ESP_ERR_INVALID_STATE;
    if (error) {
        dual_status_locked(r->job_id, error, out);
        xSemaphoreGive(scanner_mutex);
        return;
    }
    dual_attached = true;
    dual_job_highwater = r->job_id;
    dual_job_id = r->job_id;
    dual_epoch = ++dual_epoch_counter;
    dual_mode = r->mode;
    memcpy(dual_address, r->address, 6U);
    dual_address_type = r->address_type;
    dual_track_seen = false;
    dual_track_reports = 0U;
    dual_error = 0;
    dual_last_contact_ms = dual_now_ms();
    xSemaphoreGive(scanner_mutex);
    error = start_scan(true);
    xSemaphoreTake(scanner_mutex, portMAX_DELAY);
    if (error) dual_attached = false;
    dual_status_locked(r->job_id, error, out);
    xSemaphoreGive(scanner_mutex);
}

bool cerberus_ble_dual_command(uint8_t command, const void *request,
    size_t request_size, void *response, size_t *response_size)
{
    if (!response || !response_size || (!request && request_size)) return false;
    *response_size = 0U;
    if (command == CERBERUS_SPI_COMMAND_DUAL_BLE_INFO) {
        if (request_size) return false;
        cerberus_dual_ble_info_t out = { .boot_id = dual_boot_id, .source_now_ms = dual_now_ms(),
            .version = CERBERUS_DUAL_BLE_VERSION, .capacity = CERBERUS_DUAL_BLE_CAPACITY,
            .raw_bytes = CERBERUS_DUAL_BLE_RAW_BYTES };
        if (scanner_mutex) {
            xSemaphoreTake(scanner_mutex, portMAX_DELAY);
            dual_expire_locked();
            out.next_job_id = dual_job_highwater == UINT32_MAX ? 0U : dual_job_highwater + 1U;
            out.flags = (host_synced ? CERBERUS_DUAL_BLE_READY : 0U) |
                (MYNEWT_VAL(BLE_EXT_ADV) ? CERBERUS_DUAL_BLE_EXTENDED : 0U) |
                (dual_attached ? CERBERUS_DUAL_BLE_ATTACHED : 0U);
            xSemaphoreGive(scanner_mutex);
        }
        memcpy(response, &out, sizeof(out)); *response_size = sizeof(out); return true;
    }
    if (command == CERBERUS_SPI_COMMAND_DUAL_BLE_START) {
        if (request_size != sizeof(cerberus_dual_ble_start_t)) return false;
        cerberus_dual_ble_start_t r; cerberus_dual_ble_status_t out;
        memcpy(&r, request, sizeof(r)); dual_start(&r, &out);
        memcpy(response, &out, sizeof(out)); *response_size = sizeof(out); return true;
    }
    size_t expected = command == CERBERUS_SPI_COMMAND_DUAL_BLE_ROW_TIME ?
        sizeof(cerberus_dual_ble_row_request_t) : sizeof(cerberus_dual_ble_binding_t);
    if (request_size != expected || !scanner_mutex) return false;
    cerberus_dual_ble_binding_t binding;
    memcpy(&binding, request, sizeof(binding));
    xSemaphoreTake(scanner_mutex, portMAX_DELAY);
    dual_expire_locked();
    bool valid = dual_matches_locked(&binding);
    if (valid) dual_last_contact_ms = dual_now_ms();
    if (command == CERBERUS_SPI_COMMAND_DUAL_BLE_ROW_TIME) {
        cerberus_dual_ble_row_request_t r; memcpy(&r, request, sizeof(r));
        cerberus_dual_ble_row_time_t out = {.job_id = binding.job_id, .age_ms = UINT32_MAX};
        if (valid && !r.reserved && scan_status.state == CERBERUS_BLE_SCAN_COMPLETE &&
            !dual_radio_active && r.index < scan_status.stored_count) {
            const cerberus_ble_record_t *row = &scan_results[r.index];
            out.epoch = dual_epoch; out.index = r.index; out.generation = dual_generation;
            out.latest_rssi = row->latest_rssi; out.reports = row->reports_seen;
            if (row->original_time_valid) {
                out.flags |= CERBERUS_DUAL_BLE_SEEN;
                out.age_ms = dual_now_ms() - row->original_observed_ms;
            }
            if (row->latest_rssi >= -127 && row->latest_rssi <= 20)
                out.flags |= CERBERUS_DUAL_BLE_RSSI_VALID;
        }
        xSemaphoreGive(scanner_mutex);
        memcpy(response, &out, sizeof(out)); *response_size = sizeof(out); return true;
    }
    if (command == CERBERUS_SPI_COMMAND_DUAL_BLE_TRACK_STATUS) {
        cerberus_dual_ble_track_t out = {.job_id = binding.job_id, .age_ms = UINT32_MAX};
        if (valid && dual_mode == CERBERUS_DUAL_BLE_TRACK) {
            out.epoch = dual_epoch; out.reports = dual_track_reports; out.rssi = dual_track_rssi;
            if (dual_track_seen) {
                out.flags = CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID;
                out.age_ms = dual_now_ms() - dual_track_ms;
            }
        }
        xSemaphoreGive(scanner_mutex);
        memcpy(response, &out, sizeof(out)); *response_size = sizeof(out); return true;
    }
    if (command != CERBERUS_SPI_COMMAND_DUAL_BLE_STATUS &&
        command != CERBERUS_SPI_COMMAND_DUAL_BLE_STOP && command != CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE) {
        xSemaphoreGive(scanner_mutex); return false;
    }
    bool owned = scan_status.state == CERBERUS_BLE_SCAN_RUNNING || dual_radio_active;
    if (valid && command == CERBERUS_SPI_COMMAND_DUAL_BLE_STOP && owned) {
        xSemaphoreGive(scanner_mutex);
        (void)stop_scan(true);
        xSemaphoreTake(scanner_mutex, portMAX_DELAY);
        valid = dual_matches_locked(&binding);
        owned = scan_status.state == CERBERUS_BLE_SCAN_RUNNING || dual_radio_active;
    }
    cerberus_dual_ble_status_t out;
    dual_status_locked(binding.job_id, valid ? 0 : ESP_ERR_NOT_FOUND, &out);
    if (valid && command == CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE) {
        if (owned) out.error_code = ESP_ERR_INVALID_STATE;
        else {
            dual_attached = false; dual_track_seen = false;
            out.state = CERBERUS_BLE_SCAN_IDLE; out.flags = 0U; out.error_code = 0;
        }
    }
    xSemaphoreGive(scanner_mutex);
    memcpy(response, &out, sizeof(out)); *response_size = sizeof(out); return true;
}
