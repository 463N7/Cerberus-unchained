# Saved BLE evidence format

This guide describes the v4 format in installed rev102. Matching host/native checks, image verification and boot do not establish physical SD or BLE reception acceptance. See [the rev102 checkpoint](DUAL_RADIO_REV102.md) for build/installation evidence and [the rev101 checkpoint](DUAL_RADIO_REV101.md) for preceding evidence and scope. The dual-receiver saved-report workflow still needs physical acceptance. [CONTRIBUTION_REV93.md](CONTRIBUTION_REV93.md) retains the earlier contribution's scope and evidence.

Version 4 writes a bounded, passive report of one to 64 retained BLE observations, at most 32 per receiver. The reader also accepts version 3 files with separate S3 response evidence, version 2 dual-receiver files and version 1 files, which contain at most 32 observations with no encoded receiver identity. The pure codec is in `s3_main/main/ble_archive.c` and `ble_archive.h`. Its receive-metadata type and validator come from the portable `shared/dual_ble_protocol.h`; it performs no allocation, filesystem operation, radio operation, or ESP-IDF call.

| Version | Header | Row | Maximum count | Maximum file |
| --- | ---: | ---: | ---: | ---: |
| 1, read compatibility | 32 bytes | 128 bytes | 32 | 4,128 bytes |
| 2, read compatibility | 32 bytes | 144 bytes | 64, at most 32 per receiver | 9,248 bytes |
| 3, read compatibility | 32 bytes | 208 bytes | 64, at most 32 per receiver | 13,344 bytes |
| 4, current writer/reader | 32 bytes | 216 bytes | 64, at most 32 per receiver | 13,856 bytes |

Files retain the `.BLE` extension. Older readers may reject version 4 files; compatibility means that the new reader preserves support for old files, without silently rewriting them. Encoding a decoded version 1 model produces an explicit version 4 report with `LEGACY` receiver labels. V1/v2 decode with response state `UNKNOWN` and all response metadata zero; v3 retains its original response bytes and timestamps. All v1/v2/v3 records decode with receive metadata `UNKNOWN` and eight zero fields. Old C5 provenance or raw bytes cannot reconstruct an absent callback envelope.

This format preserves retained evidence for offline inspection. Loading it does not create live scan results, connection targets, or permission to use an active feature. It is distinct from Marauder's JSON AirTag log format and does not import that format.

## Evidence and time limits

- An entry retains the receiver, typed BLE address, a projected received name, explicitly described RSSI, peer flags, manufacturer and UUID metadata, and up to 31 original advertisement bytes. Address bytes use NimBLE order, with the least significant octet first. Identity is `(receiver, address type, address)`: the same typed address heard by S3 and C5 is two observations, not evidence of two physical devices. `LEGACY` means the source was unspecified in version 1; decoding does not invent an S3 receiver label.
- The name is a bounded C-string projection from the peer result. It is **not** a lossless copy of the original over-the-air name: earlier peer conversion may have discarded bytes following an embedded NUL, and the C5 companion has already sanitized its display projection. The codec preserves the bytes it receives, including S3 control/non-ASCII bytes; it cannot restore an earlier name or infer it from unrelated raw data.
- Advertisement bytes, event type, and flag bits are preserved even when the retained advertisement is malformed, truncated, or unfamiliar. A malformed AD element does not invalidate the archive container. Offline parsers must check AD lengths independently and label truncation or malformed contents.
- Versions 3 and 4 can retain up to 31 bytes of the latest admitted S3 scan response independently of its original advertisement. They record the delivered response length, retained length, event, truncation, source SID where valid, ordered original/response sequences, their observed low32 source times, and an independently valid exact64 response time. They do not serialize pointer-containing focus objects, response RSSI, or older response history. A response longer than 31 bytes keeps only that prefix; the prefix can end inside an AD element. This is not full extended-advertising support.
- `UNKNOWN` means response availability cannot be proved for this saved original, including old files, C5/LEGACY rows and stale raw ADV retained after a newer invalid original. `NONE` means the frozen valid S3 original has no **admitted matching** response at capture; it does not prove that no response existed over the air. `PRESENT` means the separately saved response passed the same-source binding checks. Non-PRESENT response fields must be zero. C5/LEGACY rows must be UNKNOWN; C5 passive discovery does not collect scan responses.
- The S3 mapper attaches response bytes only after matching the frozen base row's typed identity, original raw bytes/event/flags/report count, generation, full nonzero source epoch and exact original/copy clocks. The source helper also requires the raw advertisement sequence to match the current valid original sequence. A newer malformed/incomplete original, including one observed in the same millisecond, cannot attach later response hints to an older raw copy. Response capture does not update original bytes, original time, RSSI or active-action eligibility.
- V4 separately retains the C5 original callback's eight-byte receive envelope: callback source kind, properties, delivered length, SID, primary/secondary PHY, controller data status and independent full-AD validation. It is copied from the same frozen row as raw bytes/RSSI/age and validated against that row's retained raw length/event/flags. A known envelope is allowed only for C5. S3 and archive `LEGACY` receivers keep canonical UNKNOWN; a matching S3 response SID is not C5 receive evidence. Metadata source kind `LEGACY` means a known legacy callback received by C5 and is distinct from archive receiver `LEGACY`.
- Known legacy callback metadata has unavailable status/SID `0xff`, no observed PHY (`0/0`) and zero properties. Known extended metadata describes a complete delivered original report, with the actual source SID/PHY values or explicit unavailable SID `0xff`. Delivered length is bounded to 255, raw retention remains 31, and full-AD validity can remain VALID even when the retained prefix cuts an AD element or NimBLE's parser rejects another aspect. This is historical source metadata, not complete payload retention, fragment reassembly, authenticity or proof that the advertiser remains nearby.
- `S3` and `LEGACY` entries use original-report sweep-peak RSSI, which need not describe the particular retained ADV bytes. C5 entries use latest original-ADV RSSI. The name, manufacturer and UUID remain aggregate peer hints, not a claim that every value occurred in that raw packet. An unavailable RSSI is 127 with its validity bit clear, for either RSSI kind.
- `saved_uptime_ms` always describes the S3 save boot. For S3/LEGACY, valid exact source times satisfy `original <= ceiling <= saved`; zero is valid when explicitly marked valid. The copy ceiling is not a packet timestamp. Those receivers must have no C5 age bound or receiver boot ID.
- C5 rows require a nonzero committed C5 boot ID and valid nonzero source epoch. Exact S3 source/copy times are zero and invalid. An optional conservative **upper bound on age at save** is stored separately; it may exceed S3 saved uptime and is never subtracted to invent an exact packet timestamp. Unknown bounds have a clear validity bit and value 0. A valid bound of 0 or `UINT64_MAX` is distinguishable from unknown through that bit.
- `cerberus_ble_archive_age_at_save()` returns exact historical original-ADV age for S3/LEGACY. `cerberus_ble_archive_age_upper_at_save()` returns the C5 historical upper bound. `cerberus_ble_archive_response_age_at_save()` returns the independent exact S3 response age when available. All reject unavailable or invalid values without changing their output. Opening a file after reboot does not refresh any age; none accepts the loading boot's clock. There is no UTC timestamp or claim of present freshness.
- Scan epoch, generation, and original row index are historical source metadata. They are not an active scanner ownership token, an address identity, or an authorization binding.

## Header

All multibyte integers are unsigned little-endian. The format is independent of native C structure size, alignment, padding, and pointers.

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 8 | Magic bytes `43 42 4C 45 41 52 43 00` (`CBLEARC` followed by NUL) |
| 8 | 2 | Version 1, 2, 3 or 4; encoder writes 4 |
| 10 | 2 | Header size, exactly 32 |
| 12 | 2 | Entry size 128 for v1, 144 for v2, 208 for v3, 216 for v4 |
| 14 | 2 | Entry count 1..32 for v1, 1..64 for v2/v3/v4; each receiver is limited to 32 |
| 16 | 4 | Total file length, exactly `32 + entry_size * count` |
| 20 | 8 | Saved S3 uptime in milliseconds |
| 28 | 4 | CRC-32/ISO-HDLC |

The CRC uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. It covers the **entire header and all entries**, with bytes 28 through 31 treated as zero during calculation. This detects corruption; it does not authenticate the file or its claimed observations.

## Entry

Offsets below are relative to the beginning of each entry. Bytes 0..127 retain their existing fields, with the explicitly versioned additions noted below; v2 appends 16 bytes, v3 appends 64 response bytes and v4 appends eight receive-metadata bytes. Earlier fields retain their offsets and meaning in later versions.

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 6 | Address in native NimBLE byte order |
| 6 | 1 | Address type, 0 through 3 |
| 7 | 1 | Validity bits: bit 0 RSSI; bit 1 exact source times; bit 2 scan epoch; v2 bit 3 age upper bound; other bits zero. v1 bit 3 must be zero |
| 8 | 1 | Name projection byte count, 0 through 32 |
| 9 | 1 | Signed two's-complement RSSI, -127 through +20 when valid; 127 when unavailable |
| 10 | 1 | Peer flags, preserved without reinterpretation |
| 11 | 1 | UUID length, 0, 2, 4, or 16 |
| 12 | 2 | Manufacturer identifier |
| 14 | 1 | Retained original advertisement length, 0 through 31 |
| 15 | 1 | Original advertisement event type, preserved |
| 16 | 1 | Original advertisement flags, preserved |
| 17 | 1 | Source generation |
| 18 | 1 | Original source index, 0 through 31 |
| 19 | 1 | v1 reserved zero; v2 receiver: 0 LEGACY, 1 S3, 2 C5 |
| 20 | 2 | Delivered original advertising report count |
| 22 | 1 | v1 reserved zero; v2 RSSI kind: 0 sweep peak, 1 latest original ADV |
| 23 | 1 | Reserved, zero |
| 24 | 8 | Scan epoch; nonzero if valid, otherwise zero |
| 32 | 8 | Original source uptime in milliseconds; zero if unavailable |
| 40 | 8 | Copy ceiling uptime in milliseconds; zero if unavailable |
| 48 | 32 | Name projection; no NUL inside its declared length; remaining bytes zero |
| 80 | 16 | UUID bytes; remaining bytes zero |
| 96 | 31 | Retained original advertisement bytes; remaining bytes zero |
| 127 | 1 | Reserved, zero |
| 128 | 8 | v2 onward: conservative age upper bound at save, zero if unavailable |
| 136 | 8 | v2 onward: nonzero C5 receiver boot ID; zero for S3/LEGACY |
| 144 | 1 | v3 onward: response state 0 UNKNOWN, 1 NONE, 2 PRESENT |
| 145 | 1 | Retained response length, 1..31 when PRESENT |
| 146 | 1 | Delivered response length, 1..255 when PRESENT |
| 147 | 1 | Response event: 4 without SID; 0 or 3 with valid SID |
| 148 | 1 | Response raw flags: bit 0 TRUNCATED iff delivered length exceeds retained length; other bits zero |
| 149 | 1 | Response/original source SID, 0..15 when valid, otherwise zero |
| 150 | 1 | Response validity: bit 0 SID, bit 1 exact response time; other bits zero |
| 151 | 1 | Reserved, zero |
| 152 | 4 | Original source sequence, nonzero for PRESENT |
| 156 | 4 | Response source sequence, strictly greater than original sequence |
| 160 | 4 | Original observed low32 source time in milliseconds |
| 164 | 4 | Response observed low32 source time in milliseconds |
| 168 | 8 | Exact response S3 source uptime, zero if unavailable |
| 176 | 31 | Retained raw response bytes; remaining bytes zero |
| 207 | 1 | Reserved, zero |
| 208 | 1 | v4: receive source kind 0 UNKNOWN, 1 LEGACY callback, 2 EXTENDED callback |
| 209 | 1 | Receive properties; zero for UNKNOWN/LEGACY callback |
| 210 | 1 | Delivered callback length 0..255; zero for UNKNOWN |
| 211 | 1 | Source SID 0..15 or `0xff` unavailable for known metadata; zero for UNKNOWN |
| 212 | 1 | Primary PHY: 1=1M or 3=coded for extended; zero for UNKNOWN/LEGACY callback |
| 213 | 1 | Secondary PHY: 0=none, 1=1M, 2=2M or 3=coded for extended; zero for UNKNOWN/LEGACY callback |
| 214 | 1 | Controller status: 0=COMPLETE for extended, `0xff` unavailable for legacy callback; zero for UNKNOWN |
| 215 | 1 | Full delivered AD validation: 0 UNKNOWN, 1 VALID, 2 INVALID |

The encoder ignores unused legacy name/UUID/ADV tails and writes canonical zero padding. New response tails must already be zero, matching the admitted source's bounded copy. It does not display-sanitize either packet. The decoder rejects noncanonical padding, unknown receiver/RSSI kind or container validity bits, unsupported lengths/version, per-receiver overflow, duplicate `(receiver, address type, address)` keys, invalid receiver/time combinations, truncated files and trailing bytes. Original source index remains 0..31 for every receiver; it is not the mixed report's row number. Unknown **original advertisement** flags and event types remain valid evidence when the response state is UNKNOWN/NONE and the receive envelope is UNKNOWN; they are distinct from reserved **container** validity bits.

PRESENT requires an S3 receiver, valid nonzero scan epoch, an original event in 0..3, and original flags containing only TRUNCATED (which requires a full 31-byte retained prefix). The modular32 response-minus-original interval must be at most 2,000 ms. Retained response length equals `min(delivered_length, 31)`. Exact original time, when available, must match the original low32 field, and the response interval must fit within `ceiling - original` even if exact response time is unknown. That consistency check does not populate an unknown timestamp. Exact response time, when available, additionally requires exact original time and `original <= response <= ceiling <= saved`, the same full64 interval as the modular32 interval, and matching low32 response time. Genuine time zero is valid. The codec can represent PRESENT with explicitly unknown exact64 time; the current S3 capture mapper emits PRESENT only when both exact clocks are proved. These consistency checks do not authenticate a file's claimed radio provenance.

Known receive metadata requires a C5 receiver and the existing nonzero boot/epoch provenance. The shared validator requires original event 0..3, retained raw length `min(callback_length,31)`, raw flags limited to TRUNCATED/MALFORMED and TRUNCATED exactly when callback length exceeds 31. Known AD validation is VALID or INVALID. Legacy callbacks additionally require length at most 31, SID/status `0xff`, properties zero and primary/secondary PHY 0/0. Extended properties are limited to `0x1f`, exclude scan-response bit `0x08`, and use status COMPLETE, SID 0..15 or `0xff`, primary PHY 1 or 3 and secondary PHY 0..3. Extended reports carrying legacy PDUs require length at most 31, SID `0xff`, primary/secondary PHY 1/0 and these exact properties→original-event pairs: `0x10→3`, `0x12→2`, `0x13→0`, `0x15→1`. Other native extended reports normalize to event 0 when connectable, otherwise event 3. UNKNOWN requires every receive field zero and does not tighten old original-ADV evidence validation. These envelope checks do not reparse a truncated prefix as a complete delivered payload.

## Publication and validation

Decode validates the complete header, exact length, CRC, every entry, and all typed address duplicates before changing the destination document. Failure preserves the previous document in full. A successful decode clears unused model rows. Input must remain immutable during the operation, and input/output memory must not overlap. This protects the model from partial replacement; it does not by itself guarantee SD durability or make a filesystem rename power-loss atomic.

`tools/check_ble_archive.py` compiles the production codec and pure S3/C5 capture mappers. The codec fixture passes 6,616,148 assertions: unchanged independent v1/v2/v3 vectors including v3 PRESENT response bytes/time, independent v4 UNKNOWN/S3-response/C5-envelope vectors, full 32-row v1 and 64-row v2/v3/v4 corruption and truncation, CRC-correct hostile fields, canonical re-encoding of UNKNOWN/PRESENT/known-envelope rows, all receiver/RSSI-kind and receive-metadata byte domains, source collisions, per-source limits, response time bounds, envelope/raw context, immutable input and failed-decode atomicity. The S3 capture fixture retains its 2,523 assertions. The C5 fixture verifies frozen selected/publication receive metadata, raw/aggregate separation, historical age bounds and canonical UNKNOWN response fields. These are host checks, not physical SD or BLE reception acceptance.

The v4 host model is 15,376 bytes per 64-row document and 240 bytes per row, an increase of 512 bytes per document over v3; these are native host C type sizes, not wire sizes or new ESP32 measurements. The UI's retained/incoming documents and the worker's immutable copy remain bounded allocations. The storage and IO owners must use the 13,856-byte file limit and preserve ownership, cancellation, close/rename and immutable-copy guarantees separately from codec validation. Native frame sizes and peak runtime memory require separate validation.
