# Cerberus Unchained

Experimental Cerberus firmware with integrated
wardriving, C5 Wi-Fi capture and analysis, S3 BLE discovery and tools, Remote ID
receive workflows, AP/client navigation, and optional expansion radios.

This is a separate project from
[Cerberus Sentinel](https://github.com/463N7/Cerberus-Sentinel). Sentinel keeps its
own scope and releases. Unchained starts from the complete rev81 contribution,
with its original Git history and contributor attribution preserved.

**Current source version: `0.1.0-dev.rev124`, paired with onboard
C5 protocol firmware revision 69. No Unchained binary release is selected yet.**
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

Rev124 implements combined S3/C5 Wi-Fi results with SD save/reopen,
AP/SSID views and copy saving, and Recon gains a factual dashboard in place
of its live mascot, plus an optional RSSI radar. All 134 host suites and five
native targets pass on the same 1,108 unchanged inputs. The linked audit measures
4,800 bytes less fixed S3 data/BSS and a 4,992-byte smaller S3 image; C5 fixed
memory is unchanged. Optional viewer/worker allocations and physical acceptance
are tracked separately. The matched rev124/C5 firmware69 pair is installed with
verified image writes, paired application boot, display buffering and mounted
SD. A second idle capture more than 46 seconds later confirms advancing uptime
without additional SPI errors. Physical touchscreen, Wi-Fi save/reopen, Recon
recording, RF and sustained-load acceptance remain pending.
See [the rev124 checkpoint](docs/DUAL_RADIO_REV124.md) for validation status and
[saved Wi-Fi lists](docs/WIFI_SAVED_LISTS.md) for the storage and provenance
contract. These changes do not establish complete Marauder parity.

Rev123 implements Marauder-compatible retained Find My JSON import, offline
inspection, copy saving and export from complete saved S3/C5 originals and
matched responses, considered independently. Imported provenance stays unknown.
The separate model is bounded to 64 records, 8,192 total payload bytes and
49,152 JSON bytes; one optional JSON viewer and the existing serialized worker
handle the files. Ordinary native `.BLE` allocation is unchanged. Focused codec,
IO/storage and UI checks and independent reviews pass. The first complete
132-suite host run passed on 1,096 unchanged inputs; all five native builds
and the linked resource audit pass. S3 fixed data/BSS increases by 32 bytes,
C5 is unchanged, and optional JSON viewer/worker allocations total 68,200 bytes
before allocator overhead and other retained stores. Matched package build
and verify-only checks pass for three targets and nine images. See
[rev123](docs/DUAL_RADIO_REV123.md) for the exact local package and evidence.
The preceding rev123/C5 firmware69 update was installed with verified image writes,
paired application boot, display buffering and mounted SD. A second idle capture
more than 42 seconds later confirmed the settled boot state. Physical RF,
touchscreen, save/reopen, GPS and sustained-load acceptance remain pending.
This offline interchange does not restore imported records into live discovery
or establish full Marauder parity.

Rev123 and the rev114–122 source milestones below are included in installed
rev124. Their individual checkpoint documents preserve the evidence and
installation status recorded when each milestone was completed.

Rev122 implements integrated GPS DATA and received-NMEA views, independent GGA
metadata freshness and retained UART cleanup. GPS remains optional and OFF
unless selected through Wardriving. Focused checks and independent reviews
pass, including stable paging, whole-line privacy masking and retained UART
cleanup. All 130 host suites, five native builds, the linked resource audit and
matched package verification pass. Measured S3 fixed data/BSS grows by 128 bytes;
the optional trace and display copy use 2,624 bytes of PSRAM, excluding allocator
overhead. C5 fixed data/BSS is unchanged. See
[rev122](docs/DUAL_RADIO_REV122.md). No GPS module is attached. This is not a
full-parity or physical acceptance claim.

Rev121 implements C5 receive cleanup and Channel Survey handoffs. Independent
review, all 128 host suites, five native builds, resource measurements and
matched package verification pass on the retained final inputs.
The current Channel Survey workflow requires C5 firmware 69, although its
unchanged wire commands originated in firmware 46. See
[rev121](docs/DUAL_RADIO_REV121.md) for evidence and limits.

Rev120 replaces owner-only shared BLE host release with
an atomically checked claim token. Existing scanner, central, advertiser,
Radio Lab and Wardriving callers use the exact lease. See
[rev120](docs/DUAL_RADIO_REV120.md) for validation and acceptance limits.

Rev119 binds Wardriving S3 BLE acquisition, completed
evidence and cleanup to an exact passive scan. Unresolved cleanup stays in
STOPPING. See [rev119](docs/DUAL_RADIO_REV119.md) for validation and limits.

Rev118 binds Recon S3 acquisition, completed evidence
and cleanup to an accepted passive scan, while retaining independent C5 Wi-Fi
scheduling. See [rev118](docs/DUAL_RADIO_REV118.md)
for validation status and acceptance limits.

Rev117 binds S3 Fox Hunt callbacks and cleanup to its exact tracking scan,
retaining independent C5 tracking and source-selective retry. See
[rev117](docs/DUAL_RADIO_REV117.md).
The [coverage ledger](docs/MARAUDER_COVERAGE_LEDGER.md) now distinguishes these
receive milestones from installation and physical acceptance;
[remaining-work notes](docs/RECEIVE_REMAINING_GAPS.md) record the broader audit.

Rev116 gives BLE Analyzer independent receiver intent and retry, and binds S3
Analyzer callbacks and cleanup to its exact scan. See
[rev116](docs/DUAL_RADIO_REV116.md) for validation and acceptance limits.

Rev115 makes ordinary Bluetooth discovery intent independent of either
receiver's failure, adds source-specific retries and preserves Stop/navigation
through release. See
[rev115](docs/DUAL_RADIO_REV115.md) for validation and acceptance limits.

Rev114 moves the dedicated Find My Monitor onto the existing S3 and C5
discovery owners, with separate receiver status, frozen row selection and
retained SD evidence. See [rev114](docs/DUAL_RADIO_REV114.md) for validation
and physical acceptance limits.

Rev113 adds optional compact C5 Bluetooth response-result transfer, preserving
the existing frozen evidence and full-format compatibility. See
[rev113](docs/DUAL_RADIO_REV113.md) for validation and limits. The matched
rev113/C5 firmware68 update was installed with verified writes, paired boot and
mounted SD. Physical feature and performance acceptance remain pending.

Rev112 addresses independent S3/C5 Bluetooth Fox Hunt startup, source-specific
retry, separate signal/status presentation and retained Stop/Back cleanup.
The preceding rev112/C5 firmware67 update was installed with verified writes,
paired boot and mounted SD. See [rev112](docs/DUAL_RADIO_REV112.md) for its
validation and remaining physical acceptance. Paired boot does not establish
full feature acceptance or complete Marauder parity.

Rev111 removes a redundant response-mode dispatch from the passive C5 EXT
receive path while preserving response invalidation and owner cleanup. It
also reconciles stale Bluetooth/storage coverage notes with the implemented
rev110 behavior. See [rev111](docs/DUAL_RADIO_REV111.md) for validation and
installation status. Rev111 was the verified installed baseline before the
rev112 update. Full parity and physical feature acceptance remain open.

Rev110 adds an explicit C5 response-request mode to BLE discovery and family
views, while preserving passive as the default. S3 and C5 observations remain
source-labelled; original advertisements and matched scan responses have
independent live details and v6 SD evidence. The v1-v5 readers remain supported.
See [the rev110 checkpoint](docs/DUAL_RADIO_REV110.md) for validation, memory
costs and remaining physical checks. These changes are included in installed
rev111; physical response and saved-report acceptance remains pending.

Rev109 carries the C5 full-original family record into v5 SD reports and
offline HINTS. Older v1-v4 reports remain readable with missing evidence
explicit. Source identity, original previews, S3 responses and historical
ages keep their separate meanings. Implementation, validation and resource
costs are tracked in [the rev109 checkpoint](docs/DUAL_RADIO_REV109.md).
The matched rev109/C5 firmware66 update was installed with verified writes,
paired boot and mounted SD. Physical scan/saved-report acceptance remains
pending; boot confirmation is not complete feature validation.

Rev108 adds C5 full-original classification and focused admission for the six
passive BLE family views. The S3 retains its independent evidence and controls
the combined list and owner transitions. Generic discovery, tracking and
Analyzer avoid the additional family parsing and transfer. All 124 host suites
and five native targets pass. Measured additions are 3,312 bytes across optional
S3 PSRAM stores, 240 S3 internal bytes and 3,072 C5 internal bytes. Behavior,
resource costs and limits are tracked in
[the rev108 checkpoint](docs/DUAL_RADIO_REV108.md). These changes are included
in the installed rev109 update; their physical feature checks remain pending.

Rev107 extends the existing concurrent receivers into source-labelled BLE
family lists and C5 original-advertisement hint details. It reuses committed
observations and the existing strict parsers. The complete retained-report
requirement, remaining C5 retention gaps and validation are tracked in
[the rev107 checkpoint](docs/DUAL_RADIO_REV107.md). All 124 host suites and five
native targets pass. The optional discovery store grows by 48 PSRAM bytes;
fixed S3/C5 data and BSS are unchanged. Included in installed rev109; physical
acceptance of this local checkpoint is pending.

Rev106 adds a dedicated passive C5 activity path beside the S3 BLE Analyzer.
The compact source-clock samples avoid inventory downloads and keep the two
receivers' rates, counter scopes and gaps separate. All 123 host suites and five
native targets pass. The companion state grows by 424 PSRAM bytes; fixed
data/BSS grows by 16 bytes on S3 and 48 bytes on C5. Local validation and limits
are tracked in [the rev106 checkpoint](docs/DUAL_RADIO_REV106.md).
The matched rev106/C5 firmware65 update was installed with verified writes,
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

The rev82-rev93 contribution adds bounded Wi-Fi result transfer and recovery,
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
