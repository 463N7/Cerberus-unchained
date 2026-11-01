#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Portable, bounded, passive BLE evidence. No radio, filesystem, allocation,
 * ESP-IDF types, native struct serialization, or active-target eligibility.
 * This is a saved snapshot, not a callback stream or a live scanner bank. */
enum {
    CERBERUS_BLE_ARCHIVE_LEGACY_VERSION = 1,
    CERBERUS_BLE_ARCHIVE_VERSION = 2,
    CERBERUS_BLE_ARCHIVE_MAX_ROWS_PER_RECEIVER = 32,
    CERBERUS_BLE_ARCHIVE_MAX_ROWS = 64,
    CERBERUS_BLE_ARCHIVE_NAME_MAX = 32,
    CERBERUS_BLE_ARCHIVE_UUID_MAX = 16,
    CERBERUS_BLE_ARCHIVE_ADV_MAX = 31,
    CERBERUS_BLE_ARCHIVE_HEADER_SIZE = 32,
    CERBERUS_BLE_ARCHIVE_LEGACY_ROW_SIZE = 128,
    CERBERUS_BLE_ARCHIVE_LEGACY_MAX_SIZE = 4128,
    CERBERUS_BLE_ARCHIVE_ROW_SIZE = 144,
    CERBERUS_BLE_ARCHIVE_MAX_SIZE = 9248,
    CERBERUS_BLE_ARCHIVE_RSSI_UNAVAILABLE = 127,
};
enum {
    CERBERUS_BLE_ARCHIVE_RECEIVER_LEGACY = 0,
    CERBERUS_BLE_ARCHIVE_RECEIVER_S3 = 1,
    CERBERUS_BLE_ARCHIVE_RECEIVER_C5 = 2,
};
enum {
    CERBERUS_BLE_ARCHIVE_RSSI_SWEEP_PEAK = 0,
    CERBERUS_BLE_ARCHIVE_RSSI_LATEST_ADV = 1,
};

typedef struct {
    /* NimBLE/native byte order, least significant address octet first. The
     * address type is part of identity: 0 public, 1 random, 2/3 identity types. */
    uint8_t address[6], address_type;
    /* A bounded received peer C-string projection, not the complete on-air
     * name: prior peer conversion can lose bytes after an embedded NUL.
     * Non-ASCII/control bytes remain intact here; rendering must sanitize. */
    char name[CERBERUS_BLE_ARCHIVE_NAME_MAX + 1];
    int8_t rssi;
    bool rssi_valid; /* Semantics are explicit in rssi_kind below. */
    uint16_t manufacturer_id;
    uint8_t peer_flags;
    uint8_t uuid_length, uuid[CERBERUS_BLE_ARCHIVE_UUID_MAX];
    /* Original retained ADV only. Malformed/truncated/unknown bytes and flag
     * bits remain evidence. No raw scan-response bytes are available here. */
    uint8_t advertisement_length, advertisement[CERBERUS_BLE_ARCHIVE_ADV_MAX];
    uint8_t advertisement_event_type, advertisement_flags;
    uint16_t reports_seen;
    uint8_t generation, index;
    uint64_t scan_epoch;
    bool scan_epoch_valid;
    /* Historical S3 uptime from the SAME boot as saved_uptime_ms. The copy
     * ceiling bounds the retained source; it is not a packet timestamp.
     * Unknown source time is explicit, never inferred from generation/index.
     * Invalid source/epoch fields must be zero; invalid RSSI must be 127. */
    uint64_t original_source_uptime_ms, copy_ceiling_ms;
    bool source_time_valid;
    /* LEGACY is v1's unspecified receiver; never infer a new S3 source label.
     * C5 carries latest original-ADV RSSI, its committed boot/epoch identity,
     * and an optional conservative age at SAVE. Its exact S3 time fields must
     * be zero/invalid. That upper bound may exceed saved S3 uptime. S3/LEGACY
     * use peak RSSI and must leave C5 bound/boot fields zero/invalid. */
    uint8_t receiver, rssi_kind;
    uint64_t age_upper_at_save_ms, receiver_boot_id;
    bool age_upper_valid;
} cerberus_ble_archive_row_t;

typedef struct {
    uint64_t saved_uptime_ms;
    uint8_t count; /* 1..64, at most32/source; duplicate receiver+typed addresses rejected. */
    cerberus_ble_archive_row_t rows[CERBERUS_BLE_ARCHIVE_MAX_ROWS];
} cerberus_ble_archive_t;

bool cerberus_ble_archive_validate(const cerberus_ble_archive_t *document);

/* Encode v2: header32 + count*row144. Decode also accepts v1 row128/count<=32
 * and maps its unspecified source to LEGACY. Little-endian integers, zero padding
 * and reserved bytes. CRC-32/ISO-HDLC covers ALL encoded bytes, treating the
 * header CRC field as zero. This detects corruption, not authenticity.
 * Encode failure leaves output bytes unchanged and sets *written=0 when given
 * unless written itself illegally overlaps the input or output buffer.
 * Decode failure leaves the entire previous document unchanged. No scratch
 * document or heap is required: a complete preflight precedes publication.
 * Input and output memory must not overlap (overlap is rejected). The caller
 * must keep input immutable throughout either operation. */
bool cerberus_ble_archive_encode(const cerberus_ble_archive_t *document,
    uint8_t *output, size_t capacity, size_t *written);
bool cerberus_ble_archive_decode(const uint8_t *input, size_t length,
    cerberus_ble_archive_t *document);

/* Historical age at SAVE only. Never compare a loaded document with this
 * boot's clock or present it as current freshness. Zero age is valid.
 * Failure leaves *age_ms unchanged. */
bool cerberus_ble_archive_age_at_save(const cerberus_ble_archive_t *document,
    size_t index, uint64_t *age_ms);
/* C5 conservative age at SAVE, never an exact packet timestamp. Exact-age
 * helper above rejects C5; this helper rejects S3/LEGACY and unknown bounds.
 * Neither helper uses the loading boot's clock. Failure leaves output intact. */
bool cerberus_ble_archive_age_upper_at_save(const cerberus_ble_archive_t *document,
    size_t index, uint64_t *age_ms);
