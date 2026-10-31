# Saved BLE evidence format

This guide describes the contributed source. New workflow acceptance on physical hardware remains pending; the installed local Unified checkpoint and the unflashed Unchained adaptation are distinguished in [CONTRIBUTION_REV93.md](CONTRIBUTION_REV93.md).

Version 1 stores a bounded, passive snapshot of one to 32 received BLE entries. The pure codec is in `s3_main/main/ble_archive.c` and `ble_archive.h`. It performs no allocation, filesystem operation, radio operation, or ESP-IDF call. The encoded file is at most **4,128 bytes**: a 32-byte header followed by 128 bytes per entry.

This format preserves retained evidence for offline inspection. Loading it does not create live scan results, connection targets, or permission to use an active feature. It is distinct from Marauder's JSON AirTag log format; version 1 does not import that format.

## Evidence and time limits

- An entry retains the typed BLE address, a projected received name, the sweep peak RSSI when available, peer flags, manufacturer and UUID metadata, and up to 31 original advertisement bytes. Address bytes use NimBLE order, with the least significant octet first. Address type is part of identity; a public address and a random address with the same six bytes are distinct entries.
- The name is a bounded C-string projection from the peer result. It is **not** a lossless copy of the original over-the-air name: earlier peer conversion may already have discarded bytes following an embedded NUL. Control and non-ASCII bytes in the retained projection are preserved; only the display should sanitize them.
- Advertisement bytes, event type, and flag bits are preserved even when the retained advertisement is malformed, truncated, or unfamiliar. A malformed AD element does not invalidate the archive container. Offline parsers must check AD lengths independently and label truncation or malformed contents.
- Only the retained original advertisement bytes are saved. Raw scan-response bytes were not retained by this source and are absent from this format. Pointer-containing focus objects and their decoded scan-response hints are not serialized. Reanalysis of the retained bytes cannot reproduce a live classification that depended on a scan response or an older report.
- The RSSI is the original-report sweep peak. It is not necessarily the RSSI of the particular advertisement bytes retained in the entry. The name, manufacturer, and UUID are peer metadata, not a claim that every value occurred in that raw packet.
- `saved_uptime_ms`, the original source uptime, and its copy ceiling describe the original S3 boot. A valid source time must satisfy `original <= ceiling <= saved`. Zero is a valid time when explicitly marked valid. The ceiling is the time of the bounded snapshot copy, not a packet reception time. Missing source time is explicit.
- The only age helper returns **age at save**, `saved_uptime_ms - original_source_uptime_ms`. Opening the file after reboot does not change that age and must not compare these timestamps with the new boot's clock. There is no UTC timestamp or claim of present freshness.
- Scan epoch, generation, and original row index are historical source metadata. They are not an active scanner ownership token, an address identity, or an authorization binding.

## Header

All multibyte integers are unsigned little-endian. The format is independent of native C structure size, alignment, padding, and pointers.

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 8 | Magic bytes `43 42 4C 45 41 52 43 00` (`CBLEARC` followed by NUL) |
| 8 | 2 | Version, exactly 1 |
| 10 | 2 | Header size, exactly 32 |
| 12 | 2 | Entry size, exactly 128 |
| 14 | 2 | Entry count, 1 through 32 |
| 16 | 4 | Total file length, exactly `32 + 128 * count` |
| 20 | 8 | Saved S3 uptime in milliseconds |
| 28 | 4 | CRC-32/ISO-HDLC |

The CRC uses reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. It covers the **entire header and all entries**, with bytes 28 through 31 treated as zero during calculation. This detects corruption; it does not authenticate the file or its claimed observations.

## Entry

Offsets below are relative to the beginning of each 128-byte entry.

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 6 | Address in native NimBLE byte order |
| 6 | 1 | Address type, 0 through 3 |
| 7 | 1 | Validity bits: bit 0 RSSI; bit 1 source times; bit 2 scan epoch; other bits zero |
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
| 19 | 1 | Reserved, zero |
| 20 | 2 | Delivered original advertising report count |
| 22 | 2 | Reserved, zero |
| 24 | 8 | Scan epoch; nonzero if valid, otherwise zero |
| 32 | 8 | Original source uptime in milliseconds; zero if unavailable |
| 40 | 8 | Copy ceiling uptime in milliseconds; zero if unavailable |
| 48 | 32 | Name projection; no NUL inside its declared length; remaining bytes zero |
| 80 | 16 | UUID bytes; remaining bytes zero |
| 96 | 31 | Retained original advertisement bytes; remaining bytes zero |
| 127 | 1 | Reserved, zero |

The encoder ignores unused tails of the input model and writes canonical zero padding. It does not display-sanitize the name or advertisement. The decoder rejects noncanonical padding, unknown container validity bits, unsupported lengths or version, duplicate `(address, address_type)` pairs, invalid timing relationships, truncated files, and trailing bytes. Unknown **advertisement** flags and event types remain valid evidence; they are distinct from reserved **container** validity bits.

## Publication and validation

Decode validates the complete header, exact length, CRC, every entry, and all typed address duplicates before changing the destination document. Failure preserves the previous document in full. A successful decode clears unused model rows. Input must remain immutable during the operation, and input/output memory must not overlap. This protects the model from partial replacement; it does not by itself guarantee SD durability or make a filesystem rename power-loss atomic.

`tools/check_ble_archive.py` compiles the production codec without SDK or radio stubs. The focused fixture checks an independent Python `zlib` golden vector, all truncated lengths, corruption at every byte of a full 32-entry archive, CRC-correct hostile fields, canonical re-encoding, duplicate typed identities, boundary timestamps and historical ages, raw byte preservation, immutable input, failed-decode atomicity, and generated round trips. These are host checks, not physical SD or BLE reception acceptance.
