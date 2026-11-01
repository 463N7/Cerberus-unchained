# Rev106: independent BLE activity from both receivers

This checkpoint adds a receive-only C5 Analyzer path beside the existing S3
Analyzer. All 123 host suites and five native targets pass on the final source.
Rev103 remains the last installed checkpoint while the matched rev106 package
is prepared; rev104 and rev105 are verified local packages.

## Upstream behavior and processor roles

The pinned Marauder develop source at
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363` exposes Bluetooth Analyzer in
`MenuFunctions.cpp:5515`. Its callback at `WiFiScan.cpp:1584` increments an
activity counter and periodically updates a displayed name/RSSI. The graph
uses callback counts per configured banner interval. These facts were checked
against the local clean pinned source; they do not establish RF equivalence.

Cerberus's S3 Analyzer retains its 64 completed 200 ms callback buckets,
latest-report information and independent family evidence. C5 contributes
passive original advertising callback counts, including partial extended
reports, before inventory capacity. Scan responses are excluded on C5. These
are receiver-specific controller callbacks, not unique devices or all
over-the-air packets. The two totals must not be summed or deduplicated.

## Compact C5 measurement contract

Dedicated Analyzer mode uses the existing dual BLE owner, finite eight-second
RF sweeps and exact Stop/Release binding. It avoids inventory retention and
row downloads for activity display. Firmware 65 gates both the new mode's
START and its sample command; older peers cannot be silently started in an
unsupported mode.

One 22-byte sample carries full epoch, job ID, cumulative callback count,
C5-local elapsed milliseconds, validity/state flags and last callback RSSI.
The request supplies the existing full boot/job binding. Fields are sampled
atomically under the scanner mutex. Timing starts around the actual SDK scan
start and freezes at completion. Invalid or backward clocks cannot create a
valid rate. Saturating counters remain lower bounds, not exact interval counts.
RSSI has no peer identity or age in this frame and cannot establish freshness.

S3 derives C5 rates only from consecutive valid count/time differences in one
boot/job/epoch. Zero-duration, rewound, reset, saturated and interrupted sample
intervals cannot become invented zero-rate measurements. C5 windows retain
their own elapsed duration and are not inserted into S3's 200 ms buckets.
Polling uses the existing bounded transaction scheduler; drawing reads cached
state and performs no SPI work.

## Ownership and acceptance

Analyzer entry, retry, Stop, Back and sleep must respect both existing owners.
Unavailable C5 telemetry is distinct from a valid zero count; the existing S3
view remains useful independently. Cleanup must continue after timeout and
must not rearm a receiver while a previous owner is still releasing.

The default BOTH RECEIVERS page presents the S3 completed 200 ms buckets and
C5 source intervals separately. S3 DETAILS retains the existing family and
advertisement/response evidence. A transport error hides current C5 count/rate
while retained source intervals remain explicitly labelled. Stop, Back and
sleep continue cleanup after a release timeout; they cannot silently abandon
the old owner or restart it while exiting.

## Final local validation

- All 123 host suites pass, with retained per-suite logs and source hashes.
- All five native targets pass: S3, onboard C5, bridge, expansion WROOM and
  expansion XIAO. The first native run caught a missing UI forward declaration;
  that failure is retained, the declaration is fixed, and both complete host
  and native runs were repeated on the corrected source.
- Actual C5 producer/controller integration covers 75 callbacks across more
  than 32 identities, partial extended reports, response/anonymous exclusions,
  parser/inventory bypass, true zero intervals, final sweeps and new ownership.
  Additional checks cover stale bindings, failed starts, counter saturation,
  same-millisecond growth, clock faults, old firmware and release fences.
- Four actual-renderer fixtures were inspected: independent receivers,
  saturation, unsupported C5 and release timeout. These are synthetic UI
  previews, not photographs or physical touchscreen acceptance. The final host
  run reproduced the reviewed images byte for byte.

Native measurements compare final rev106 ELFs with retained, unflashed rev105:

| Measurement | Rev105 | Rev106 | Change |
| --- | ---: | ---: | ---: |
| S3 companion state in the existing PSRAM allocation | 8,072 B | 8,496 B | +424 B |
| S3 Analyzer state | 288 B | 288 B | 0 |
| S3 live-discovery store | 20,560 B | 20,560 B | 0 |
| S3 internal data + BSS | 172,572 B | 172,588 B | +16 B |
| C5 data + BSS | 80,032 B | 80,080 B | +48 B |
| S3 dual-controller entry frame | 176 B | 192 B | +16 B |

The new dual-view drawing entry frame is 864 bytes. The UI main entry frame
remains 1,136 bytes and its task allocation remains 8,192 bytes. These are
selected native frames, not a bound on an entire nested call chain. Inlined
helpers are not assigned zero cost. Type sizes exclude allocator overhead and
fragmentation; neither static section sizes nor host checks establish runtime
headroom, sustained stability or RF performance.

Evidence is retained under `artifacts/rev106-host-evidence`,
`artifacts/rev106-native-evidence` and `artifacts/device-rev106`. No installation,
physical dual reception, rate calibration, extended/coded reception, sustained
headroom, Stop/Back or next-scan acceptance is claimed at this preparation
stage. Full Marauder parity, GPS and expansion hardware acceptance remain open.
