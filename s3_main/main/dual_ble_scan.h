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
    /* Frozen original-ADV receive evidence. Older peers provide canonical
     * UNKNOWN, not an inferred PHY/event mode or full-AD validation result. */
    cerberus_dual_ble_receive_metadata_t receive;
} cerberus_dual_ble_row_t;
/* Historical identity of the published bank, not the currently running job.
 * Freeze beside a copied row. Generation zero is valid after wrap; the full
 * boot/epoch pair carries source identity. These fields grant no live owner. */
typedef struct {
    uint64_t boot_id, epoch;
    uint8_t generation, index;
} cerberus_dual_ble_provenance_t;
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
    /* One frozen wire row only; the existing staging bank remains the sole
     * unpublished inventory. No queued snapshots or additional full bank. */
    cerberus_dual_ble_snapshot_status_t snapshot;
    cerberus_dual_ble_snapshot_inventory_t pending_inventory;
    union {
        cerberus_dual_ble_snapshot_row_t snapshot_row;
        cerberus_dual_ble_snapshot_metadata_row_t snapshot_metadata;
    };
    uint64_t snapshot_request_id, snapshot_request_started_ms, next_snapshot_ms;
    uint64_t published_epoch, published_boot_id, track_request_ms, next_poll_ms, last_tick_ms;
    uint32_t job_counter;
    /* Current public begin: completed RF sweeps, coherent published banks (empty included),
     * and latency to its first new nonempty C5 publication. Retained rows do
     * not set this metric; it is not an observation/RSSI timestamp. */
    uint32_t sweeps, snapshots, first_result_ms;
    /* Counts from the exact last published frozen bank / RF sweep. Unknown
     * is explicit; these are replacement/drop events, not unique devices. */
    uint32_t inventory_replaced, inventory_dropped;
    uint64_t discovery_started_ms;
    esp_err_t error;
    uint8_t phase, count, read_index, copied, retries, published_generation, inventory_retries;
    bool enabled, extended, has_track, stop_requested;
    bool has_first_result, live_updates, discovery_clock_valid, snapshot_anchor_valid, snapshots_capable;
    bool inventory_known, inventory_replacement_enabled, pending_inventory_known, inventory_query_enabled;
    bool metadata_read_enabled;
} cerberus_dual_ble_scan_t;
typedef struct {
    bool enabled, attached, receiving, reading, stopping, extended;
    uint8_t count;
    uint32_t sweeps;
    uint32_t snapshots, first_result_ms;
    bool has_first_result, live_updates;
    uint32_t inventory_replaced, inventory_dropped;
    bool inventory_known, inventory_replacement_enabled;
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
/* At most four transport calls; stop starting more at an eight-ms local
 * budget. Cache recovery may add one passive peer-status probe within a
 * transport call. One in-flight SDK transaction can exceed that budget. */
void cerberus_dual_ble_tick(cerberus_dual_ble_scan_t *, uint64_t now_ms);
void cerberus_dual_ble_view(const cerberus_dual_ble_scan_t *, cerberus_dual_ble_view_t *);
bool cerberus_dual_ble_row(const cerberus_dual_ble_scan_t *, uint8_t index,
    cerberus_dual_ble_row_t *out);
/* Call on the same UI owner as tick. Row and provenance are copied together
 * from the last fully published bank; neither a new START nor Stop replaces
 * them. No transport, clock or allocation. Failed copies clear both outputs. */
bool cerberus_dual_ble_copy_row(const cerberus_dual_ble_scan_t *, uint8_t index,
    cerberus_dual_ble_row_t *out, cerberus_dual_ble_provenance_t *provenance);
void cerberus_dual_ble_track_view(const cerberus_dual_ble_scan_t *, uint64_t now_ms,
    cerberus_dual_ble_track_view_t *out);
bool cerberus_dual_ble_released(const cerberus_dual_ble_scan_t *);
