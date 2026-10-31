#include "ble_archive.h"

#include <string.h>

enum {
    ROW_RSSI_VALID = 1U << 0,
    ROW_SOURCE_TIME_VALID = 1U << 1,
    ROW_SCAN_EPOCH_VALID = 1U << 2,
    ROW_VALIDITY_MASK = ROW_RSSI_VALID | ROW_SOURCE_TIME_VALID | ROW_SCAN_EPOCH_VALID,
};

static const uint8_t archive_magic[8] = {'C', 'B', 'L', 'E', 'A', 'R', 'C', 0};

static bool overlaps(const void *left, size_t left_size, const void *right, size_t right_size)
{
    const uintptr_t a = (uintptr_t)left, b = (uintptr_t)right;
    if (!left_size || !right_size) return false;
    return a <= b ? b - a < left_size : a - b < right_size;
}

static size_t name_length(const char *name)
{
    for (size_t i = 0; i <= CERBERUS_BLE_ARCHIVE_NAME_MAX; ++i)
        if (!name[i]) return i;
    return CERBERUS_BLE_ARCHIVE_NAME_MAX + 1U;
}

static bool row_valid(const cerberus_ble_archive_row_t *r, uint64_t saved_ms)
{
    if (r->address_type > 3U || r->index >= CERBERUS_BLE_ARCHIVE_MAX_ROWS ||
        name_length(r->name) > CERBERUS_BLE_ARCHIVE_NAME_MAX ||
        (r->uuid_length != 0U && r->uuid_length != 2U &&
         r->uuid_length != 4U && r->uuid_length != CERBERUS_BLE_ARCHIVE_UUID_MAX) ||
        r->advertisement_length > CERBERUS_BLE_ARCHIVE_ADV_MAX) return false;
    if (r->rssi_valid ? (r->rssi < -127 || r->rssi > 20) :
        r->rssi != CERBERUS_BLE_ARCHIVE_RSSI_UNAVAILABLE) return false;
    if (r->scan_epoch_valid ? !r->scan_epoch : r->scan_epoch != 0U) return false;
    if (r->source_time_valid) {
        if (r->original_source_uptime_ms > r->copy_ceiling_ms ||
            r->copy_ceiling_ms > saved_ms) return false;
    } else if (r->original_source_uptime_ms || r->copy_ceiling_ms) return false;
    return true;
}

bool cerberus_ble_archive_validate(const cerberus_ble_archive_t *document)
{
    if (!document || !document->count || document->count > CERBERUS_BLE_ARCHIVE_MAX_ROWS)
        return false;
    for (size_t i = 0; i < document->count; ++i) {
        const cerberus_ble_archive_row_t *row = &document->rows[i];
        if (!row_valid(row, document->saved_uptime_ms)) return false;
        for (size_t j = 0; j < i; ++j)
            if (row->address_type == document->rows[j].address_type &&
                !memcmp(row->address, document->rows[j].address, sizeof(row->address))) return false;
    }
    return true;
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t get64(const uint8_t *p)
{
    return (uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32;
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8U * i));
}

static void put64(uint8_t *p, uint64_t value)
{
    put32(p, (uint32_t)value);
    put32(p + 4, (uint32_t)(value >> 32));
}

/* Header bytes 28..31 hold the CRC and are treated as zero in the calculation. */
static uint32_t archive_crc(const uint8_t *bytes, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= i >= 28U && i < 32U ? 0U : bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return ~crc;
}

static void encode_row(const cerberus_ble_archive_row_t *r, uint8_t *out)
{
    const size_t name_size = name_length(r->name);
    memset(out, 0, CERBERUS_BLE_ARCHIVE_ROW_SIZE);
    memcpy(out, r->address, 6);
    out[6] = r->address_type;
    out[7] = (r->rssi_valid ? ROW_RSSI_VALID : 0U) |
        (r->source_time_valid ? ROW_SOURCE_TIME_VALID : 0U) |
        (r->scan_epoch_valid ? ROW_SCAN_EPOCH_VALID : 0U);
    out[8] = (uint8_t)name_size;
    out[9] = (uint8_t)r->rssi;
    out[10] = r->peer_flags;
    out[11] = r->uuid_length;
    put16(out + 12, r->manufacturer_id);
    out[14] = r->advertisement_length;
    out[15] = r->advertisement_event_type;
    out[16] = r->advertisement_flags;
    out[17] = r->generation;
    out[18] = r->index;
    put16(out + 20, r->reports_seen);
    put64(out + 24, r->scan_epoch);
    put64(out + 32, r->original_source_uptime_ms);
    put64(out + 40, r->copy_ceiling_ms);
    memcpy(out + 48, r->name, name_size);
    memcpy(out + 80, r->uuid, r->uuid_length);
    memcpy(out + 96, r->advertisement, r->advertisement_length);
}

static bool all_zero(const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) if (bytes[i]) return false;
    return true;
}

static bool decode_row(const uint8_t *p, uint64_t saved_ms, cerberus_ble_archive_row_t *r)
{
    if ((p[7] & ~ROW_VALIDITY_MASK) || p[8] > CERBERUS_BLE_ARCHIVE_NAME_MAX ||
        p[11] > CERBERUS_BLE_ARCHIVE_UUID_MAX || p[14] > CERBERUS_BLE_ARCHIVE_ADV_MAX ||
        p[19] || p[22] || p[23] || p[127] ||
        !all_zero(p + 48 + p[8], CERBERUS_BLE_ARCHIVE_NAME_MAX - p[8]) ||
        !all_zero(p + 80 + p[11], CERBERUS_BLE_ARCHIVE_UUID_MAX - p[11]) ||
        !all_zero(p + 96 + p[14], CERBERUS_BLE_ARCHIVE_ADV_MAX - p[14])) return false;
    for (size_t i = 0; i < p[8]; ++i) if (!p[48 + i]) return false;
    memset(r, 0, sizeof(*r));
    memcpy(r->address, p, 6);
    r->address_type = p[6];
    r->rssi_valid = (p[7] & ROW_RSSI_VALID) != 0;
    r->source_time_valid = (p[7] & ROW_SOURCE_TIME_VALID) != 0;
    r->scan_epoch_valid = (p[7] & ROW_SCAN_EPOCH_VALID) != 0;
    /* Avoid implementation-defined unsigned-to-signed narrowing for RSSI. */
    r->rssi = p[9] <= INT8_MAX ? (int8_t)p[9] : (int8_t)((int)p[9] - 256);
    r->peer_flags = p[10];
    r->uuid_length = p[11];
    r->manufacturer_id = get16(p + 12);
    r->advertisement_length = p[14];
    r->advertisement_event_type = p[15];
    r->advertisement_flags = p[16];
    r->generation = p[17];
    r->index = p[18];
    r->reports_seen = get16(p + 20);
    r->scan_epoch = get64(p + 24);
    r->original_source_uptime_ms = get64(p + 32);
    r->copy_ceiling_ms = get64(p + 40);
    memcpy(r->name, p + 48, p[8]);
    memcpy(r->uuid, p + 80, p[11]);
    memcpy(r->advertisement, p + 96, p[14]);
    return row_valid(r, saved_ms);
}

bool cerberus_ble_archive_encode(const cerberus_ble_archive_t *document,
    uint8_t *output, size_t capacity, size_t *written)
{
    if (!written) return false;
    /* Bad aliased metadata cannot be cleared without modifying the input. */
    if ((document && overlaps(written, sizeof(*written), document, sizeof(*document))) ||
        (output && overlaps(written, sizeof(*written), output, capacity))) return false;
    *written = 0;
    if (!output || !cerberus_ble_archive_validate(document)) return false;
    const size_t size = CERBERUS_BLE_ARCHIVE_HEADER_SIZE +
        (size_t)document->count * CERBERUS_BLE_ARCHIVE_ROW_SIZE;
    if (capacity < size || overlaps(document, sizeof(*document), output, size)) return false;
    memset(output, 0, CERBERUS_BLE_ARCHIVE_HEADER_SIZE);
    memcpy(output, archive_magic, sizeof(archive_magic));
    put16(output + 8, CERBERUS_BLE_ARCHIVE_VERSION);
    put16(output + 10, CERBERUS_BLE_ARCHIVE_HEADER_SIZE);
    put16(output + 12, CERBERUS_BLE_ARCHIVE_ROW_SIZE);
    put16(output + 14, document->count);
    put32(output + 16, (uint32_t)size);
    put64(output + 20, document->saved_uptime_ms);
    for (size_t i = 0; i < document->count; ++i)
        encode_row(&document->rows[i], output + CERBERUS_BLE_ARCHIVE_HEADER_SIZE +
            i * CERBERUS_BLE_ARCHIVE_ROW_SIZE);
    put32(output + 28, archive_crc(output, size));
    *written = size;
    return true;
}

bool cerberus_ble_archive_decode(const uint8_t *input, size_t length,
    cerberus_ble_archive_t *document)
{
    if (!input || !document || length < CERBERUS_BLE_ARCHIVE_HEADER_SIZE ||
        length > CERBERUS_BLE_ARCHIVE_MAX_SIZE ||
        overlaps(input, length, document, sizeof(*document)) ||
        memcmp(input, archive_magic, sizeof(archive_magic)) ||
        get16(input + 8) != CERBERUS_BLE_ARCHIVE_VERSION ||
        get16(input + 10) != CERBERUS_BLE_ARCHIVE_HEADER_SIZE ||
        get16(input + 12) != CERBERUS_BLE_ARCHIVE_ROW_SIZE) return false;
    const uint16_t count = get16(input + 14);
    const uint64_t saved_ms = get64(input + 20);
    if (!count || count > CERBERUS_BLE_ARCHIVE_MAX_ROWS ||
        length != CERBERUS_BLE_ARCHIVE_HEADER_SIZE + (size_t)count * CERBERUS_BLE_ARCHIVE_ROW_SIZE ||
        get32(input + 16) != length || get32(input + 28) != archive_crc(input, length)) return false;
    cerberus_ble_archive_row_t row;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *encoded = input + CERBERUS_BLE_ARCHIVE_HEADER_SIZE + i * CERBERUS_BLE_ARCHIVE_ROW_SIZE;
        if (!decode_row(encoded, saved_ms, &row)) return false;
        for (size_t j = 0; j < i; ++j)
            if (!memcmp(encoded, input + CERBERUS_BLE_ARCHIVE_HEADER_SIZE +
                j * CERBERUS_BLE_ARCHIVE_ROW_SIZE, 7)) return false;
    }
    /* Publication occurs only after every row and the complete file passed. */
    memset(document, 0, sizeof(*document));
    document->saved_uptime_ms = saved_ms;
    document->count = (uint8_t)count;
    for (size_t i = 0; i < count; ++i)
        (void)decode_row(input + CERBERUS_BLE_ARCHIVE_HEADER_SIZE +
            i * CERBERUS_BLE_ARCHIVE_ROW_SIZE, saved_ms, &document->rows[i]);
    return true;
}

bool cerberus_ble_archive_age_at_save(const cerberus_ble_archive_t *document,
    size_t index, uint64_t *age_ms)
{
    if (!document || !age_ms || !document->count || document->count > CERBERUS_BLE_ARCHIVE_MAX_ROWS ||
        index >= document->count || !row_valid(&document->rows[index], document->saved_uptime_ms) ||
        !document->rows[index].source_time_valid) return false;
    *age_ms = document->saved_uptime_ms - document->rows[index].original_source_uptime_ms;
    return true;
}
