# Cerberus Unchained

Experimental Cerberus firmware for the expanded rev81 feature set: integrated
wardriving, C5 Wi-Fi capture and analysis, S3 BLE discovery and tools, Remote ID
receive workflows, AP/client navigation, and optional expansion radios.

This is a separate project from
[Cerberus Sentinel](https://github.com/463N7/Cerberus-Sentinel). Sentinel keeps its
own scope and releases. Unchained starts from the complete rev81 contribution,
with its original Git history and contributor attribution preserved.

**Current source version: `0.1.0-dev.rev81` (experimental), paired with onboard
C5 protocol firmware revision 60. No Unchained binary release is selected yet.**
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

The imported checkpoint recorded 79 passing host suites and five firmware
targets; those are historical results, not fresh Unchained hardware acceptance.
Reported Wi-Fi **NO MEM**, **Association Sleep**, and **Bad Message** failures
remain unresolved. Rev81 Back navigation and the next normal scan still need
physical confirmation. GPS reception, sustained load, saved-file correctness,
and six-radio expansion acceptance remain incomplete.

- [Contribution overview and remaining work](docs/CONTRIBUTION_REV81.md)
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
