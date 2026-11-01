# Remaining receive and device-workflow gaps

This source audit used Cerberus `41b65fdec1f2dc1ccdb01d73e56887dee5d9194e`
and the clean pinned Marauder develop checkout
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363`. A read-only GitHub compare at
2026-11-01 16:21:17 UTC reported develop identical to that commit: zero ahead,
behind or intervening commits. The response is retained at
`artifacts/device-rev117/marauder-develop-check.json`. This is a point-in-time
source check, not proof of complete porting or physical acceptance.

## Receiver ownership

- **Fox Hunt:** the rev116 tracker accepted matching-target callbacks from any
  current generic scanner epoch. Generic Start was permitted while the tracker
  remained enabled, and UI Stop checked host epoch plus the public generation
  byte. The local rev117 candidate implements exact TRACK
  attachment/start/state/stop/release binding. Physical acceptance remains
  pending. See [the milestone](DUAL_RADIO_REV117.md).
- **Recon:** `sentinel_scan_session.c` has a remote-command ambiguity path that
  can accept a later RUNNING observation after a failed START. The S3 local
  call has a definite admission result: `ESP_ERR_INVALID_STATE` must not grant
  ownership of an already-running foreign scanner. The local rev118 candidate
  adds an explicit local-rejection acknowledgement and exact passive lease
  binding across S3 acquisition, completed row/Watch evidence and cleanup.
  It retains C5's separate transport scheduler. See
  [the rev118 milestone](DUAL_RADIO_REV118.md) for validation and remaining
  physical acceptance; the pure parallel scheduler fixture alone does not
  prove the native boundary. Whole-device/C5 ownership parity remains a
  separate audit, not a claim made by this S3 change.
- **Wardriving:** the local rev119 candidate replaces generation-byte S3
  ownership with the exact passive attachment, whole-bank capture and retained
  cleanup. See [the milestone](DUAL_RADIO_REV119.md) for validation and physical
  acceptance limits. Its worker still deliberately ends both receivers on a
  terminal source error. A deliberate degraded-mode recording/retry design
  remains open; partial files and single-source periods need explicit reporting
  if that policy changes. C5 ownership and expansion release remain separate
  acceptance/audit work.
  The scoped S3 API has a bounded mutex wait. A future retry policy must
  distinguish a returned status/copy timeout from an accepted scan's reported
  controller error and from an SD append timeout; rev119 still treats those
  returned S3 call failures as terminal for the recording session.
- **Shared BLE host:** local rev120 replaces owner-only release with a nonreused
  token, epoch and owner checked together under a short critical lock. The
  scanner, central, advertiser, Radio Lab and first-use Wardriving initializer
  use the new lease. Pending initialization remains reserved across reset, and
  readiness is distinct from retained ownership. See
  [the milestone](DUAL_RADIO_REV120.md) for source/build evidence and limits.
  A valid-lease check followed by an SDK operation is still not one atomic
  operation with host reset; broader SDK command serialization and physical
  concurrent-reset acceptance remain open. This does not establish that every
  host client or C5 ownership path is race-free.
- **MAC Monitor:** its context/job/epoch guards already protect callback,
  start, stop and release paths. This audit found no equivalent reachable
  takeover defect; that is not a claim of complete physical acceptance.

## GPS Data and NMEA presentation

Pinned Marauder exposes GPS Data and NMEA Stream in `MenuFunctions.cpp` and
`CommandLine.cpp`. `RunGPSInfo()` and `RunGPSNmea()` live in `WiFiScan.cpp`.
The latter combines queued receiver sentences with generated GGA/RMC lines;
it is not uniformly a raw UART stream.

Cerberus already has the optional `wardrive_gps_uart.c` owner,
`wardrive_gps_core.c` parser and GPS-aware main-firmware logging. It currently
shows a compact summary and discards complete input lines after parsing.
A future DATA/NMEA details page can share the existing Wardriving service
without adding another UART owner or configuration route. Opening the page
must not implicitly start GPS, radios or logging.

Before richer presentation, distinguish unknown and stale data:

- GGA satellites and HDOP need their own validity/age; empty fields currently
  become zero, while a later RMC can refresh location without refreshing GGA.
  HDOP must not be labelled accuracy in metres.
- The uninitialized UART status path leaves UTC age at zero; other absent/OFF
  paths use `UINT32_MAX`. A zero must not be displayed as fresh evidence.
- No recent checksum-valid input does not establish physical hardware absence.
- Parser fix freshness and WiGLE row eligibility use different age limits.
  Receiver status is not proof that a particular observation may be geotagged.

A bounded optional trace should preserve complete input bytes, sequence,
receipt time and parser outcome without per-line allocations. Invalid/overflow
and discarded-history counts should be explicit. Raw coordinate-bearing lines
need whole-line masking under privacy settings. UART receipt time is neither
GNSS fix time nor synchronized radio capture time. Standalone GPS acquisition,
PPS, POI, geofencing and tracker controls require separate work. No GPS module
or six-C5 backboard has been physically accepted.

## BLE persistence and external analysis

Pinned `WiFiScan.cpp:4028–4131` saves/loads an AirTag JSON array with `mac`,
hexadecimal `payload` and `payload_size`. The file omits address type,
receiver, source epoch/time and ADV/RSP provenance. An importer must not invent
those fields. Existing-MAC sightings update upstream RSSI/time without replacing
the stored payload. This is retained-list persistence, not a callback timeline.

The inspected ordinary `RunBluetoothScan()` path does not open PCAP or a log.
The shared CLI serial option and `Buffer.cpp` logging support alone do not prove
a generic BLE PCAP feature. Cerberus's v6 `.BLE` snapshots retain richer source
evidence but have at most 64 observations, 32 per receiver, and 31-byte raw
previews. Decoded evidence beyond a preview is not a lossless retained payload.

Marauder JSON compatibility, broader history and complete raw payload retention
remain gaps. A separate human-readable export of a validated `.BLE` document
would help external analysis but would not, by itself, implement Marauder's
load-to-live-list workflow. It should use the existing worker and bounded
streaming scratch, preserve unknowns and lossless 64-bit identities, and never
replace historical evidence with a fresh clock or radio lookup. A true timeline
needs its own journal semantics rather than repeated rows in a v6 snapshot.

## SHOW_INFO source review

Pinned `RunInfo()` (`WiFiScan.cpp:4944`) presents device version, board/FCC
label, application/IDF, interface identities, SD/battery and network status.
Cerberus already exposes related information in its dashboard/System health,
SD tools and read-only USB console (`ui.c`, `system_health.c`, `usb_console.c`).
These are existing partial implementations, so SHOW_INFO is classified partial
after this source review, not complete.

A consolidated navigable display, running partition and fuller option mapping
remain open. The S3 application version and C5 numeric capability revision are
different facts: the latter cannot prove an exact matched build pair. Board/FCC
identifiers must come from actual board information. This review adds no new
device-information UI and no hardware acceptance.
