#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ble_archive.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { ++checks; assert(c); } while (0)
#define HEADER CERBERUS_BLE_ARCHIVE_HEADER_SIZE
#define ROW CERBERUS_BLE_ARCHIVE_ROW_SIZE
#define MAX CERBERUS_BLE_ARCHIVE_MAX_SIZE

static cerberus_ble_archive_t sample(unsigned count)
{
    cerberus_ble_archive_t d = {.count = (uint8_t)count, .saved_uptime_ms = UINT64_C(0x1020304050607080)};
    for (unsigned i = 0; i < count && i < 32; ++i) {
        cerberus_ble_archive_row_t *r = &d.rows[i];
        for (unsigned j = 0; j < 6; ++j) r->address[j] = (uint8_t)(j + 1);
        r->address[5] = (uint8_t)(6 + i);
        r->address_type = 2;
        r->name[0] = 'A'; r->name[1] = 1; r->name[2] = (char)0xff;
        r->rssi = -55; r->rssi_valid = true;
        r->manufacturer_id = 0x004c; r->peer_flags = 0xa5;
        r->uuid_length = 2; r->uuid[0] = 0x0d; r->uuid[1] = 0x18;
        r->advertisement_length = 3;
        r->advertisement[0] = 2; r->advertisement[1] = 1; r->advertisement[2] = 6;
        r->advertisement_event_type = 3; r->advertisement_flags = 0xf8;
        r->reports_seen = 0x1234;
        r->generation = 0xfe; r->index = (uint8_t)(31 - i);
        r->scan_epoch_valid = true; r->scan_epoch = UINT64_C(0x0102030405060708);
        r->source_time_valid = true;
        r->original_source_uptime_ms = UINT64_C(0x100000001);
        r->copy_ceiling_ms = UINT64_C(0x100000005);
    }
    return d;
}

static void put16(uint8_t *p, unsigned n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
static void put32(uint8_t *p, uint32_t n) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (i * 8)); }
static void put64(uint8_t *p, uint64_t n) { for (unsigned i = 0; i < 8; ++i) p[i] = (uint8_t)(n >> (i * 8)); }

/* Independent table-based CRC oracle, checked against Python zlib's fixed
 * golden bytes below. Production uses a bit-at-a-time CRC implementation. */
static uint32_t crc_table[256];
static void crc_init(void)
{
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (unsigned j = 0; j < 8; ++j) c = c & 1U ? (c >> 1) ^ UINT32_C(0xedb88320) : c >> 1;
        crc_table[i] = c;
    }
}
static void seal(uint8_t *p, size_t length)
{
    memset(p + 28, 0, 4);
    uint32_t c = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) c = crc_table[(c ^ p[i]) & 255U] ^ (c >> 8);
    put32(p + 28, ~c);
}

static size_t encode(const cerberus_ble_archive_t *d, uint8_t *out)
{
    size_t n = SIZE_MAX;
    CHECK(cerberus_ble_archive_encode(d, out, MAX, &n));
    CHECK(n == HEADER + (size_t)d->count * ROW);
    return n;
}

static void bad_decode(const uint8_t *p, size_t n)
{
    cerberus_ble_archive_t d, before;
    memset(&d, 0xa7, sizeof(d)); before = d;
    CHECK(!cerberus_ble_archive_decode(p, n, &d));
    CHECK(!memcmp(&d, &before, sizeof(d)));
}

static void bad_model(const cerberus_ble_archive_t *d)
{
    uint8_t out[MAX], before[MAX];
    memset(out, 0xb8, sizeof(out)); memcpy(before, out, sizeof(out));
    size_t written = 999;
    CHECK(!cerberus_ble_archive_validate(d));
    CHECK(!cerberus_ble_archive_encode(d, out, sizeof(out), &written));
    CHECK(!written);
    CHECK(!memcmp(out, before, sizeof(out)));
}

static void golden_and_bounds(void)
{
    static const char hex[] =
        "43424c45415243000100200080000100a0000000807060504030201031ca8e79"
        "010203040506020703c9a5024c000303f8fe1f00341200000807060504030201"
        "010000000100000005000000010000004101ff00000000000000000000000000"
        "000000000000000000000000000000000d180000000000000000000000000000"
        "0201060000000000000000000000000000000000000000000000000000000000";
    uint8_t bytes[MAX + 1], expected[160];
    for (unsigned i = 0; i < sizeof(expected); ++i) {
        unsigned x;
        CHECK(sscanf(hex + 2 * i, "%2x", &x) == 1);
        expected[i] = (uint8_t)x;
    }
    CHECK(sizeof(hex) == sizeof(expected) * 2 + 1);
    cerberus_ble_archive_t d = sample(1), copy = d, decoded;
    memset(bytes, 0xba, sizeof(bytes));
    const size_t n = encode(&d, bytes);
    CHECK(n == sizeof(expected));
    CHECK(!memcmp(bytes, expected, n));
    CHECK(bytes[n] == 0xba);
    CHECK(!memcmp(&d, &copy, sizeof(d)));
    CHECK(cerberus_ble_archive_decode(bytes, n, &decoded));
    CHECK(!memcmp(&decoded, &copy, sizeof(copy)));
    for (size_t cap = 0; cap < n; ++cap) {
        uint8_t out[MAX]; memset(out, 0xcc, sizeof(out));
        size_t written = 11;
        CHECK(!cerberus_ble_archive_encode(&d, out, cap, &written));
        CHECK(!written);
        for (size_t j = 0; j < sizeof(out); ++j) CHECK(out[j] == 0xcc);
    }
    size_t written = 11;
    CHECK(!cerberus_ble_archive_encode(&d, NULL, n, &written)); CHECK(!written);
    CHECK(!cerberus_ble_archive_encode(&d, bytes, n, NULL));
    bad_model(NULL); bad_decode(NULL, n);
    CHECK(!cerberus_ble_archive_decode(bytes, n, NULL));
    CHECK(!cerberus_ble_archive_validate(NULL));
    for (size_t cut = 0; cut < n; ++cut) bad_decode(bytes, cut);
    bad_decode(bytes, n + 1);
    bad_decode(bytes, SIZE_MAX);
    CHECK(!cerberus_ble_archive_encode(&d, (uint8_t *)&d, sizeof(d), &written));
    CHECK(!memcmp(&d, &copy, sizeof(d)));
    CHECK(!cerberus_ble_archive_decode((const uint8_t *)&d, n, &d));
    CHECK(!memcmp(&d, &copy, sizeof(d)));
    CHECK(!cerberus_ble_archive_encode(&d, (uint8_t *)&d + sizeof(d) - 1, n, &written));
    CHECK(!memcmp(&d, &copy, sizeof(d)));
}

static void model_validation(void)
{
    cerberus_ble_archive_t good = sample(1), d;
    for (unsigned count = 0; count < 256; ++count) {
        d = sample(count <= 32 ? count : 1); d.count = (uint8_t)count;
        CHECK(cerberus_ble_archive_validate(&d) == (count >= 1 && count <= 32));
    }
    for (unsigned type = 0; type < 256; ++type) {
        d = good; d.rows[0].address_type = (uint8_t)type;
        CHECK(cerberus_ble_archive_validate(&d) == (type <= 3));
    }
    for (int signal = -128; signal <= 127; ++signal) {
        d = good; d.rows[0].rssi = (int8_t)signal;
        CHECK(cerberus_ble_archive_validate(&d) == (signal >= -127 && signal <= 20));
        d.rows[0].rssi_valid = false;
        CHECK(cerberus_ble_archive_validate(&d) == (signal == 127));
    }
    for (unsigned len = 0; len < 256; ++len) {
        d = good; d.rows[0].uuid_length = (uint8_t)len;
        CHECK(cerberus_ble_archive_validate(&d) == (len == 0 || len == 2 || len == 4 || len == 16));
        d = good; d.rows[0].advertisement_length = (uint8_t)len;
        CHECK(cerberus_ble_archive_validate(&d) == (len <= 31));
        d = good; d.rows[0].index = (uint8_t)len;
        CHECK(cerberus_ble_archive_validate(&d) == (len < 32));
    }
    d = good; memset(d.rows[0].name, 'x', sizeof(d.rows[0].name)); bad_model(&d);
    d.rows[0].name[32] = 0; CHECK(cerberus_ble_archive_validate(&d));
    d = good; d.rows[0].scan_epoch = 0; bad_model(&d);
    d = good; d.rows[0].scan_epoch_valid = false; bad_model(&d);
    d.rows[0].scan_epoch = 0; CHECK(cerberus_ble_archive_validate(&d));
    d = good; d.rows[0].source_time_valid = false; bad_model(&d);
    d.rows[0].original_source_uptime_ms = 0; bad_model(&d);
    d.rows[0].copy_ceiling_ms = 0; CHECK(cerberus_ble_archive_validate(&d));
    d = good; d.rows[0].copy_ceiling_ms = d.rows[0].original_source_uptime_ms - 1; bad_model(&d);
    d = good; d.saved_uptime_ms = d.rows[0].copy_ceiling_ms - 1; bad_model(&d);
    d = sample(2); d.rows[1] = d.rows[0]; bad_model(&d);
    d.rows[1].address_type = 3; CHECK(cerberus_ble_archive_validate(&d));
    d = sample(32); d.rows[31] = d.rows[1]; bad_model(&d);
}

static void time_and_projection(void)
{
    uint8_t wire[MAX], wire2[MAX];
    cerberus_ble_archive_t d = sample(1), decoded;
    uint64_t age = 999;
    CHECK(cerberus_ble_archive_age_at_save(&d, 0, &age));
    CHECK(age == d.saved_uptime_ms - d.rows[0].original_source_uptime_ms);
    const uint64_t saved_age = age;
    size_t n = encode(&d, wire);
    CHECK(cerberus_ble_archive_decode(wire, n, &decoded));
    CHECK(cerberus_ble_archive_age_at_save(&decoded, 0, &age) && age == saved_age);
    /* There is intentionally no current-clock argument: a reboot cannot make
     * this historical row fresh or reinterpret its uptime as the new boot. */
    CHECK(!cerberus_ble_archive_age_at_save(NULL, 0, &age) && age == saved_age);
    CHECK(!cerberus_ble_archive_age_at_save(&d, 1, &age) && age == saved_age);
    CHECK(!cerberus_ble_archive_age_at_save(&d, SIZE_MAX, &age) && age == saved_age);
    CHECK(!cerberus_ble_archive_age_at_save(&d, 0, NULL));
    d.rows[0].source_time_valid = false;
    d.rows[0].original_source_uptime_ms = d.rows[0].copy_ceiling_ms = 0;
    CHECK(!cerberus_ble_archive_age_at_save(&d, 0, &age) && age == saved_age);
    n = encode(&d, wire);
    CHECK(cerberus_ble_archive_decode(wire, n, &decoded));
    CHECK(!decoded.rows[0].source_time_valid);
    d.rows[0].source_time_valid = true; d.saved_uptime_ms = 0;
    CHECK(cerberus_ble_archive_age_at_save(&d, 0, &age) && !age);
    n = encode(&d, wire); CHECK(cerberus_ble_archive_decode(wire, n, &decoded));
    d.saved_uptime_ms = UINT64_MAX;
    CHECK(cerberus_ble_archive_age_at_save(&d, 0, &age) && age == UINT64_MAX);
    d.rows[0].original_source_uptime_ms = d.rows[0].copy_ceiling_ms = UINT64_MAX;
    CHECK(cerberus_ble_archive_age_at_save(&d, 0, &age) && !age);
    d.rows[0].copy_ceiling_ms = 0;
    age = 99; CHECK(!cerberus_ble_archive_age_at_save(&d, 0, &age) && age == 99);

    /* Ignored model tails/unused rows do not enter the canonical file, while
     * meaningful non-ASCII/control name bytes and all ADV octets survive. */
    d = sample(1); memset(d.rows[0].name, 'z', 32); d.rows[0].name[32] = 0;
    n = encode(&d, wire); CHECK(cerberus_ble_archive_decode(wire, n, &decoded));
    CHECK(!memcmp(d.rows[0].name, decoded.rows[0].name, 33));
    d.rows[0].name[0] = 0;
    n = encode(&d, wire); CHECK(cerberus_ble_archive_decode(wire, n, &decoded));
    for (size_t i = 0; i < 33; ++i) CHECK(!decoded.rows[0].name[i]);
    memset(d.rows[0].name + 1, 0xa5, 32);
    memset(d.rows[0].uuid + 2, 0xb6, 14);
    memset(d.rows[0].advertisement + 3, 0xc7, 28);
    memset(d.rows + 1, 0xdd, sizeof(d.rows) - sizeof(d.rows[0]));
    CHECK(encode(&d, wire2) == n && !memcmp(wire, wire2, n));
    for (unsigned byte = 0; byte < 256; ++byte) {
        d = sample(1); d.rows[0].advertisement_length = 31;
        d.rows[0].advertisement_flags = (uint8_t)byte;
        d.rows[0].advertisement_event_type = (uint8_t)byte;
        d.rows[0].peer_flags = (uint8_t)byte;
        for (unsigned i = 0; i < 31; ++i) d.rows[0].advertisement[i] = (uint8_t)(byte + i);
        n = encode(&d, wire); CHECK(cerberus_ble_archive_decode(wire, n, &decoded));
        CHECK(!memcmp(d.rows[0].advertisement, decoded.rows[0].advertisement, 31));
        CHECK(decoded.rows[0].advertisement_flags == byte);
        CHECK(decoded.rows[0].advertisement_event_type == byte);
        CHECK(decoded.rows[0].peer_flags == byte);
    }
}

static void hostile_files(void)
{
    uint8_t wire[MAX + 1], changed[MAX + 1];
    cerberus_ble_archive_t d = sample(32);
    const size_t n = encode(&d, wire);
    CHECK(n == MAX);
    for (size_t cut = 0; cut < n; ++cut) bad_decode(wire, cut);
    bad_decode(wire, n + 1);
    for (size_t i = 0; i < n; ++i) {
        memcpy(changed, wire, n); changed[i] ^= 1;
        bad_decode(changed, n); /* Any byte, including the full final row. */
    }
    memcpy(changed, wire, n); memcpy(changed + HEADER + 31 * ROW, changed + HEADER, 7);
    seal(changed, n); bad_decode(changed, n); /* Duplicate last row is atomic. */
    memcpy(changed, wire, n); changed[HEADER + 31 * ROW + 127] = 1;
    seal(changed, n); bad_decode(changed, n);
    const unsigned invalid_u16_offsets[] = {8, 10, 12, 14};
    for (unsigned k = 0; k < sizeof(invalid_u16_offsets) / sizeof(invalid_u16_offsets[0]); ++k)
        for (unsigned value = 0; value <= 65535; value += 257) {
            memcpy(changed, wire, n); put16(changed + invalid_u16_offsets[k], value); seal(changed, n);
            bad_decode(changed, n);
        }
    memcpy(changed, wire, n); put32(changed + 16, UINT32_MAX); seal(changed, n); bad_decode(changed, n);
    memcpy(changed, wire, n); put64(changed + HEADER + 40, UINT64_MAX); seal(changed, n); bad_decode(changed, n);
    memcpy(changed, wire, n); put64(changed + HEADER + 32, UINT64_MAX); seal(changed, n); bad_decode(changed, n);
}

static void canonical_mutations(void)
{
    uint8_t good[MAX], wire[MAX], reencoded[MAX];
    cerberus_ble_archive_t seed = sample(1);
    const size_t n = encode(&seed, good);
    for (size_t offset = 0; offset < n; ++offset) {
        if (offset >= 28 && offset < 32) continue;
        for (unsigned value = 0; value < 256; ++value) {
            memcpy(wire, good, n); wire[offset] = (uint8_t)value; seal(wire, n);
            cerberus_ble_archive_t result, before;
            memset(&result, 0xc5, sizeof(result)); before = result;
            if (cerberus_ble_archive_decode(wire, n, &result)) {
                CHECK(cerberus_ble_archive_validate(&result));
                CHECK(encode(&result, reencoded) == n);
                CHECK(!memcmp(wire, reencoded, n));
                for (unsigned i = 1; i < 32; ++i) {
                    cerberus_ble_archive_row_t zero = {0};
                    CHECK(!memcmp(&result.rows[i], &zero, sizeof(zero)));
                }
            } else CHECK(!memcmp(&result, &before, sizeof(result)));
        }
    }
}

static uint32_t rng_state = UINT32_C(0x1379bdf);
static uint32_t random32(void)
{
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5;
    return rng_state;
}

static void generated_roundtrips(void)
{
    static const uint8_t uuid_lengths[] = {0, 2, 4, 16};
    uint8_t first[MAX], second[MAX];
    for (unsigned iteration = 0; iteration < 1500; ++iteration) {
        cerberus_ble_archive_t d = sample(1 + random32() % 32), decoded;
        d.saved_uptime_ms = (uint64_t)random32() << 32 | random32();
        for (unsigned i = 0; i < d.count; ++i) {
            cerberus_ble_archive_row_t *r = &d.rows[i];
            r->address_type = (uint8_t)(random32() % 4);
            memset(r->name, 0, sizeof(r->name));
            const unsigned length = random32() % 33;
            for (unsigned j = 0; j < length; ++j) r->name[j] = (char)(1 + random32() % 255);
            r->rssi_valid = (random32() & 1) != 0;
            r->rssi = r->rssi_valid ? (int8_t)((int)(random32() % 148) - 127) : 127;
            r->peer_flags = (uint8_t)random32(); r->manufacturer_id = (uint16_t)random32();
            r->uuid_length = uuid_lengths[random32() % 4];
            for (unsigned j = 0; j < 16; ++j) r->uuid[j] = (uint8_t)random32();
            r->advertisement_length = (uint8_t)(random32() % 32);
            for (unsigned j = 0; j < 31; ++j) r->advertisement[j] = (uint8_t)random32();
            r->advertisement_event_type = (uint8_t)random32();
            r->advertisement_flags = (uint8_t)random32();
            r->reports_seen = (uint16_t)random32(); r->generation = (uint8_t)random32();
            r->index = (uint8_t)(random32() % 32);
            r->scan_epoch_valid = (random32() & 1) != 0;
            r->scan_epoch = r->scan_epoch_valid ? (uint64_t)random32() + 1 : 0;
            r->source_time_valid = (random32() & 1) != 0;
            r->original_source_uptime_ms = r->source_time_valid ? d.saved_uptime_ms / 3 : 0;
            r->copy_ceiling_ms = r->source_time_valid ? d.saved_uptime_ms / 2 : 0;
        }
        cerberus_ble_archive_t before = d;
        const size_t n = encode(&d, first);
        CHECK(!memcmp(&d, &before, sizeof(d)));
        CHECK(cerberus_ble_archive_decode(first, n, &decoded));
        CHECK(cerberus_ble_archive_validate(&decoded));
        CHECK(encode(&decoded, second) == n && !memcmp(first, second, n));
        for (unsigned i = 0; i < d.count; ++i) {
            CHECK(!memcmp(d.rows[i].name, decoded.rows[i].name, sizeof(d.rows[i].name)));
            CHECK(!memcmp(d.rows[i].advertisement, decoded.rows[i].advertisement, d.rows[i].advertisement_length));
            CHECK(decoded.rows[i].address_type == d.rows[i].address_type);
            CHECK(!memcmp(decoded.rows[i].address, d.rows[i].address, 6));
        }
    }
}

int main(void)
{
    crc_init();
    golden_and_bounds();
    model_validation();
    time_and_projection();
    hostile_files();
    canonical_mutations();
    generated_roundtrips();
    printf("BLE archive codec: PASS %u checks; header=%u row=%u max=%u; host document=%zu row=%zu bytes\n",
        checks, HEADER, ROW, MAX, sizeof(cerberus_ble_archive_t), sizeof(cerberus_ble_archive_row_t));
    return 0;
}
