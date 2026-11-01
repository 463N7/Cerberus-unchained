#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Complementary receive-only observer. A bound session never authorizes an
 * active operation. Times below are C5 callback ages, not S3 clock values. */
#define CERBERUS_DUAL_BLE_VERSION 1U
#define CERBERUS_DUAL_BLE_MIN_FIRMWARE 61U
#define CERBERUS_DUAL_BLE_CAPACITY 32U
#define CERBERUS_DUAL_BLE_RAW_BYTES 31U
#define CERBERUS_DUAL_BLE_STALE_MS 5000U
#define CERBERUS_DUAL_BLE_ORPHAN_MS 12000U
enum { CERBERUS_DUAL_BLE_DISCOVERY, CERBERUS_DUAL_BLE_TRACK };
enum { CERBERUS_DUAL_BLE_ATTACHED = 1U, CERBERUS_DUAL_BLE_OWNED = 2U,
       CERBERUS_DUAL_BLE_EXTENDED = 4U, CERBERUS_DUAL_BLE_READY = 8U };
enum { CERBERUS_DUAL_BLE_SEEN = 1U, CERBERUS_DUAL_BLE_RSSI_VALID = 2U };

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

_Static_assert(sizeof(cerberus_dual_ble_info_t) == 22U, "dual BLE info frame");
_Static_assert(sizeof(cerberus_dual_ble_start_t) == 22U, "dual BLE start frame");
_Static_assert(sizeof(cerberus_dual_ble_status_t) == 22U, "dual BLE status frame");
_Static_assert(sizeof(cerberus_dual_ble_row_time_t) == 22U, "dual BLE row frame");
_Static_assert(sizeof(cerberus_dual_ble_track_t) == 22U, "dual BLE track frame");

static inline bool cerberus_dual_ble_start_valid(const cerberus_dual_ble_start_t *r)
{
    if (!r || !r->boot_id || !r->job_id || r->version != CERBERUS_DUAL_BLE_VERSION ||
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
