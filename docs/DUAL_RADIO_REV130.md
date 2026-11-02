# Rev130: passive Mesh Detection foundation

This development slice adds an original, bounded IEEE 802.15.4 observation
pipeline for the onboard S3/C5 pair. It builds on the receive continuity fix in
[rev129](DUAL_RADIO_REV129.md). It needs no GPS or expansion board. The wider
[Mesh Detection plan](MESH_DETECTION_PLAN.md), including protocol discovery,
selected-address location and historical map reopening, remains unfinished.

## Processor roles and ownership

C5 owns passive IEEE 802.15.4 reception on channels 11–26, fixed-channel or
selected-mask sweeping, callback-buffer lifetime and radio cleanup. It reuses
the existing 4,096-byte worker. The 16-slot copied-frame FIFO and pinned summary
are optional internal-heap allocations, released after cleanup. There is no
second C5 radio task. A valid active STATUS renews a ten-second lease; lease
expiry stops reception and records a timeout. Stopped evidence has a separate
30-second retention period, checked before renewal.

S3 owns bounded header parsing, PAN/address/traffic aggregation, touch selection,
rendering and asynchronous SD reports. Its model/controller and UI storage use
optional PSRAM, with no internal-RAM fallback. C5 is reserved for IEEE 802.15.4
while this service is held; it cannot simultaneously scan Wi-Fi/BLE on its one
RF path. S3 complementary BLE/Wi-Fi observations are a later integration step.

The C5 capability revision is 72. Commands 146–153 use full 64-bit sessions and
distinct, nonwrapping row and summary identities. A row stays immutable until
its exact ACK. Repeated ACKs cannot retire the next row. An uncertain START is
resolved by STATUS, never by blindly issuing another START. Back/hidden-page
cleanup closes the exact owned session rather than stopping a foreign tool.

The S3 controller budgets four logical wire calls per tick and checks an
eight-millisecond elapsed budget before starting another call. INFO and START
cost two calls because they refresh capability. This is cooperative scheduling,
not a hard SDK latency guarantee. A 139-byte record needs 16 nine-byte READs,
plus PEEK and ACK; high-rate traffic can overflow the 16-slot queue. Queue drops,
invalid callbacks and S3 retention omissions are separate evidence.

## What the observations mean

The independent parser supports bounded clear IEEE 802.15.4 MAC headers with
typed short and extended addresses, explicit/compressed/unknown PAN provenance,
security/header-IE bounds and opaque encrypted payloads. Full EUI-64 values are
retained. See [parser scope and references](MESH_MAC_PARSER.md).

The stable S3 store retains up to 16 PAN contexts, 64 addresses, 64 directed
traffic edges and 64 complete raw witnesses. It does not evict an older identity
when a new one appears. It keeps omission counters and explicitly marks an
unavailable latest witness after the raw store fills. Aggregated counters can
continue after the first 64 raw witnesses.

- A PAN context is a channel/value bucket within one session, not proof that
  every address in it belongs to one network. Unknown PAN is distinct from PAN 0.
- An edge means a clear MAC header addressed traffic from one endpoint to
  another. It does not prove delivery, routing, adjacency or parent/child roles.
- RSSI/LQI belong to a received source. An address seen only as a destination
  has no measured signal. Broadcast/sentinel addresses and addressless ACKs do
  not become invented peer links.
- Timestamps are C5 callback uptime, not UTC, synchronized S3 time, or precise
  RF arrival times. Retry suppression applies only to transport delivery IDs;
  repeated over-the-air MAC sequence numbers remain observations.
- SDK metadata replaces the FCS positions. Retained MAC bytes omit both the SDK
  length byte and RSSI/LQI footer; they are not an RF frame with verified FCS.

This foundation does not classify every encrypted Zigbee/Thread frame, discover
commissioned Matter fabrics, authenticate devices, join networks, or extract
authoritative routing tables. Matter commissioning BLE hints would be separate
evidence, not proof of Thread membership. LoRa reception needs additional radio
hardware.

## Interface and reports

The integrated entry is Scan → Mesh Detection, with PAN, Addresses and Traffic
views, fixed/sweep settings, frozen row details and explicit Stop/Back. A new
view defaults to sweeping channels 11–26 with a 250 ms dwell.
Existing Energy, Beacon, Activity and Compare tools remain reachable from the
same workflow. The focused UI/controller checks and software render previews
pass; do not infer availability on the installed device. STOP & SAVE requests
the final report while acquiring; SAVE REPORT saves an already frozen scan.
OPEN CSV hands a saved file to the existing paged text viewer. That viewer uses
the existing bounded file-read path, rather than the archive save worker.

SAVE REPORT uses the existing serialized archive worker. It copies a fully
stopped, drained, final-summary-fenced and released scan before returning to the
UI. Formatting and filesystem calls run in the worker. Reports are versioned
CSV files under `/CERBERUS/LOGS/M00001.CSV` and subsequent unused names, with
settings, source counters, channel dwell, S3 omissions, PANs, addresses, directed
observations and retained raw witnesses. Binary evidence is hex, with no
untrusted text interpreted as CSV formulas. Reports preserve full addresses;
screen privacy masking is not an export anonymizer.

The report buffer is capped at 98,304 bytes plus a terminal NUL in PSRAM. Existing
archive formats keep their exact-sized allocations. The SD writer reserves an
unused temporary name, checks write/flush/sync/close and publishes by rename.
Failure retains the created `.TMP` and its name, preserving older files.
Cancellation before storage skips the save; an in-progress synchronous save
reports its actual result. Successful rename is not proof of power-loss safety.
CSV export does not imply native map reopening or raw PCAP support.

## Validation status

Focused parser, model, link, controller, C5 worker, actual C5-to-S3 bridge,
report/worker and UI fixtures have passed on the host. The bridge exposed and
verified a cleanup fix: an SDK receive failure must clear the terminal channel
as well as close callback admission, allowing the S3 final-state fence to retain
prior evidence with the original radio error. Failed ACK operations return no
success sequence; successful ACKs still echo the exact retired row.

The complete 146-suite registry and native matrix are being validated. Host
allocation sizes are measurements of the host ABI, not target-heap or stack
high-water evidence. The S3 UI workspace is 22,560 host bytes; the selected Mesh
report worker arm is 119,776, including its 98,305-byte formatter buffer. Existing
archive operations retain their earlier allocation sizes. The optional C5
context is 2,624 host bytes, with 144-byte slots. Native measurements follow.

Rev126 remains the last verified installation. Rev130 has not been flashed,
accepted with real Zigbee/Thread traffic, or published as a firmware release.
The existing public PR still describes the separately validated rev128 work.
Physical acceptance must check actual reception, Stop/Back, SD reports and
repeatable handoffs to Wi-Fi and Bluetooth.

No CYM application code was copied. Its pinned workflows informed the plan;
the existing Cerberus and third-party notices remain in place. This slice does
not close the larger Marauder parity ledger.
