# Cerberus Unchained

Experimental Cerberus firmware for the expanded rev106 feature set: integrated
wardriving, C5 Wi-Fi capture and analysis, S3 BLE discovery and tools, Remote ID
receive workflows, AP/client navigation, and optional expansion radios.

This is a separate project from
[Cerberus Sentinel](https://github.com/463N7/Cerberus-Sentinel). Sentinel keeps its
own scope and releases. Unchained starts from the complete rev81 contribution,
with its original Git history and contributor attribution preserved.

**Current source version: `0.1.0-dev.rev106`, paired with onboard
C5 protocol firmware revision 65. No Unchained binary release is selected yet.**
The installer stays unavailable until an Unchained matched image set is built
and deliberately selected. Sentinel 1.0.0 binaries are not Unchained firmware.

## Build and validation

Use Python 3.11 or newer and PlatformIO Core 6.1.19. The repository includes the
C5 platform adapter and pinned firmware configurations.

```sh
python -m pip install platformio==6.1.19 ziglang==0.14.1
python tools/run_host_checks.py
python tools/build_unified.py --list
python tools/build_unified.py --target all
```

To build and package the three matched onboard images (S3 application, onboard
C5, and temporary USB bridge):

```sh
python tools/build_unchained.py --target all --package
```

First-time dependency resolution can change generated inputs. Review those
changes and rerun the build for a stable receipt. Run firmware builds and host
checks separately. These commands do not flash hardware or publish a release.
See [Unchained build and release instructions](docs/UNCHAINED_BUILD.md) and
[the inherited validation procedures](docs/VALIDATION.md).

## Development status

Rev106 adds a dedicated passive C5 activity path beside the S3 BLE Analyzer.
The compact source-clock samples avoid inventory downloads and keep the two
receivers' rates, counter scopes and gaps separate. All 123 host suites and five
native targets pass. The companion state grows by 424 PSRAM bytes; fixed
data/BSS grows by 16 bytes on S3 and 48 bytes on C5. Local validation and limits
are tracked in [the rev106 checkpoint](docs/DUAL_RADIO_REV106.md).
The matched rev106/C5 firmware65 update is installed with verified writes,
both chips online and SD mounted. Touchscreen, RF and loaded-memory acceptance
remain pending.

Rev105 adds receiver filtering and file/name/signal ordering to saved BLE
reports. Browsing uses a bounded index over the loaded document, preserves
frozen selections and historical ages, and makes no radio or storage requests.
The v4 file format and older readers are unchanged. All 123 host suites and
five native targets pass. The existing optional viewer grows by 72 PSRAM bytes;
fixed internal data/BSS is unchanged. Local validation and remaining physical
acceptance are tracked in [the rev105 checkpoint](docs/DUAL_RADIO_REV105.md).
This previously local change is included in the installed rev106 update.

Rev104 combines ordinary BLE discovery into one source-labelled list, with
ALL/S3/C5 receiver filtering, stable sorting and frozen selection. It reuses
the existing concurrent receivers, keeps distinct signal/time evidence and
adds no radio task or full observation bank. Family-specific views retain
their S3 evidence. All 122 host suites and five native targets pass. The new
presentation store adds 768 PSRAM bytes; fixed internal data/BSS is unchanged.
Measured resource costs and pending physical acceptance are tracked separately
in [the rev104 checkpoint](docs/DUAL_RADIO_REV104.md). This previously local
change is included in the installed rev106 update.

Rev103 moves the read-only console's formatting scratch from its task stack to
one reusable PSRAM buffer, with cleanup only after the writer is quiescent.
It preserves the 4 KiB stack, full 64-bit formatting and output capacity. Long
archive paths and filenames now print separately to avoid combined truncation.
The formatter's compiled frame falls 848 to 80 bytes, and the observed remaining
console stack rises 524 to 1,292 bytes in the read-only sequence. All 121 host
suites and five native targets pass; the matched local update is installed with
both chips online and SD mounted. See [the rev103 checkpoint](docs/DUAL_RADIO_REV103.md)
for evidence and limits; USB handover, sustained load and full feature acceptance
remain open.

Rev102 preserves the C5 receive envelope in v4 SD reports alongside the S3
matched-response evidence. The existing offline viewer gains C5 RECEIVE INFO;
old v1/v2/v3 reports remain readable without inventing absent metadata. Saved
rows remain historical evidence and cannot become live action targets. All 120
host suites and five native targets pass; the matched local package is installed
with verified writes, both chips online and SD mounted. Fixed internal data/BSS
is unchanged; optional archive buffers grow 2,088 bytes. Physical save/reopen,
receive and runtime stress/stack acceptance remain pending. See
[the rev102 checkpoint](docs/DUAL_RADIO_REV102.md).

Rev101 preserves C5 callback source, delivered length, SID/PHY, properties,
controller status and full AD-layout validity with each original observation.
The optional metadata read still uses eleven chunks per row. The existing C5
detail view adds RAW ADV / RECEIVE INFO tabs; older peers retain explicit
unknown metadata. All 120 host suites and five native targets pass on the final
source, including the unsaved-metadata label. The matched package is installed
with both chips online and SD mounted; live reception and touchscreen acceptance
remain pending. See
[the rev101 checkpoint](docs/DUAL_RADIO_REV101.md).

Rev100 preserves S3 matched scan-response bytes separately from original ADV
evidence and extends the existing archive with v3 write and v1/v2 read support.
Selected observations retain full scan epochs. C5 v2 discovery gains bounded
32-slot replacement and frozen replacement/drop counters; legacy scans retain
their prior admission behavior. All 120 host suites and five native build
targets pass on unchanged inputs. The matched local package is installed with
verified image writes, both chips online and SD mounted. Touchscreen, live
reception and real-card save/reopen acceptance remain pending. See
[the rev100 checkpoint](docs/DUAL_RADIO_REV100.md).

Rev99 extends the existing BLE SD archive to one report containing up to 32
observations per receiver. New v2 files preserve receiver identity, S3 peak
versus C5 latest-ADV RSSI, and exact historical time versus conservative age
bounds. Old v1 files remain readable as LEGACY. Save freezes selected evidence
and waits for normal receiver cleanup before starting the SD worker; larger
result copies run outside the interrupt-disabled critical section. All 120 host
suites and five native targets pass on matching unchanged inputs. The matched
package is installed with verified images, both chips online and SD mounted.
Linked fixed RAM increases by 128 S3 bytes and zero C5 bytes; the larger report
buffers use optional PSRAM. Physical save/reopen and navigation acceptance remain
pending. See
[the rev99 checkpoint](docs/DUAL_RADIO_REV99.md)
for current build, installation and acceptance evidence.

Rev98 adds coherent C5 Bluetooth snapshots during the existing
eight-second listening sweep. S3 transfers and publishes a frozen bank while
C5 continues receiving, with separate update/sweep counters and first-result
reader timing. The existing completed-sweep path remains for older C5 peers.
It also fixes CHANGE AP / SCAN returning a dual-origin AP to the legacy C5-only
list. All 120 host suites and five native targets pass on matching unchanged
inputs. That matched package was installed with verified images, both chips online,
display buffer active and SD mounted. S3 linked internal memory is unchanged;
the BLE coordinator adds 176 bytes in PSRAM and C5 static RAM grows by 3,312
bytes. Live-result latency, corrected navigation and repeated radio transitions
still need physical acceptance. See [the rev98 checkpoint](docs/DUAL_RADIO_REV98.md)
and [live BLE snapshot scope](docs/DUAL_BLE_LIVE_SNAPSHOTS.md).

Rev97 adds a verified reuse path for a selected C5 Wi-Fi result: its completed
scan session, full relationship proof and exact AP identity must still match
before the original bank can reach the AP tools. Valid reuse starts no new
scan and retains the original observation age. S3 results still require fresh
C5 reacquisition. Analysis labels and channel-history denominators follow the
actual band coverage; reopening one proven bank cannot create another sample.
The separate C5 Bluetooth list gains signal/name/scan-order sorting, a
conservative recent filter and visible age bounds/unknown timing. These presentation
controls retain copied device identities and do not restart either radio.
All 120 host suites and five native targets pass on matching unchanged inputs.
The linked internal-memory increase is 528 bytes. The matched rev97 package
is installed with all image hashes verified, both chips online, display ready
and SD mounted. Settled internal free RAM is 134,652 bytes before new scans.
Touchscreen checks of the new controls and handoff remain pending. Evidence is in the
[rev97 checkpoint](docs/DUAL_RADIO_REV97.md).

Rev96 moves the three CPU-only directory-list banks to lazy, stable PSRAM
allocations to preserve internal radio memory. Their payload totals 21,760
bytes. A missing/exhausted PSRAM bank produces an explicit memory error and a
zero visible row count; Back and later reloads remain available. All 120 host
suites and all five native targets pass on stable inputs. The linked internal
sections shrink by 21,744 bytes. The paired rev96 package is installed, both
chips are online and SD is mounted. Cold internal free RAM is 135,268 bytes,
exactly 21,744 bytes above rev95. After BLE Stop/Back and a split Wi-Fi scan,
the console captured 19 S3 plus 11 C5 APs, no scan errors and released owners.
Warm internal free RAM is 88,036 bytes, 22,500 above scan admission, and remained
stable at a later read. The owner reported repeated split success; the captured
C5 counter independently establishes one completed job so far. The SD-list
browsing and following-scan check remains pending.

Rev95 addresses the retained S3 memory limitation observed after Bluetooth use.
It moves eligible NimBLE host allocations to PSRAM, preserves the existing
buffer counts and host lifetime, and rejects BLE initialization when optional
PSRAM is absent. It also adds cached per-radio/lifecycle diagnostics and bounded
recovery from an unknown C5 firmware cache. All 119 host suites and all five
native targets pass on stable inputs. The matched onboard package also passes
image, partition, source and console-stack checks. On the preceding installed
rev95 build, after BLE Stop/Back the owner and console confirmed a split scan:
9 S3 APs plus 17 C5 APs in 4,959 ms, no scan errors and no held radio owners.
Warm internal free RAM was 66,292 bytes, only 756 bytes above the next-scan
admission threshold. This narrow reserve motivates rev96. See
[radio memory recovery](docs/RADIO_MEMORY_RECOVERY.md).

Rev94 adds adaptive dual-radio scanning: Wi-Fi-only scans use S3 for 2.4 GHz
and C5 for 5 GHz when both are available; ordinary BLE discovery and fox hunting
gain a separate C5 receiver alongside S3. Mixed Recon and Wardriving keep C5
Wi-Fi plus S3 BLE. Receiver provenance, bounded memory, Stop/Back recovery and
fresh AP selection checks are part of the implementation. The paired rev94
build was installed on the development board; the owner confirmed completed
split Wi-Fi scans, Wi-Fi Stop/restart/Back and activity on both BLE receivers.
Diagnostics confirm repeated C5 5 GHz sweeps and active S3 BLE discovery.
Rev94 could fall back to C5 after BLE retained too much S3 internal memory;
the rev95/rev96 memory work above addresses this measured limitation.
Dual Fox Hunt, comparative
speed, coverage and sustained lifecycle acceptance remain pending. See
[dual-radio scanning and validation](docs/DUAL_RADIO_SCANNING.md).

The rev82–rev93 contribution adds bounded Wi-Fi result transfer and recovery,
scan-list controls and passive SSID/trust review, BLE analysis and SD snapshots,
Recon inventory and session recording, SD copy error handling, and a live
Recent Networks viewer in the integrated Wardriving tab. C5 Wi-Fi and S3 BLE
observations feed the existing scan services. Recent Networks reuses those
observations without another radio scan or worker task.

The local rev93 checkpoint recorded 107 passing host suites and five firmware
targets. Its installed-board evidence belongs to that original development
checkpoint; the Unchained-branded rev93 contribution was unflashed at submission.
The later rev94 installation has the limited acceptance described above. See the
[rev93 contribution and validation summary](docs/CONTRIBUTION_REV93.md).

Reported Wi-Fi **NO MEM**, **Association Sleep**, and **Bad Message** failures
remain open in the feature ledger. Later local owner checks confirmed Back and
subsequent Wi-Fi scan navigation; these do not validate every active feature.
The new Recent Networks UI, BLE archive save/reopen, Recon recording contents,
GPS reception, sustained load and six-radio expansion still need acceptance.
The full Marauder port and Wardriving integration remain in progress.

- [Rev93 contribution and remaining work](docs/CONTRIBUTION_REV93.md)
- [Rev94 dual-radio scanning](docs/DUAL_RADIO_SCANNING.md)
- [Original rev81 contribution](docs/CONTRIBUTION_REV81.md)
- [Feature coverage ledger](docs/MARAUDER_COVERAGE_LEDGER.md)
- [Integration targets and layout](INTEGRATION.md)
- [SD resources and their notices](sdcard/README.md)

## Origin and maintenance

Unchained was started by **463N7** from **valleytechsolutions**'s
[Sentinel PR #1](https://github.com/463N7/Cerberus-Sentinel/pull/1), preserving
commit `531f666d0600604c2502bdb64dd41d2a1a25a37e` and its ancestors.
The `upstream-rev81` tag identifies that exact imported snapshot.

The original contribution and validation documents retain their historical
names and claims. The [imported Sentinel README](docs/SENTINEL_REFERENCE.md)
is reference material for the original firmware. Internal `sentinel_*` source
names remain for compatibility; the product, build versions, packages, and
installer identify Unchained separately.

Bring fixes between Sentinel and Unchained selectively using their shared
history. Neither project automatically incorporates the other's features.
Existing licenses and source/data attribution remain in force; see `LICENSES/`,
[Watch notices](docs/WATCH_THIRD_PARTY_NOTICES.md), and the SD resource notices.
