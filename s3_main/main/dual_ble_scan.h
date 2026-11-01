#pragma once
#include "cerberus_protocol.h"
#include "dual_ble_protocol.h"
#include "esp_err.h"

/* UI-owned optional PSRAM store. No allocation/task/RF occurs in getters.
 * C5 observations remain separate from S3 raw-evidence/action namespaces. */
typedef struct {
    uint8_t address[6], address_type;
    int8_t rssi;
    char name[CERBERUS_BLE_NAME_MAX_LENGTH + 1U];
    uint8_t uuid[CERBERUS_BLE_UUID_MAX_LENGTH], uuid_length;
    uint8_t raw[CERBERUS_DUAL_BLE_RAW_BYTES], raw_length, raw_flags, event_type;
    uint16_t manufacturer_id, reports;
    uint8_t flags, timing_flags;
    uint32_t callback_age_ms;
    uint64_t request_started_ms;
} cerberus_dual_ble_row_t;
typedef struct {
    void *context;
    esp_err_t (*exchange)(void *, uint8_t, const void *, size_t, void *, size_t);
    uint64_t (*now_ms)(void *);
} cerberus_dual_ble_ops_t;
typedef struct {
    cerberus_dual_ble_ops_t ops;
    cerberus_dual_ble_row_t rows[CERBERUS_DUAL_BLE_CAPACITY];
    cerberus_dual_ble_row_t staging[CERBERUS_DUAL_BLE_CAPACITY];
    cerberus_dual_ble_start_t start;
    cerberus_dual_ble_status_t remote;
    cerberus_dual_ble_track_t track;
    cerberus_ble_scan_result_payload_t metadata;
    uint64_t published_epoch, track_request_ms, next_poll_ms, last_tick_ms;
    uint32_t job_counter, sweeps;
    esp_err_t error;
    uint8_t phase, count, read_index, copied, retries;
    bool enabled, extended, has_track, stop_requested;
} cerberus_dual_ble_scan_t;
typedef struct {
    bool enabled, attached, receiving, reading, stopping, extended;
    uint8_t count;
    uint32_t sweeps;
    esp_err_t error;
} cerberus_dual_ble_view_t;
typedef struct {
    bool seen, fresh;
    int8_t rssi;
    uint32_t age_upper_ms, reports;
} cerberus_dual_ble_track_view_t;

void cerberus_dual_ble_init(cerberus_dual_ble_scan_t *, const cerberus_dual_ble_ops_t *);
esp_err_t cerberus_dual_ble_begin(cerberus_dual_ble_scan_t *, uint8_t mode,
    const uint8_t address[6], uint8_t address_type);
void cerberus_dual_ble_stop(cerberus_dual_ble_scan_t *);
/* At most four single-frame operations; stop starting more at an eight-ms
 * local budget. One in-flight SDK transaction can exceed that budget. */
void cerberus_dual_ble_tick(cerberus_dual_ble_scan_t *, uint64_t now_ms);
void cerberus_dual_ble_view(const cerberus_dual_ble_scan_t *, cerberus_dual_ble_view_t *);
bool cerberus_dual_ble_row(const cerberus_dual_ble_scan_t *, uint8_t index,
    cerberus_dual_ble_row_t *out);
void cerberus_dual_ble_track_view(const cerberus_dual_ble_scan_t *, uint64_t now_ms,
    cerberus_dual_ble_track_view_t *out);
bool cerberus_dual_ble_released(const cerberus_dual_ble_scan_t *);
