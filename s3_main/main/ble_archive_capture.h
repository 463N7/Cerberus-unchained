#pragma once

#include "ble_archive.h"
#include "c5_spi_link.h"
#include "ble_rssi.h"
#include <string.h>

/* Copy an already frozen observation. This helper never queries the scanner,
 * repairs identities, refreshes source time or imports an active target. */
static inline bool cerberus_ble_archive_capture_row(cerberus_ble_archive_row_t *out,
    const cerberus_c5_ble_result_t *peer, const cerberus_c5_ble_advertisement_t *raw,
    uint8_t generation, uint8_t index, uint64_t epoch, bool epoch_valid,
    uint64_t original_ms, uint64_t ceiling_ms, bool source_valid)
{
    if (!out || !peer || !raw || peer->address_type > 3U ||
        raw->address_type != peer->address_type || memcmp(raw->address, peer->address, 6U) ||
        raw->length > CERBERUS_BLE_ARCHIVE_ADV_MAX ||
        (peer->uuid_length != 0U && peer->uuid_length != 2U &&
         peer->uuid_length != 4U && peer->uuid_length != 16U) ||
        (epoch_valid && !epoch) || (source_valid && original_ms > ceiling_ms)) return false;
    const char *end = memchr(peer->name, 0, sizeof(peer->name));
    if (!end) return false;
    cerberus_ble_archive_row_t row = {0};
    row.receiver = CERBERUS_BLE_ARCHIVE_RECEIVER_S3;
    row.rssi_kind = CERBERUS_BLE_ARCHIVE_RSSI_SWEEP_PEAK;
    memcpy(row.address, peer->address, sizeof(row.address));
    row.address_type = peer->address_type;
    memcpy(row.name, peer->name, (size_t)(end - peer->name));
    row.rssi_valid = cerberus_ble_rssi_valid(peer->rssi);
    row.rssi = row.rssi_valid ? peer->rssi : CERBERUS_BLE_ARCHIVE_RSSI_UNAVAILABLE;
    row.manufacturer_id = peer->manufacturer_id;
    row.peer_flags = peer->flags;
    row.uuid_length = peer->uuid_length;
    memcpy(row.uuid, peer->uuid, row.uuid_length);
    row.advertisement_length = raw->length;
    memcpy(row.advertisement, raw->data, raw->length);
    row.advertisement_event_type = raw->event_type;
    row.advertisement_flags = raw->flags;
    row.reports_seen = raw->reports_seen;
    row.generation = generation;
    row.index = index;
    row.scan_epoch_valid = epoch_valid;
    row.scan_epoch = epoch_valid ? epoch : 0U;
    row.source_time_valid = source_valid;
    row.original_source_uptime_ms = source_valid ? original_ms : 0U;
    row.copy_ceiling_ms = source_valid ? ceiling_ms : 0U;
    *out = row;
    return true;
}
