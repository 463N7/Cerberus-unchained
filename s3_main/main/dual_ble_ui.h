/* Included by ui.c after its draw helpers. This companion view deliberately
 * does not insert C5 packets into S3's evidence, archive or action namespace. */
#pragma once
#include "dual_ble_scan.h"
#include "esp_heap_caps.h"

enum { DUAL_BLE_UI_NONE, DUAL_BLE_UI_REDRAW, DUAL_BLE_UI_BACK_S3,
    DUAL_BLE_UI_TOGGLE, DUAL_BLE_UI_FOX };
static cerberus_dual_ble_scan_t *dual_ble_ui_store;
static cerberus_dual_ble_row_t dual_ble_ui_painted[4], dual_ble_ui_selected;
static uint8_t dual_ble_ui_painted_count, dual_ble_ui_offset;
static uint8_t dual_ble_ui_target[6], dual_ble_ui_type;
static int dual_ble_ui_mode = -1;
static bool dual_ble_ui_attempted, dual_ble_ui_detail;
static esp_err_t dual_ble_ui_error;

static uint64_t dual_ble_ui_clock(void *context)
{ (void)context; return (uint64_t)(esp_timer_get_time() / 1000LL); }

static esp_err_t dual_ble_ui_exchange(void *context, uint8_t command,
    const void *request, size_t request_size, void *response, size_t response_size)
{
    (void)context;
    return c5_spi_link_dual_ble_exchange(command, request, request_size, response, response_size);
}

static void dual_ble_ui_retry(void)
{
    dual_ble_ui_attempted = false; dual_ble_ui_error = ESP_OK;
    if (dual_ble_ui_store) cerberus_dual_ble_stop(dual_ble_ui_store);
}

/* Desired mode comes only from ordinary BLE discovery or exact-address Fox
 * Hunt. Mixed Recon/Wardriving pass -1 and retain their existing radio roles. */
static void dual_ble_ui_sync(int mode, const uint8_t address[6], uint8_t type, uint64_t now)
{
    if (mode < -1 || mode > CERBERUS_DUAL_BLE_TRACK ||
        (mode == CERBERUS_DUAL_BLE_TRACK && (!address || type > 3U))) mode = -1;
    bool changed = mode != dual_ble_ui_mode || (mode == CERBERUS_DUAL_BLE_TRACK &&
        (type != dual_ble_ui_type || memcmp(address, dual_ble_ui_target, 6U)));
    if (changed) {
        dual_ble_ui_mode = mode; dual_ble_ui_attempted = false; dual_ble_ui_error = ESP_OK;
        if (mode == CERBERUS_DUAL_BLE_TRACK) {
            memcpy(dual_ble_ui_target, address, 6U); dual_ble_ui_type = type;
        }
        if (dual_ble_ui_store) cerberus_dual_ble_stop(dual_ble_ui_store);
    }
    if (mode >= 0 && !dual_ble_ui_store && !dual_ble_ui_attempted) {
        dual_ble_ui_store = heap_caps_calloc(1U, sizeof(*dual_ble_ui_store), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!dual_ble_ui_store) { dual_ble_ui_error = ESP_ERR_NO_MEM; dual_ble_ui_attempted = true; }
        else {
            const cerberus_dual_ble_ops_t ops = {NULL, dual_ble_ui_exchange, dual_ble_ui_clock};
            cerberus_dual_ble_init(dual_ble_ui_store, &ops);
        }
    }
    if (!dual_ble_ui_store) return;
    cerberus_dual_ble_tick(dual_ble_ui_store, now);
    if (mode >= 0 && !dual_ble_ui_attempted && cerberus_dual_ble_released(dual_ble_ui_store)) {
        dual_ble_ui_attempted = true;
        dual_ble_ui_error = cerberus_dual_ble_begin(dual_ble_ui_store, (uint8_t)mode,
            mode == CERBERUS_DUAL_BLE_TRACK ? dual_ble_ui_target : NULL,
            mode == CERBERUS_DUAL_BLE_TRACK ? dual_ble_ui_type : 0U);
    }
}

static void dual_ble_ui_get_view(cerberus_dual_ble_view_t *out)
{
    cerberus_dual_ble_view(dual_ble_ui_store, out);
    if (out && dual_ble_ui_error) out->error = dual_ble_ui_error;
}

static bool dual_ble_ui_released(void)
{ return cerberus_dual_ble_released(dual_ble_ui_store); }

static void dual_ble_ui_open(bool reset_detail)
{ if (reset_detail) dual_ble_ui_detail = false; }

static bool dual_ble_ui_selected_peer(cerberus_c5_ble_result_t *out)
{
    if (!out || !dual_ble_ui_detail) return false;
    memset(out, 0, sizeof(*out));
    memcpy(out->address, dual_ble_ui_selected.address, 6U);
    out->address_type = dual_ble_ui_selected.address_type;
    memcpy(out->name, dual_ble_ui_selected.name, sizeof(out->name));
    out->rssi = dual_ble_ui_selected.rssi;
    out->manufacturer_id = dual_ble_ui_selected.manufacturer_id;
    /* Address and display fields only: no connection or sound eligibility. */
    return true;
}

static esp_err_t dual_ble_ui_draw(bool s3_enabled)
{
    char line[64], identifier[24];
    cerberus_dual_ble_view_t view; dual_ble_ui_get_view(&view);
    ESP_RETURN_ON_ERROR(draw_small_button(8, 50, 304, 30, "BACK TO S3 RECEIVER"), TAG, "C5 BLE back");
    if (dual_ble_ui_detail) {
        const cerberus_dual_ble_row_t *row = &dual_ble_ui_selected;
        snprintf(line, sizeof(line), "C5 / %.33s", row->name[0] ? row->name : "<UNNAMED>");
        ESP_RETURN_ON_ERROR(draw_centered(96, line, UI_TEXT, 1), TAG, "C5 BLE name");
        cerberus_privacy_format_ble_identifier(row->address, privacy_addresses_masked(), identifier, sizeof(identifier));
        ESP_RETURN_ON_ERROR(draw_centered(118, identifier, UI_TEXT, 1), TAG, "C5 BLE address");
        snprintf(line, sizeof(line), "ADDRESS TYPE %u / C5 OBSERVATIONS ONLY", row->address_type);
        ESP_RETURN_ON_ERROR(draw_centered(140, line, UI_MUTED, 1), TAG, "C5 BLE type");
        uint32_t age = cerberus_dual_ble_age_upper(row->callback_age_ms, row->request_started_ms, dual_ble_ui_clock(NULL));
        if ((row->timing_flags & (CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID)) ==
            (CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID))
            snprintf(line, sizeof(line), "LAST ADV %d dBm / AGE <= %lu ms", row->rssi, (unsigned long)age);
        else snprintf(line, sizeof(line), "LAST ADV RSSI / TIME UNAVAILABLE");
        ESP_RETURN_ON_ERROR(draw_centered(162, line, UI_MUTED, 1), TAG, "C5 BLE timing");
        snprintf(line, sizeof(line), "REPORTS %u / VENDOR ID %04X", row->reports, row->manufacturer_id);
        ESP_RETURN_ON_ERROR(draw_centered(186, line, UI_TEXT, 1), TAG, "C5 BLE metadata");
        snprintf(line, sizeof(line), "ORIGINAL ADV %u B / FLAGS %02X", row->raw_length, row->raw_flags);
        ESP_RETURN_ON_ERROR(draw_centered(212, line, UI_MUTED, 1), TAG, "C5 BLE packet");
        for (unsigned part = 0; part < 4U; ++part) {
            size_t n = 0U; line[0] = '\0';
            for (unsigned i = part * 8U; i < row->raw_length && i < (part + 1U) * 8U; ++i)
                n += (size_t)snprintf(line + n, sizeof(line) - n, "%02X ", row->raw[i]);
            ESP_RETURN_ON_ERROR(draw_centered((uint16_t)(234U + part * 16U), line, UI_TEXT, 1), TAG, "C5 BLE bytes");
        }
        ESP_RETURN_ON_ERROR(draw_small_button(8, 316, 148, 38, "BACK TO C5 LIST"), TAG, "C5 BLE list back");
        ESP_RETURN_ON_ERROR(draw_small_button(164, 316, 148, 38, "FOX THIS ADDRESS"), TAG, "C5 BLE fox");
    } else {
        snprintf(line, sizeof(line), "C5 PASSIVE RECEIVER / %u STORED", view.count);
        ESP_RETURN_ON_ERROR(draw_centered(90, line, UI_TEXT, 1), TAG, "C5 BLE source");
        if (view.error) snprintf(line, sizeof(line), "C5 ERROR %ld / S3 VIEW AVAILABLE", (long)view.error);
        else snprintf(line, sizeof(line), "%s / %lu SWEEPS%s", view.stopping ? "STOPPING" :
            view.reading ? "READING" : view.enabled ? "SCANNING" : "STOPPED",
            (unsigned long)view.sweeps, view.extended ? " / EXT ENABLED" : "");
        ESP_RETURN_ON_ERROR(draw_centered(110, line, view.error ? UI_BAD : UI_MUTED, 1), TAG, "C5 BLE status");
        if (dual_ble_ui_offset >= view.count) dual_ble_ui_offset = 0U;
        dual_ble_ui_painted_count = 0U;
        for (uint8_t i = 0U; i < 4U; ++i) {
            cerberus_dual_ble_row_t *row = &dual_ble_ui_painted[i];
            if (!cerberus_dual_ble_row(dual_ble_ui_store, dual_ble_ui_offset + i, row)) break;
            uint16_t y = (uint16_t)(132U + i * 46U);
            ESP_RETURN_ON_ERROR(cerberus_display_fill_rect(8, y, 304, 42, UI_PANEL), TAG, "C5 BLE row");
            snprintf(line, sizeof(line), "%.30s", row->name[0] ? row->name : "<UNNAMED>");
            ESP_RETURN_ON_ERROR(cerberus_display_draw_text(14, y + 5U, line, UI_TEXT, UI_PANEL, 1), TAG, "C5 BLE row name");
            cerberus_privacy_format_ble_identifier(row->address, privacy_addresses_masked(), identifier, sizeof(identifier));
            if (row->timing_flags & CERBERUS_DUAL_BLE_RSSI_VALID)
                snprintf(line, sizeof(line), "%s T%u / C5 %d dBm", identifier, row->address_type, row->rssi);
            else snprintf(line, sizeof(line), "%s T%u / C5 RSSI N/A", identifier, row->address_type);
            ESP_RETURN_ON_ERROR(cerberus_display_draw_text(14, y + 23U, line, UI_MUTED, UI_PANEL, 1), TAG, "C5 BLE row address");
            ++dual_ble_ui_painted_count;
        }
        if (!view.count) ESP_RETURN_ON_ERROR(draw_centered(208, "WAITING FOR A COMPLETED C5 SWEEP", UI_MUTED, 1), TAG, "C5 BLE empty");
        ESP_RETURN_ON_ERROR(draw_small_button(8, 326, 148, 32, "PREVIOUS"), TAG, "C5 BLE previous");
        ESP_RETURN_ON_ERROR(draw_small_button(164, 326, 148, 32, "NEXT"), TAG, "C5 BLE next");
    }
    return draw_button(20, 370, 280, 40, s3_enabled ? "STOP BOTH RECEIVERS" : "START BOTH RECEIVERS");
}

static int dual_ble_ui_tap(uint16_t x, uint16_t y)
{
    if (x >= 8U && x < 312U && y >= 50U && y < 80U) return DUAL_BLE_UI_BACK_S3;
    if (x >= 20U && x < 300U && y >= 370U && y < 410U) return DUAL_BLE_UI_TOGGLE;
    if (dual_ble_ui_detail) {
        if (y >= 316U && y < 354U) {
            if (x >= 8U && x < 156U) { dual_ble_ui_detail = false; return DUAL_BLE_UI_REDRAW; }
            if (x >= 164U && x < 312U) return DUAL_BLE_UI_FOX;
        }
    } else {
        if (x >= 8U && x < 312U && y >= 132U && y < 316U) {
            uint8_t row = (uint8_t)((y - 132U) / 46U);
            if (row < dual_ble_ui_painted_count && (y - 132U) % 46U < 42U) {
                dual_ble_ui_selected = dual_ble_ui_painted[row]; dual_ble_ui_detail = true;
                return DUAL_BLE_UI_REDRAW;
            }
        }
        if (y >= 326U && y < 358U) {
            if (x >= 8U && x < 156U && dual_ble_ui_offset >= 4U) dual_ble_ui_offset -= 4U;
            else if (x >= 164U && x < 312U && dual_ble_ui_store && dual_ble_ui_offset + 4U < dual_ble_ui_store->count)
                dual_ble_ui_offset += 4U;
            else return DUAL_BLE_UI_NONE;
            return DUAL_BLE_UI_REDRAW;
        }
    }
    return DUAL_BLE_UI_NONE;
}

static esp_err_t dual_ble_ui_draw_fox(uint16_t y, uint64_t now)
{
    char line[64]; cerberus_dual_ble_track_view_t track;
    cerberus_dual_ble_view_t view; dual_ble_ui_get_view(&view);
    memset(&track, 0, sizeof(track));
    bool current = dual_ble_ui_mode == CERBERUS_DUAL_BLE_TRACK && dual_ble_ui_store &&
        dual_ble_ui_store->start.mode == CERBERUS_DUAL_BLE_TRACK &&
        dual_ble_ui_store->start.address_type == dual_ble_ui_type &&
        !memcmp(dual_ble_ui_store->start.address, dual_ble_ui_target, 6U);
    if (current) cerberus_dual_ble_track_view(dual_ble_ui_store, now, &track);
    if (view.error) snprintf(line, sizeof(line), "C5 TRACK ERROR %ld", (long)view.error);
    else if (track.seen) snprintf(line, sizeof(line), "C5 %d dBm / AGE <= %lu ms%s", track.rssi,
        (unsigned long)track.age_upper_ms,
        track.fresh ? "" : " / STALE");
    else snprintf(line, sizeof(line), "C5 %s / EXACT ADDRESS + TYPE", view.stopping ? "STOPPING" :
        current ? "SEARCHING" : "WAITING");
    return draw_centered(y, line, view.error ? UI_BAD : track.fresh ? UI_GOOD : UI_MUTED, 1);
}
