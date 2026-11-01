# Adaptive two-radio scanning

This rev94 development work follows the rev93 contribution on a separate
branch. It requires the matched S3 rev94 application and onboard C5 protocol
firmware 61. The implementation below is not a measured performance claim;
physical acceptance remains separate from host checks and native builds.

## Using the views

Open the ordinary Wi-Fi scan from the launcher, Wi-Fi menu or lesson. When
both radios are free, the view shows `S3: 2.4 GHz + C5: 5 GHz`. It retains up to
32 rows from each receiver (64 total), supports band filters and sorting, and
keeps a previous completed bank visible while a replacement runs. Stop and
Back remain available during scan and result transfer. A clean refusal by S3
before starting uses the existing C5 all-band scan with an explicit fallback
label; uncertain cleanup is an error, not permission to steal the radio.

Tap a row for a frozen result. `OPEN AP TOOLS` performs a fresh C5 all-band
scan and checks the chosen network before entering the established AP tools.
A disappeared or changed AP reports failure. `BACK TO SCAN` returns to the
retained dual-radio list without starting another scan. The existing multi-AP
selection path remains separate from this single-AP handoff.

In ordinary BLE discovery, tap the receiver line below the filter/header to
open the C5 companion. S3 discovery continues while that pane is displayed.
The C5 pane shows its own 32-row bank, typed addresses, latest original-ADV
RSSI, timing and a bounded 31-byte advertisement preview. Its selected row
can enter `FOX THIS ADDRESS`; S3 rows retain their existing Fox Hunt route.
Fox Hunt displays separate S3 and C5 signals. Start/Stop on the companion
controls both receiver sessions. Counts remain per receiver rather than being
presented as a deduplicated count of physical devices.

## Implementation boundaries

The S3 Wi-Fi worker is temporary and uses an exclusive Wi-Fi owner plus BLE
admission guards. Its normalized result bank and SDK readout buffer use
PSRAM. The Wi-Fi view uses 5,648 bytes of optional PSRAM and avoids repeatedly
repainting an unchanged completed page. The optional BLE companion store
uses 7,336 bytes of PSRAM and adds no worker task. These sizes describe the
host-compiled model structures; live driver heap and stack headroom require
device measurements.

C5 Wi-Fi leases use a C5-issued 64-bit epoch; BLE uses a boot identity,
C5-issued monotonic job ID and a full-width session epoch. Stale commands do
not acquire a newer session, and byte-sized legacy generations are not used
as the sole identity fence. Bounded SPI steps keep bulk result retrieval out
of a single synchronous UI call. Software deadlines do not guarantee an SDK
transaction can be interrupted at the exact deadline.

## Radio assignments

| Activity | ESP32-S3 | ESP32-C5 |
| --- | --- | --- |
| Wi-Fi-only scan, both receivers available | Passive 2.4 GHz scan | Passive 5 GHz scan |
| Wi-Fi scan while S3 is reserved | Existing S3 activity continues | Both Wi-Fi bands, with degraded-mode status |
| Mixed Wi-Fi/BLE Recon or Wardriving | BLE | Both Wi-Fi bands |
| BLE-only discovery | Existing BLE discovery | Complementary passive BLE observation |
| Selected-device BLE fox hunt | Local typed-address RSSI measurements | Independent typed-address RSSI measurements |

An assignment never authorizes taking a radio from a portal, connection,
capture, monitor or another owning session. A rejected or uncertain start
cannot be interpreted as an owned scan. Stop applies only to the session
which accepted that start. Failed cleanup retains ownership and an explicit
error until the underlying resource is known to be released.

The C5 cannot scan both Wi-Fi bands simultaneously by itself. Separating the
bands across two chips can reduce sweep time when both are free. Espressif's
[S3 coexistence guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/coexist.html)
and [C5 coexistence guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/api-guides/coexist.html)
describe the shared radio and time arbitration within each chip. Running every
protocol on both chips continuously is therefore not the default policy.

## Identity and evidence

Wi-Fi rows retain their receiver provenance. An S3 observation has no C5 scan
index or generation. Before passing a chosen row to existing C5-dependent AP
tools, acquire and verify a new C5 snapshot and resolve the exact chosen
BSSID, channel, security and SSID within that snapshot. Never reuse a position
from a merged or sorted display as a C5 command index.

The S3 Wi-Fi SDK inventory supplies aggregate scan records. Its readout time
is a receipt time, not a beacon callback timestamp; it must not enable the
existing frame-recency flag. Its NUL-terminated SSID field also does not
reconstruct bytes after an embedded NUL in a raw beacon SSID element.

BLE identity comparisons include address type. Different receivers retain
independent RSSI and timing evidence. Cross-radio duplicate observations do
not establish unique physical-device counts, and two received reports do not
necessarily represent two transmitted packets. A C5 observation must not
borrow S3 raw-advertisement, scan-response or action-eligibility evidence.

The existing C5 scanner can request extended 1M and coded-PHY observation in
the current build configuration. The S3's existing legacy scan path can
retain names and scan responses through its own discovery workflow. Actual
coverage depends on broadcasters, the installed SDK/controller, antennas and
RF conditions. It must be validated with suitable test advertisers.

Two receivers on the same handheld are not a calibrated direction-finding
array. Fox hunting presents each receiver's signal and age independently;
it does not infer bearing, exact distance or triangulation from their RSSI.

## Acceptance required

- Compare repeated Wi-Fi-only scans against the prior C5-only baseline using
  the same dwell/country policy and AP environment. Record completion latency,
  per-band observations, retained/dropped inventory and internal/PSRAM minima.
- Verify Stop, Back, failed starts, C5 resets and repeated restart, including
  transitions to ordinary AP tools, mixed Recon and Wardriving.
- Verify that sorting and refreshing cannot change the selected identity;
  disappearing or changed APs must fail reacquisition explicitly.
- Compare BLE observations on each receiver using controlled legacy and coded
  advertisers. Check duplicate handling, address types, report age and buffer
  truncation without assuming every address is a different device.
- Check fox hunting with a selected owned advertiser and movement, including
  a device heard by only one receiver, stale samples and Stop/Back recovery.

Faster discovery and improved coverage are goals to measure. Compilation and
host fixtures alone cannot establish either result on the Cerberus board.

## Recorded validation — 2026-11-01 UTC

The final Windows checkpoint passed 22 targeted runners against stable inputs.
This includes all eight new radio suites and the affected scan observation,
probe/relation evidence, packet-monitor worker, incremental SPI readout,
handshake, Watch timing, Recon parallelism, BLE tracking/discovery UI and
build-report/package-proof regressions. The complete configured 115-suite
matrix was not rerun for this checkpoint; it remains available through
`tools/run_host_checks.py` and the Windows/Linux workflow.

Host checks used Python 3.14.2 with Zig 0.14.1. Native builds used Python
3.12.14 and PlatformIO 6.2.0 with the repository's pinned ESP-IDF/toolchains.
All five native targets and the existing native gates passed on the stable
repeat build:

| Target | Application image bytes |
| --- | ---: |
| S3 main | 1,866,864 |
| Onboard C5 | 1,658,160 |
| S3 programming bridge | 255,248 |
| Expansion C5 WROOM | 926,400 |
| Expansion C5 XIAO | 930,624 |

The matched S3/C5/bridge development package passed the verify-only package
checks for hashes, chip headers, partitions and image bounds. Its three app
hashes match the stable native report. The final native and targeted-host
input fingerprints match the current checkout. Generated receipts are
`artifacts/integration/build-report.json`,
`artifacts/host-checks/rev94-radio-checks.json` and
`artifacts/build/matched-build.json`.

Initial validation exposed obsolete production-extraction fixtures for the
new scan internals; those fixtures were updated without dropping their
lifecycle assertions. The initial native run also generated SDK configs.
Earlier failed/changed-input reports were retained before the final passing
checks. Visual host previews verified the C5 list/details, and the Fox Hunt
render test confirms that pending target changes show neither receiver's
previous target sample under the new identity.

No device was flashed, no over-the-air performance test was run, and no
hardware acceptance is claimed. This checkpoint is a separate local follow-up
to the submitted rev93 contribution.
