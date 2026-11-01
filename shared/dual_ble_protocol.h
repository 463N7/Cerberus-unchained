#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Complementary receive-only observer. A bound session never authorizes an
 * active operation. Times below are C5 callback ages, not S3 clock values. */
#define CERBERUS_DUAL_BLE_LEGACY_VERSION 1U
#define CERBERUS_DUAL_BLE_VERSION 2U
#define CERBERUS_DUAL_BLE_MIN_FIRMWARE 61U
#define CERBERUS_DUAL_BLE_SNAPSHOT_MIN_FIRMWARE 62U
#define CERBERUS_DUAL_BLE_CAPACITY 32U
#define CERBERUS_DUAL_BLE_RAW_BYTES 31U
#define CERBERUS_DUAL_BLE_STALE_MS 5000U
#define CERBERUS_DUAL_BLE_ORPHAN_MS 12000U
enum { CERBERUS_DUAL_BLE_DISCOVERY, CERBERUS_DUAL_BLE_TRACK };
enum { CERBERUS_DUAL_BLE_ATTACHED = 1U, CERBERUS_DUAL_BLE_OWNED = 2U,
       CERBERUS_DUAL_BLE_EXTENDED = 4U, CERBERUS_DUAL_BLE_READY = 8U,
       CERBERUS_DUAL_BLE_SNAPSHOTS = 16U };
enum { CERBERUS_DUAL_BLE_SEEN = 1U, CERBERUS_DUAL_BLE_RSSI_VALID = 2U };
enum { CERBERUS_DUAL_BLE_SNAPSHOT_RUNNING = 1U, CERBERUS_DUAL_BLE_SNAPSHOT_COMPLETE = 2U,
       CERBERUS_DUAL_BLE_SNAPSHOT_EXTENDED = 4U };
enum { CERBERUS_DUAL_BLE_SNAPSHOT_OK, CERBERUS_DUAL_BLE_SNAPSHOT_BUSY,
       CERBERUS_DUAL_BLE_SNAPSHOT_STALE, CERBERUS_DUAL_BLE_SNAPSHOT_RANGE,
       CERBERUS_DUAL_BLE_SNAPSHOT_INVALID };
#define CERBERUS_DUAL_BLE_SNAPSHOT_CHUNK_BYTES 10U
#define CERBERUS_DUAL_BLE_SNAPSHOT_ROW_BYTES 102U
#define CERBERUS_DUAL_BLE_SNAPSHOT_CHUNKS 11U

typedef struct __attribute__((packed)) {
    uint64_t boot_id;
    uint32_t source_now_ms;
    uint32_t next_job_id; /* C5-issued, never reused during this boot. Zero: exhausted. */
    uint8_t version, capacity, raw_bytes, flags, reserved[2];
} cerberus_dual_ble_info_t;
typedef struct __attribute__((packed)) {
    uint64_t boot_id;
    uint32_t job_id;
    uint8_t address[6], address_type, mode, version, reserved;
} cerberus_dual_ble_start_t;
typedef struct __attribute__((packed)) {
    uint64_t boot_id;
    uint32_t job_id;
} cerberus_dual_ble_binding_t;
typedef struct __attribute__((packed)) {
    uint64_t epoch;
    uint32_t job_id;
    int32_t error_code;
    uint8_t state, generation, count, flags, version, reserved;
} cerberus_dual_ble_status_t;
typedef struct __attribute__((packed)) {
    cerberus_dual_ble_binding_t binding;
    uint8_t index, reserved;
} cerberus_dual_ble_row_request_t;
typedef struct __attribute__((packed)) {
    uint64_t epoch;
    uint32_t job_id, age_ms;
    uint8_t index, generation;
    int8_t latest_rssi;
    uint8_t flags;
    uint16_t reports;
} cerberus_dual_ble_row_time_t;
typedef struct __attribute__((packed)) {
    uint64_t epoch;
    uint32_t job_id, age_ms, reports;
    int8_t rssi;
    uint8_t flags;
} cerberus_dual_ble_track_t;

/* v2: a per-job strict request counter makes lost BEGIN replies idempotent.
 * C5-issued handles never repeat during a boot and map to boot/job/full epoch.
 * A successful next BEGIN replaces the sole frozen bank; reads never do. */
typedef struct __attribute__((packed)) {
    cerberus_dual_ble_binding_t binding;
    uint64_t request_id;
} cerberus_dual_ble_snapshot_begin_t;
typedef struct __attribute__((packed)) {
    cerberus_dual_ble_binding_t binding;
    uint64_t handle;
} cerberus_dual_ble_snapshot_binding_t;
typedef struct __attribute__((packed)) {
    cerberus_dual_ble_binding_t binding;
    uint64_t handle;
    uint8_t index, chunk;
} cerberus_dual_ble_snapshot_read_t;
typedef struct __attribute__((packed)) {
    uint64_t boot_id, handle;
    int32_t error_code;
    uint8_t count, flags;
} cerberus_dual_ble_snapshot_status_t;
typedef struct __attribute__((packed)) {
    uint64_t handle;
    uint8_t index, chunk, byte_count, error;
    uint8_t data[CERBERUS_DUAL_BLE_SNAPSHOT_CHUNK_BYTES];
} cerberus_dual_ble_snapshot_chunk_t;
/* Peer name/UUID/manufacturer are accumulated peer hints. Raw/RSSI/time are
 * one latest original ADV observation; these are not synthetic packet fields.
 * age_ms is frozen at the first BEGIN. The S3 must keep that request's clock
 * anchor across retries, rather than treating an idempotent reply as newer. */
typedef struct __attribute__((packed)) {
    uint8_t address[6], address_type;
    int8_t latest_rssi;
    uint8_t name_length, uuid_length, name[32], uuid[16];
    uint16_t manufacturer_id;
    uint8_t peer_flags, raw[31], raw_length, event_type, raw_flags;
    uint16_t reports;
    uint32_t age_ms;
    uint8_t timing_flags;
} cerberus_dual_ble_snapshot_row_t;

_Static_assert(sizeof(cerberus_dual_ble_info_t) == 22U, "dual BLE info frame");
_Static_assert(sizeof(cerberus_dual_ble_start_t) == 22U, "dual BLE start frame");
_Static_assert(sizeof(cerberus_dual_ble_status_t) == 22U, "dual BLE status frame");
_Static_assert(sizeof(cerberus_dual_ble_row_time_t) == 22U, "dual BLE row frame");
_Static_assert(sizeof(cerberus_dual_ble_track_t) == 22U, "dual BLE track frame");
_Static_assert(sizeof(cerberus_dual_ble_snapshot_begin_t) == 20U, "snapshot begin frame");
_Static_assert(sizeof(cerberus_dual_ble_snapshot_binding_t) == 20U, "snapshot binding frame");
_Static_assert(sizeof(cerberus_dual_ble_snapshot_read_t) == 22U, "snapshot read frame");
_Static_assert(sizeof(cerberus_dual_ble_snapshot_status_t) == 22U, "snapshot status frame");
_Static_assert(sizeof(cerberus_dual_ble_snapshot_chunk_t) == 22U, "snapshot chunk frame");
_Static_assert(sizeof(cerberus_dual_ble_snapshot_row_t) == 102U, "snapshot row layout");

static inline bool cerberus_dual_ble_start_valid(const cerberus_dual_ble_start_t *r)
{
    if (!r || !r->boot_id || !r->job_id ||
        (r->version != CERBERUS_DUAL_BLE_VERSION && r->version != CERBERUS_DUAL_BLE_LEGACY_VERSION) ||
        r->reserved || r->mode > CERBERUS_DUAL_BLE_TRACK || r->address_type > 3U) return false;
    if (r->mode == CERBERUS_DUAL_BLE_DISCOVERY) {
        if (r->address_type) return false;
        for (size_t i = 0; i < 6U; ++i) if (r->address[i]) return false;
    }
    return true;
}

static inline uint32_t cerberus_dual_ble_age_upper(uint32_t age_at_reply,
    uint64_t request_started_ms, uint64_t now_ms)
{
    if (now_ms < request_started_ms || now_ms - request_started_ms > UINT32_MAX ||
        now_ms - request_started_ms > UINT32_MAX - age_at_reply) return UINT32_MAX;
    return age_at_reply + (uint32_t)(now_ms - request_started_ms);
}
