# Remaining receive and device-workflow gaps

This source audit used Cerberus `41b65fdec1f2dc1ccdb01d73e56887dee5d9194e`
and the clean pinned Marauder develop checkout
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363`. A read-only GitHub compare at
2026-11-01 16:21:17 UTC reported develop identical to that commit: zero ahead,
behind or intervening commits. The response is retained at
`artifacts/device-rev117/marauder-develop-check.json`. This is a point-in-time
source check, not proof of complete porting or physical acceptance.

The source milestones described below are now included in installed
[rev124/C5 firmware69](DUAL_RADIO_REV124.md). Rev124 adds native saved Wi-Fi
observations and the Recon field dashboard. Rev125 implements separate offline
AP JSON/SSID text interchange; its full validation and installation status are
tracked in [the rev125 checkpoint](DUAL_RADIO_REV125.md). Verified image writes, paired
application boot, display buffering and SD mount establish the installed
baseline; physical feature acceptance remains separate. The earlier milestone
documents retain their original source and installation history.

## Receiver ownership

- **Fox Hunt:** the rev116 tracker accepted matching-target callbacks from any
  current generic scanner epoch. Generic Start was permitted while the tracker
  remained enabled, and UI Stop checked host epoch plus the public generation
  byte. Rev117 implements exact TRACK
  attachment/start/state/stop/release binding. Physical acceptance remains
  pending. See [the milestone](DUAL_RADIO_REV117.md).
- **Recon:** `sentinel_scan_session.c` has a remote-command ambiguity path that
  can accept a later RUNNING observation after a failed START. The S3 local
  call has a definite admission result: `ESP_ERR_INVALID_STATE` must not grant
  ownership of an already-running foreign scanner. Rev118
  adds an explicit local-rejection acknowledgement and exact passive lease
  binding across S3 acquisition, completed row/Watch evidence and cleanup.
  It retains C5's separate transport scheduler. See
  [the rev118 milestone](DUAL_RADIO_REV118.md) for validation and remaining
  physical acceptance; the pure parallel scheduler fixture alone does not
  prove the native boundary. Whole-device/C5 ownership parity remains a
  separate audit, not a claim made by this S3 change.
- **Wardriving:** rev119 replaces generation-byte S3
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
- **Shared BLE host:** rev120 replaces owner-only release with a nonreused
  token, epoch and owner checked together under a short critical lock. The
  scanner, central, advertiser, Radio Lab and first-use Wardriving initializer
  use the new lease. Pending initialization remains reserved across reset, and
  readiness is distinct from retained ownership. See
  [the milestone](DUAL_RADIO_REV120.md) for source/build evidence and limits.
  A valid-lease check followed by an SDK operation is still not one atomic
  operation with host reset; broader SDK command serialization and physical
  concurrent-reset acceptance remain open. This does not establish that every
  host client or C5 ownership path is race-free.
- **C5 receive control and Channel Survey:** a subsequent rev120 source audit
  reproduced stale queued Stop/timeout publication into replacement scans,
  false retained ownership after successful timer-failure cancellation, and
  legacy failed-cancel admission of Wi-Fi while discovery remained active.
  Channel Survey pending/running and retained cleanup was invisible to BLE
  admission, with no UI retry after ERROR. Rev121 implements these
  corrections, with focused production-bound regressions and independent
  source review complete. All 128 host suites, five native builds and matched
  package verification pass; see [the milestone](DUAL_RADIO_REV121.md). The current Survey UI
  requires C5 firmware 69's checked cleanup acknowledgement, while its wire
  commands retain their original firmware 46 layout.
  Existing survey generation8/band and unbound Stop do not prove full
  cross-reboot or generation-reuse safety. Whole-device and physical acceptance
  remain open; a single SPI command task is not proof of serialization with
  asynchronous NimBLE callbacks or the survey worker.
- **MAC Monitor:** its context/job/epoch guards already protect callback,
  start, stop and release paths. This audit found no equivalent reachable
  takeover defect; that is not a claim of complete physical acceptance.

## GPS Data and NMEA presentation

Pinned Marauder exposes GPS Data and NMEA Stream in `MenuFunctions.cpp` and
`CommandLine.cpp`. `RunGPSInfo()` and `RunGPSNmea()` live in `WiFiScan.cpp`.
The latter combines queued receiver sentences with generated GGA/RMC lines;
it is not uniformly a raw UART stream.

Rev122 implements GPS DATA and received-NMEA subviews through the existing
optional `wardrive_gps_uart.c` owner. Its source work adds independent GGA
metadata freshness, strict decimal parsing, bounded optional raw-line retention
and retained cleanup after UART failures. Opening these views does not start
GPS, radios or logging; the existing Wardriving GPS switch remains the single
configuration route. Implementation, focused checks and independent reviews
are complete. All 130 host suites, five native builds, the linked resource
audit and matched package verification pass. The audit measures 128 additional
S3 fixed data/BSS bytes and 2,624 optional PSRAM bytes for the trace/UI copy
before allocator overhead; C5 fixed data/BSS is unchanged. These are not
runtime heap, stack, latency or receiver-loss guarantees. See
[the milestone](DUAL_RADIO_REV122.md) and
[adapter semantics](WARDRIVE_GPS_ADAPTER.md).

Receipt time is UART read completion, not GNSS fix time or synchronized radio
capture time. Internal ABSENT means no recent checksum-valid input, not proof
of missing hardware. Parser freshness and WiGLE row eligibility remain different
limits. The trace reports parser overflow and history eviction, not loss before
the UART reader. Full-byte retention does not establish valid NMEA. Location
visibility defaults hidden and omits entire raw payloads, including unknown or
malformed lines; it is separate from address-display preferences.

Paging preserves a stable copy and identifies receipt ages as ages at copy.
Hiding a paused copy erases its raw bytes; LIVE must obtain a new copy before
those bytes can be shown again. Sequence exhaustion stops trace publication
without stopping the parser, and a new receiver epoch clears the ring. Exact
observer End retains its token on timeout and retries at a 250 ms UI cadence;
observer mutex attempts are bounded at 25 ms. These are wait/pacing policies,
not measured response times or guaranteed cleanup deadlines. UART-driver
cleanup remains with the existing Wardriving worker and holds STOPPING until
released; no new UART owner is introduced.

Standalone acquisition, upstream generated-sentence/serial behavior, richer
receiver metadata, PPS, POI, geofencing and tracker controls remain open. No GPS
module or six-C5 backboard has been physically accepted. The rev122 source work
is included in installed rev123; paired boot is not evidence of live receiver
or geotagged-file acceptance. GPS/NMEA parity remains partial.

## BLE persistence and external analysis

Pinned `RunSaveATList()` and `RunLoadATList()` in `WiFiScan.cpp:4028-4131`
save/load an AirTag root JSON array with `mac`, hexadecimal `payload` and
`payload_size`; `byteArrayToHexString()` and `hexStringToByteArray()` are in
pinned `esp32_marauder/utils.h` (lines 91 and 111).
The file omits address type, receiver, source epoch/time, RSSI and ADV/RSP
provenance. Existing-MAC sightings update upstream RSSI/time without replacing
the retained payload. This is retained-list persistence, not a callback timeline.
The read-only comparison at 2026-11-01 19:45:27 UTC again reported develop
identical to `79cc6bcba1db6679baee1dab2b0c1ad5010e6363`; its response is retained
in `artifacts/device-rev123/marauder-develop-check.json`.

[Rev123](DUAL_RADIO_REV123.md) implements this schema as a separate offline
import/view/save-copy model and exports qualifying complete packets from
validated native `.BLE` documents. It preserves duplicate MAC records, order,
empty payloads and opaque bytes without inventing absent provenance or inserting
imported records into live discovery. Saved originals and matched responses
are considered independently; omitted incomplete/unproven and nonmatching
packets are counted separately. A full decoded hint is not a substitute for
missing raw bytes. More than 64 eligible packets fails atomically.

The explicit Cerberus bounds are 64 records, 8,192 aggregate payload bytes and
49,152 JSON bytes. One optional JSON viewer and the existing serialized worker
use the selected allocation arm; ordinary native `.BLE` allocation is unchanged.
Codec, IO/storage and UI focused checks and independent reviews pass. The first
complete 132-suite host run passed on 1,096 unchanged inputs. All five native
builds, the linked resource audit and matched package verification pass. The
local matched package contains three targets and nine images. Native S3 fixed
data/BSS grows by 32 bytes, C5 is unchanged, and the optional JSON viewer plus worker requests 68,200 bytes before allocator
overhead and other stores. Retaining the native viewer during export raises
that allocation sum to 129,312 bytes; no runtime headroom or SD-latency claim
follows from these sizes. SD save/reopen, interrupted-write recovery and physical
UI acceptance remain open despite verified paired boot and SD mount on rev123.

The inspected ordinary upstream `RunBluetoothScan()` path does not open PCAP
or a log. The shared CLI serial option and `Buffer.cpp` logging support alone
do not prove a generic BLE PCAP feature. Cerberus v6 snapshots retain richer
source evidence but at most 64 observations, 32 per receiver, and 31-byte raw
previews. Decoded evidence beyond a preview is not a lossless retained payload.

Remaining work includes an explicitly designed load-to-live-list workflow,
continuous callback/history journaling, full original and response payload
retention, and richer external exports that preserve native source/type/time
proof. Current JSON intentionally loses that metadata; it cannot carry full
Cerberus identity or replace the original `.BLE` evidence. Repeated snapshot
rows are not a callback timeline. Rev124 adds native AP/SSID report persistence,
and rev125 implements separate AP JSON/SSID text interchange. Manual editing,
load-to-live-list semantics and broader backup/restore remain separate. Offline
schema compatibility alone does not establish
full Marauder persistence parity or hardware acceptance.

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
