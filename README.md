# Cerberus Unchained

Experimental Cerberus firmware for the expanded rev95 feature set: integrated
wardriving, C5 Wi-Fi capture and analysis, S3 BLE discovery and tools, Remote ID
receive workflows, AP/client navigation, and optional expansion radios.

This is a separate project from
[Cerberus Sentinel](https://github.com/463N7/Cerberus-Sentinel). Sentinel keeps its
own scope and releases. Unchained starts from the complete rev81 contribution,
with its original Git history and contributor attribution preserved.

**Current source version: `0.1.0-dev.rev95` (candidate), paired with onboard
C5 protocol firmware revision 61. No Unchained binary release is selected yet.**
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

Rev95 addresses the retained S3 memory limitation observed after Bluetooth use.
It moves eligible NimBLE host allocations to PSRAM, preserves the existing
buffer counts and host lifetime, and rejects BLE initialization when optional
PSRAM is absent. It also adds cached per-radio/lifecycle diagnostics and bounded
recovery from an unknown C5 firmware cache. All 119 host suites and all five
native targets pass on stable inputs. The matched onboard package also passes
image, partition, source and console-stack checks. The repeated
BLE-to-split-Wi-Fi device check remains pending. The paired rev95 build is
installed: both chips are online, SD is mounted, and all three new diagnostic
commands respond. Cold S3 internal free RAM is 113,524 bytes. See
[radio memory recovery](docs/RADIO_MEMORY_RECOVERY.md).

Rev94 adds adaptive dual-radio scanning: Wi-Fi-only scans use S3 for 2.4 GHz
and C5 for 5 GHz when both are available; ordinary BLE discovery and fox hunting
gain a separate C5 receiver alongside S3. Mixed Recon and Wardriving keep C5
Wi-Fi plus S3 BLE. Receiver provenance, bounded memory, Stop/Back recovery and
fresh AP selection checks are part of the implementation. The paired rev94
build was installed on the development board; the owner confirmed completed
split Wi-Fi scans, Wi-Fi Stop/restart/Back and activity on both BLE receivers.
Diagnostics confirm repeated C5 5 GHz sweeps and active S3 BLE discovery.
A confirmed limitation remains: after BLE use, the initialized S3 Bluetooth
host retains enough internal memory to force Wi-Fi onto the C5 fallback.
Repairing that transition is the next priority. Dual Fox Hunt, comparative
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
