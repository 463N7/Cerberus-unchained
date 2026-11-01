#include "dual_ble_archive_capture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static cerberus_dual_ble_row_t observation(void)
{
    cerberus_dual_ble_row_t row={.address={1,2,3,4,5,6},.address_type=3,.rssi=-93,
        .name="OLD PEER NAME",.uuid={0x44,0xfd},.uuid_length=2,
        .raw={5,0xff,0x06,0x00,0x71,0x72},.raw_length=6,
        .manufacturer_id=0x004c,.reports=65535,.flags=CERBERUS_BLE_RESULT_CONNECTABLE,
        .timing_flags=CERBERUS_DUAL_BLE_SEEN|CERBERUS_DUAL_BLE_RSSI_VALID,
        .callback_age_ms=7000,.request_started_ms=100};
    return row;
}
static const cerberus_dual_ble_provenance_t provenance={
    .boot_id=UINT64_C(0xfedcba9876543210),.epoch=UINT64_C(0x123456789abcdef0),
    .generation=0,.index=31};
static void assert_exact_times_unknown(const cerberus_ble_archive_row_t *row)
{
    assert(!row->source_time_valid && !row->original_source_uptime_ms && !row->copy_ceiling_ms);
    assert(row->response_state==CERBERUS_BLE_ARCHIVE_RESPONSE_UNKNOWN);
    assert(!row->response_length && !row->response_original_length && !row->response_event_type &&
        !row->response_flags && !row->response_sid && !row->response_sid_valid && !row->response_time_valid &&
        !row->response_original_sequence && !row->response_sequence && !row->response_original_observed_ms &&
        !row->response_observed_ms && !row->response_source_uptime_ms);
    for(unsigned i=0;i<sizeof(row->response);++i)assert(!row->response[i]);
}
static void reject_unchanged(const cerberus_dual_ble_row_t *source,
    const cerberus_dual_ble_provenance_t *identity)
{
    cerberus_ble_archive_row_t out;memset(&out,0xa5,sizeof(out));
    const cerberus_ble_archive_row_t sentinel=out;
    assert(!cerberus_dual_ble_archive_capture_row(&out,source,identity,500));
    assert(!memcmp(&out,&sentinel,sizeof(out)));
}
int main(void)
{
    cerberus_dual_ble_row_t source=observation();
    cerberus_ble_archive_row_t row;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,500));
    assert(row.receiver==CERBERUS_BLE_ARCHIVE_RECEIVER_C5);
    assert(row.rssi_kind==CERBERUS_BLE_ARCHIVE_RSSI_LATEST_ADV && row.rssi_valid && row.rssi==-93);
    assert(row.receiver_boot_id==provenance.boot_id && row.scan_epoch_valid && row.scan_epoch==provenance.epoch);
    assert(row.generation==0 && row.index==31 && row.address_type==3);
    assert(row.reports_seen==65535 && row.age_upper_valid && row.age_upper_at_save_ms==7400);
    assert_exact_times_unknown(&row);
    /* Aggregate peer hints are copied separately from a different raw company
     * ID. A mapper must never edit the packet to make the metadata agree. */
    assert(!strcmp(row.name,"OLD PEER NAME") && row.manufacturer_id==0x004c);
    assert(row.uuid_length==2 && row.uuid[0]==0x44 && row.uuid[1]==0xfd);
    assert(row.advertisement_length==source.raw_length && !memcmp(row.advertisement,source.raw,source.raw_length));
    assert(row.advertisement[2]==0x06 && row.advertisement_event_type==source.event_type);
    cerberus_ble_archive_t document={.saved_uptime_ms=500,.count=1};document.rows[0]=row;
    assert(cerberus_ble_archive_validate(&document));
    uint64_t age=0;assert(cerberus_ble_archive_age_upper_at_save(&document,0,&age) && age==7400);
    /* Evidence older than the S3 boot is valid as a bound, not an exact time. */
    assert(age>document.saved_uptime_ms);

    /* Raw malformed/truncated evidence survives byte-for-byte. Unused tails
     * are canonicalized, not made into new evidence or parsed for eligibility. */
    source.raw_flags=CERBERUS_BLE_ADVERTISEMENT_MALFORMED|CERBERUS_BLE_ADVERTISEMENT_TRUNCATED;
    source.raw_length=31;for(unsigned i=0;i<31;++i)source.raw[i]=(uint8_t)(255-i);
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,500));
    assert(row.advertisement_flags==source.raw_flags && !memcmp(row.advertisement,source.raw,31));
    document.rows[0]=row;assert(cerberus_ble_archive_validate(&document));
    uint8_t encoded[CERBERUS_BLE_ARCHIVE_MAX_SIZE];size_t written=0;
    assert(cerberus_ble_archive_encode(&document,encoded,sizeof(encoded),&written));
    cerberus_ble_archive_t decoded;
    assert(cerberus_ble_archive_decode(encoded,written,&decoded));
    assert(decoded.rows[0].receiver_boot_id==provenance.boot_id && decoded.rows[0].scan_epoch==provenance.epoch);
    assert(decoded.rows[0].advertisement_flags==source.raw_flags && !memcmp(decoded.rows[0].advertisement,source.raw,31));
    assert(cerberus_ble_archive_age_upper_at_save(&decoded,0,&age) && age==7400);
    assert_exact_times_unknown(&decoded.rows[0]);

    source=observation();source.request_started_ms=0;source.callback_age_ms=0;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,0));
    assert(row.age_upper_valid && row.age_upper_at_save_ms==0);assert_exact_times_unknown(&row);
    source.callback_age_ms=7;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,UINT64_MAX-7));
    assert(row.age_upper_valid && row.age_upper_at_save_ms==UINT64_MAX);
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,UINT64_MAX-6));
    assert(!row.age_upper_valid && !row.age_upper_at_save_ms);
    source=observation();
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,99));
    assert(!row.age_upper_valid && !row.age_upper_at_save_ms);
    source.callback_age_ms=UINT32_MAX;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,500));
    assert(!row.age_upper_valid && !row.age_upper_at_save_ms);
    source=observation();source.timing_flags=CERBERUS_DUAL_BLE_RSSI_VALID;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,500));
    assert(!row.age_upper_valid && row.rssi_valid && row.rssi==-93);
    source.timing_flags=CERBERUS_DUAL_BLE_SEEN;source.rssi=127;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,500));
    assert(row.age_upper_valid && !row.rssi_valid && row.rssi==CERBERUS_BLE_ARCHIVE_RSSI_UNAVAILABLE);

    source=observation();reject_unchanged(NULL,&provenance);reject_unchanged(&source,NULL);
    cerberus_dual_ble_provenance_t bad=provenance;
    bad.boot_id=0;reject_unchanged(&source,&bad);bad=provenance;
    bad.epoch=0;reject_unchanged(&source,&bad);bad=provenance;
    bad.index=32;reject_unchanged(&source,&bad);
    source.address_type=4;reject_unchanged(&source,&provenance);source=observation();
    source.uuid_length=3;reject_unchanged(&source,&provenance);source=observation();
    source.raw_length=32;reject_unchanged(&source,&provenance);source=observation();
    source.event_type=5;reject_unchanged(&source,&provenance);source=observation();
    source.flags=0x80;reject_unchanged(&source,&provenance);source=observation();
    source.raw_flags=0x80;reject_unchanged(&source,&provenance);source=observation();
    source.timing_flags=0x80;reject_unchanged(&source,&provenance);source=observation();
    source.rssi=21;reject_unchanged(&source,&provenance);source=observation();
    source.rssi=-128;reject_unchanged(&source,&provenance);source=observation();
    memset(source.name,'X',sizeof(source.name));reject_unchanged(&source,&provenance);
    source.name[32]=0;
    assert(cerberus_dual_ble_archive_capture_row(&row,&source,&provenance,500));
    assert(strlen(row.name)==32);
    puts("C5 archive capture: frozen identity, raw/aggregate separation, v3 round-trip with UNKNOWN response, checked age bounds and transactional rejection passed");
    return 0;
}
