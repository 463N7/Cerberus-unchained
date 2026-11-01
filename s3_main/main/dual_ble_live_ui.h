/* UI-task integration. Included after the existing discovery/session owner. */
#pragma once
#include "dual_ble_diagnostics_ui.h"

static bool dual_ble_live_enabled(void)
{
    return ble_discovery_live && ble_discovery_live->session.enabled;
}

static bool dual_ble_live_discovery_released(void)
{
    return !ble_discovery_live ||
        cerberus_ble_discovery_session_released(&ble_discovery_live->session);
}

static void ble_fox_hunt_open_peer(const cerberus_c5_ble_result_t *peer, bool from_c5)
{
    /* Navigation cannot choose a second target while this page owns a sweep
     * or is draining one. Preserve the original target and stop permission. */
    if (!peer || ble_mode == UI_BLE_MODE_FOX_HUNT || ble_fox_hunt_exit_pending ||
        ble_fox_hunt_s3_attached || ble_fox_hunt_s3_started) return;
    /* A tracking target is only an immutable typed address, never permission
     * to use another receiver's raw packet for a GATT or sound operation. */
    ble_fox_hunt_target = *peer;
    ble_fox_hunt_from_c5 = from_c5;
    ble_fox_hunt_start_pending = true;
    ble_fox_hunt_start_requested_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
    ble_session_exit_page = UI_PAGE_BLE;
    ble_fox_hunt_exit_pending = false;
    ble_fox_hunt_exit_started_ms = 0U;
    ble_fox_hunt_retry_requested = false;
    ble_fox_hunt_s3_attached = ble_fox_hunt_s3_started = false;
    ble_fox_hunt_s3_start_pending = ble_fox_hunt_cleanup_timed_out = false;
    ble_fox_hunt_error = ESP_OK;
    ble_fox_hunt_barrier_error = ESP_OK;
    ble_fox_hunt_last_tick_ms = ble_fox_hunt_last_draw_ms = 0U;
    ble_discovery_leave();
    dual_ble_ui_sync(-1, NULL, 0U, (uint64_t)esp_timer_get_time() / 1000U);
    ble_mode = UI_BLE_MODE_FOX_HUNT;
}

static bool dual_ble_live_prepare_fox(uint32_t now, bool asleep)
{
    if (!ble_fox_hunt_start_pending) return true;
    /* Stop keeps ticking the shared barrier and any acquired source until
     * release; it never enters the attachment path below. */
    if (ble_fox_hunt_exit_pending) return true;
    if (!dual_ble_live_discovery_released() || !dual_ble_ui_released() ||
        cerberus_s3_ble_host_owner() != CERBERUS_S3_BLE_OWNER_NONE) {
        if ((uint32_t)(now - ble_fox_hunt_start_requested_ms) >= 12000U) {
            ble_fox_hunt_barrier_error = ESP_ERR_TIMEOUT;
            /* Preserve the pending ownership barrier; explicit Back remains
             * available and cannot start or stop a different owner's scan. */
            if (!asleep && !ble_fox_hunt_last_draw_ms) {
                ble_fox_hunt_last_draw_ms = now;
                ESP_ERROR_CHECK(draw_detail_page());
            }
        }
        return false;
    }
    if (asleep || ble_fox_hunt_barrier_error != ESP_OK) return false;
    /* The common drain is complete. A local attach failure must not suppress
     * the independent C5 TRACK request on the next global coordinator tick. */
    ble_fox_hunt_start_pending = false;
    ble_fox_hunt_attach_s3();
    if (!asleep) ESP_ERROR_CHECK(draw_detail_page());
    return !ble_fox_hunt_start_pending;
}

static void dual_ble_live_tap(uint16_t x, uint16_t y)
{
    int action = dual_ble_ui_tap(x, y);
    if (action == DUAL_BLE_UI_BACK_S3) {
        ble_mode = UI_BLE_MODE_LIST;
    } else if (action == DUAL_BLE_UI_TOGGLE) {
        if (!ble_discovery_live && !ble_discovery_acquire()) return;
        if (dual_ble_live_enabled()) ble_discovery_leave();
        else {
            (void)cerberus_ble_discovery_session_start(&ble_discovery_live->session);
            dual_ble_ui_retry();
        }
    } else if (action == DUAL_BLE_UI_FOX) {
        cerberus_c5_ble_result_t peer;
        if (dual_ble_ui_selected_peer(&peer)) ble_fox_hunt_open_peer(&peer, true);
    } else if (action == DUAL_BLE_UI_NONE) return;
    ESP_ERROR_CHECK(draw_detail_page());
}

static void dual_ble_live_tick(uint64_t now, bool asleep)
{
    int mode = -1;
    bool ordinary = !ui_capability_blocked && !ble_attack_selecting &&
        current_page == UI_PAGE_BLE &&
        (ble_mode == UI_BLE_MODE_LIST || ble_mode == UI_BLE_MODE_C5);
    /* Old C5/S3 session owners must release before a companion is armed. */
    bool ready = live_session.active == SENTINEL_SCAN_NONE &&
        live_session.pending.kind == SENTINEL_SCAN_COMMAND_NONE &&
        live_ble_session.active == SENTINEL_SCAN_NONE &&
        live_ble_session.pending.kind == SENTINEL_SCAN_COMMAND_NONE;
    cerberus_dual_wifi_status_t wifi;
    cerberus_dual_wifi_scan_get_status(&wifi);
    ready = ready && !dual_wifi_ui_busy(&wifi);
    if (!asleep && ready && ordinary && dual_ble_live_enabled())
        mode = CERBERUS_DUAL_BLE_DISCOVERY;
    else if (!asleep && ready && current_page == UI_PAGE_BLE &&
        ble_mode == UI_BLE_MODE_ANALYZER && ble_analyzer_started &&
        !ble_analyzer_start_pending && !ble_analyzer_exit_pending &&
        !ble_analyzer_retry_requested && ble_analyzer_error == ESP_OK)
        mode = CERBERUS_DUAL_BLE_ANALYZER;
    else if (!asleep && ready && current_page == UI_PAGE_BLE &&
        ble_mode == UI_BLE_MODE_FOX_HUNT && !ble_fox_hunt_start_pending &&
        !ble_fox_hunt_exit_pending)
        mode = CERBERUS_DUAL_BLE_TRACK;
    if (mode == CERBERUS_DUAL_BLE_DISCOVERY)
        dual_ble_ui_sync_family((uint8_t)ble_filter, now);
    else dual_ble_ui_sync(mode, mode == CERBERUS_DUAL_BLE_TRACK ?
        ble_fox_hunt_target.address : NULL, ble_fox_hunt_target.address_type, now);
    dual_ble_publish_diagnostics(now, asleep);
    static uint64_t last_draw;
    if (ordinary && ble_mode == UI_BLE_MODE_C5 && !asleep && now - last_draw >= 500U) {
        last_draw = now;
        ESP_ERROR_CHECK(draw_detail_page());
    }
}
