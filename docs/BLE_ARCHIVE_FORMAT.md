# Saved BLE evidence format

This guide describes the rev99 source. See [the rev99 checkpoint](DUAL_RADIO_REV99.md) for current build and installation evidence. The new dual-receiver saved-report workflow and the preceding live BLE snapshot changes still need physical acceptance. [CONTRIBUTION_REV93.md](CONTRIBUTION_REV93.md) retains the earlier contribution's scope and evidence.

Version 2 writes a bounded, passive report of one to 64 retained BLE observations, at most 32 per receiver. The new reader also accepts existing version 1 files, which contain at most 32 observations with no encoded receiver identity. The pure codec is in `s3_main/main/ble_archive.c` and `ble_archive.h`. It performs no allocation, filesystem operation, radio operation, or ESP-IDF call.

| Version | Header | Row | Maximum count | Maximum file |
| --- | ---: | ---: | ---: | ---: |
| 1, read compatibility | 32 bytes | 128 bytes | 32 | 4,128 bytes |
| 2, current writer/reader | 32 bytes | 144 bytes | 64, at most 32 per receiver | 9,248 bytes |

Files retain the `.BLE` extension. Version 1 readers may reject version 2 files; compatibility means that the new reader preserves support for old files, without silently rewriting them. Encoding a decoded version 1 model produces an explicit version 2 report with `LEGACY` receiver labels.

This format preserves retained evidence for offline inspection. Loading it does not create live scan results, connection targets, or permission to use an active feature. It is distinct from Marauder's JSON AirTag log format and does not import that format.

## Evidence and time limits

- An entry retains the receiver, typed BLE address, a projected received name, explicitly described RSSI, peer flags, manufacturer and UUID metadata, and up to 31 original advertisement bytes. Address bytes use NimBLE order, with the least significant octet first. Identity is `(receiver, address type, address)`: the same typed address heard by S3 and C5 is two observations, not evidence of two physical devices. `LEGACY` means the source was unspecified in version 1; decoding does not invent an S3 receiver label.
- The name is a bounded C-string projection from the peer result. It is **not** a lossless copy of the original over-the-air name: earlier peer conversion may have discarded bytes following an embedded NUL, and the C5 companion has already sanitized its display projection. The codec preserves the bytes it receives, including S3 control/non-ASCII bytes; it cannot restore an earlier name or infer it from unrelated raw data.
- Advertisement bytes, event type, and flag bits are preserved even when the retained advertisement is malformed, truncated, or unfamiliar. A malformed AD element does not invalidate the archive container. Offline parsers must check AD lengths independently and label truncation or malformed contents.
- Only the retained original advertisement bytes are saved. Raw scan-response bytes were not retained by this source and are absent from this format. Pointer-containing focus objects and their decoded scan-response hints are not serialized. Reanalysis of the retained bytes cannot reproduce a live classification that depended on a scan response or an older report.
- `S3` and `LEGACY` entries use original-report sweep-peak RSSI, which need not describe the particular retained ADV bytes. C5 entries use latest original-ADV RSSI. The name, manufacturer and UUID remain aggregate peer hints, not a claim that every value occurred in that raw packet. An unavailable RSSI is 127 with its validity bit clear, for either RSSI kind.
- `saved_uptime_ms` always describes the S3 save boot. For S3/LEGACY, valid exact source times satisfy `original <= ceiling <= saved`; zero is valid when explicitly marked valid. The copy ceiling is not a packet timestamp. Those receivers must have no C5 age bound or receiver boot ID.
- C5 rows require a nonzero committed C5 boot ID and valid nonzero source epoch. Exact S3 source/copy times are zero and invalid. An optional conservative **upper bound on age at save** is stored separately; it may exceed S3 saved uptime and is never subtracted to invent an exact packet timestamp. Unknown bounds have a clear validity bit and value 0. A valid bound of 0 or `UINT64_MAX` is distinguishable from unknown through that bit.
- `cerberus_ble_archive_age_at_save()` returns exact historical age for S3/LEGACY. `cerberus_ble_archive_age_upper_at_save()` returns the C5 historical upper bound. Both reject unavailable or invalid values without changing their output. Opening a file after reboot does not refresh either age; neither helper accepts the loading boot's clock. There is no UTC timestamp or claim of present freshness.
- Scan epoch, generation, and original row index are historical source metadata. They are not an active scanner ownership token, an address identity, or an authorization binding.

## Header

All multibyte integers are unsigned little-endian. The format is independent of native C structure size, alignment, padding, and pointers.

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 8 | Magic bytes `43 42 4C 45 41 52 43 00` (`CBLEARC` followed by NUL) |
| 8 | 2 | Version 1 or 2; encoder writes 2 |
| 10 | 2 | Header size, exactly 32 |
| 12 | 2 | Entry size 128 for v1, 144 for v2 |
| 14 | 2 | Entry count 1..32 for v1, 1..64 for v2; v2 also limits each receiver to 32 |
| 16 | 4 | Total file length, exactly `32 + entry_size * count` |
| 20 | 8 | Saved S3 uptime in milliseconds |
| 28 | 4 | CRC-32/ISO-HDLC |

The CRC uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. It covers the **entire header and all entries**, with bytes 28 through 31 treated as zero during calculation. This detects corruption; it does not authenticate the file or its claimed observations.

## Entry

Offsets below are relative to the beginning of each entry. Bytes 0..127 retain their existing fields, with the explicitly versioned additions noted below; v2 appends 16 bytes.

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
| 128 | 8 | v2 only: conservative age upper bound at save, zero if unavailable |
| 136 | 8 | v2 only: nonzero C5 receiver boot ID; zero for S3/LEGACY |

The encoder ignores unused tails of the input model and writes canonical zero padding. It does not display-sanitize the name or advertisement. The decoder rejects noncanonical padding, unknown receiver/RSSI kind or container validity bits, unsupported lengths/version, per-receiver overflow, duplicate `(receiver, address type, address)` keys, invalid receiver/time combinations, truncated files and trailing bytes. Original source index remains 0..31 for every receiver; it is not the mixed report's row number. Unknown **advertisement** flags and event types remain valid evidence; they are distinct from reserved **container** validity bits.

## Publication and validation

Decode validates the complete header, exact length, CRC, every entry, and all typed address duplicates before changing the destination document. Failure preserves the previous document in full. A successful decode clears unused model rows. Input must remain immutable during the operation, and input/output memory must not overlap. This protects the model from partial replacement; it does not by itself guarantee SD durability or make a filesystem rename power-loss atomic.

`tools/check_ble_archive.py` compiles the production codec and the pure C5 capture mapper. The codec fixture passes 2,724,148 assertions: an unchanged independent v1 golden vector, independent v2 LEGACY/C5 golden vectors, full 32-row v1 and 64-row v2 corruption, truncated files, CRC-correct hostile fields, canonical re-encoding, all receiver/RSSI-kind byte values, same-address observations on separate receivers, per-source 32-row limits, boundary timestamps/upper bounds, raw-byte preservation, immutable input, failed-decode atomicity and generated round trips. The mapper fixture checks copied full-width C5 provenance, raw/aggregate separation, unknown/backward/overflow timing and v2 round trips. These are host checks, not physical SD or BLE reception acceptance.

The host model is 10,768 bytes per 64-row document and 168 bytes per row; these are native host C type sizes, not wire sizes or ESP32 measurements. The UI's retained/incoming documents and the worker's immutable copy remain bounded allocations. The storage and IO owners must use the 9,248-byte file limit and preserve ownership, cancellation, close/rename and immutable-copy guarantees separately from codec validation.
