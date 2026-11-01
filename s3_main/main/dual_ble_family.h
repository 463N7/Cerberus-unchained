#pragma once

/* Passive hints from one complete original advertisement, not authenticated
 * identity, response correlation or action eligibility. The payload parser is
 * independent of receiver/UI types; the frozen C5 proof gate follows below.
 * No clock, allocation, RF or transport calls. Caller keeps inputs/output
 * separate; all failed classifications clear evidence and retain only state. */
#include "ble_ad_hints.h"
#include "ble_findmy_hints.h"
#include "ble_flipper_hints.h"
#include "ble_flock_hints.h"
#include "ble_flock_oui.h"
#include "ibeacon.h"

enum {
    CERBERUS_DUAL_BLE_FAMILY_IBEACON = 1U << 0,
    CERBERUS_DUAL_BLE_FAMILY_FIND_MY = 1U << 1,
    CERBERUS_DUAL_BLE_FAMILY_FLIPPER = 1U << 2,
    CERBERUS_DUAL_BLE_FAMILY_FLOCK = 1U << 3,
    CERBERUS_DUAL_BLE_FAMILY_META_HINT = 1U << 4,
    CERBERUS_DUAL_BLE_FAMILY_HC_NAME_HINT = 1U << 5,
};
enum {
    CERBERUS_DUAL_BLE_FAMILY_UNAVAILABLE,
    CERBERUS_DUAL_BLE_FAMILY_COMPLETE,
    CERBERUS_DUAL_BLE_FAMILY_NO_PROVENANCE,
    CERBERUS_DUAL_BLE_FAMILY_RECEIVE_UNKNOWN,
    CERBERUS_DUAL_BLE_FAMILY_TRUNCATED,
    CERBERUS_DUAL_BLE_FAMILY_MALFORMED,
    CERBERUS_DUAL_BLE_FAMILY_INVALID_SOURCE,
};
typedef struct {
    uint8_t categories, hc_name, state, ibeacon_result;
    bool available, flock_oui, meta_blocked;
    cerberus_ble_findmy_hints_t findmy;
    cerberus_ble_flipper_hints_t flipper;
    cerberus_ble_flock_hints_t flock;
    cerberus_ble_meta_evidence_t meta;
    cerberus_ibeacon_t ibeacon;
} cerberus_dual_ble_family_evidence_t;

_Static_assert(sizeof(cerberus_dual_ble_family_evidence_t) <= 48U,
    "Passive family detail must remain bounded");

static inline bool cerberus_dual_ble_family_unavailable(
    cerberus_dual_ble_family_evidence_t *out, uint8_t state)
{
    if (out) { memset(out, 0, sizeof(*out)); out->state = state; }
    return false;
}

/* Full delivered container only, <=255 bytes. The caller establishes that it
 * is an original and complete before invoking this portable parser. It does
 * not accept fragments or join other reports. A true zero-category result
 * means valid AD with no supported hint; false means unclassifiable evidence.
 * Categories, including blocked Meta identifiers, are same-report only.
 * Conflicting exact iBeacon fields may invalidate that tuple interpretation
 * while leaving the complete AD and independent family hints valid. */
static inline bool cerberus_dual_ble_family_parse_ad(const uint8_t *data,
    size_t length, const uint8_t address[6], uint8_t address_type,
    cerberus_dual_ble_family_evidence_t *out)
{
    if (!out) return false;
    if (!address || address_type > 3U || (!data && length) ||
        length > CERBERUS_BLE_AD_HINT_MAX_LENGTH)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_INVALID_SOURCE);
    cerberus_dual_ble_family_evidence_t pending = {0};
    cerberus_ble_ad_hints_t parsed;
    if (!cerberus_ble_ad_hints_parse(data, length, &parsed) ||
        !cerberus_ble_findmy_hints_parse(data, length, &pending.findmy) ||
        !cerberus_ble_flipper_hints_parse(data, length, &pending.flipper) ||
        !cerberus_ble_flock_hints_parse(data, length, &pending.flock))
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_MALFORMED);
    pending.available = true;
    pending.state = CERBERUS_DUAL_BLE_FAMILY_COMPLETE;
    pending.meta = parsed.meta;
    pending.meta_blocked = parsed.blocked_id;
    pending.hc_name = cerberus_ble_ad_hc_name_code(&parsed);
    pending.flock_oui = cerberus_ble_flock_oui_hint(address, address_type);
    pending.ibeacon_result = (uint8_t)cerberus_ibeacon_parse_ad(data, length, &pending.ibeacon);
    if (pending.ibeacon_result == CERBERUS_IBEACON_FOUND)
        pending.categories |= CERBERUS_DUAL_BLE_FAMILY_IBEACON;
    if (pending.findmy.bits) pending.categories |= CERBERUS_DUAL_BLE_FAMILY_FIND_MY;
    if (pending.flipper.bits) pending.categories |= CERBERUS_DUAL_BLE_FAMILY_FLIPPER;
    if (cerberus_ble_flock_hints_level(&pending.flock) || pending.flock_oui)
        pending.categories |= CERBERUS_DUAL_BLE_FAMILY_FLOCK;
    if (cerberus_ble_ad_meta_hint(&parsed)) pending.categories |= CERBERUS_DUAL_BLE_FAMILY_META_HINT;
    if (pending.hc_name) pending.categories |= CERBERUS_DUAL_BLE_FAMILY_HC_NAME_HINT;
    *out = pending;
    return true;
}

#include "dual_ble_scan.h"

/* Historical publication proof, never a current radio lease. Generation0 is
 * valid after wrap; boot+epoch must be nonzero and index must be in the bank.
 * Raw bytes alone supply family facts. Cached peer name, UUID, manufacturer,
 * RSSI, count and time fields cannot qualify or corroborate a hint.
 * Known receive metadata is required. A valid full report longer than31 is
 * still unclassifiable here because its suffix was not retained. Incomplete
 * EXT reports and responses never acquire this envelope. Nonlegacy SIDff is
 * allowed by the shared validator: one-original format hints do not claim a
 * particular advertising set or S3-style matched response. A later partial
 * callback may be absent from this retained-original view; no current/latest
 * callback or freshness claim follows from classification. */
static inline bool cerberus_dual_ble_family_classify(const cerberus_dual_ble_row_t *row,
    const cerberus_dual_ble_provenance_t *provenance,
    cerberus_dual_ble_family_evidence_t *out)
{
    if (!out) return false;
    if (!row)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_INVALID_SOURCE);
    if (!provenance || !provenance->boot_id || !provenance->epoch ||
        provenance->index >= CERBERUS_DUAL_BLE_CAPACITY)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_NO_PROVENANCE);
    if (row->address_type > 3U || row->event_type > 3U ||
        row->raw_length > CERBERUS_DUAL_BLE_RAW_BYTES ||
        (row->raw_flags & ~(CERBERUS_BLE_ADVERTISEMENT_TRUNCATED | CERBERUS_BLE_ADVERTISEMENT_MALFORMED)) ||
        !cerberus_dual_ble_receive_metadata_valid(&row->receive,
            row->raw_length, row->event_type, row->raw_flags))
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_INVALID_SOURCE);
    if (row->receive.source_kind == CERBERUS_DUAL_BLE_RECEIVE_UNKNOWN)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_RECEIVE_UNKNOWN);
    if ((row->raw_flags & CERBERUS_BLE_ADVERTISEMENT_TRUNCATED) ||
        row->receive.callback_length > CERBERUS_DUAL_BLE_RAW_BYTES)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_TRUNCATED);
    if ((row->raw_flags & CERBERUS_BLE_ADVERTISEMENT_MALFORMED) ||
        row->receive.ad_validation != CERBERUS_DUAL_BLE_AD_VALID)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_MALFORMED);
    if (row->receive.callback_length != row->raw_length)
        return cerberus_dual_ble_family_unavailable(out, CERBERUS_DUAL_BLE_FAMILY_INVALID_SOURCE);
    return cerberus_dual_ble_family_parse_ad(row->raw, row->raw_length,
        row->address, row->address_type, out);
}
