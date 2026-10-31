# Cerberus Unchained build and release

Unchained is experimental firmware built on its rev81 foundation, with the
rev82–93 passive observation, analysis, storage and Wardriving additions in this
contribution. The source version comes from the checked-in CMake configuration;
the onboard C5 protocol revision remains 60. A version label, host check or
compiler result does not establish on-device acceptance.

## Hardware and builds

Use the production Cerberus ESP32-S3 profile with 16 MB flash and 2 MB quad
PSRAM, onboard ESP32-C5 with a 2 MB firmware budget, ST7796/GT911 display/touch,
and the existing microSD and SPI/READY wiring. Keep the checked-in pin and
partition definitions. Do not enable octal PSRAM on this board.

Install PlatformIO Core 6.1.19 in Python 3.11 or newer:

```sh
python -m pip install platformio==6.1.19
python tools/build_unified.py --list
python tools/build_unified.py --target all
```

The five-target matrix includes the S3 application, onboard C5, temporary S3
programming bridge, and the optional WROOM and XIAO expansion C5 variants.
Compiling expansion firmware does not verify a fitted expansion board. The S3
build also runs the READY placement and native console-frame gates; its local
ELF inspection never connects a debugger to a device. See the scope of those
checks in [validation](VALIDATION.md).

For a matched onboard installation package:

```sh
python tools/build_unchained.py --target all --package
```

The package is created in `dist/cerberus-unchained-<version>-<timestamp>/`.
It contains separate S3, C5, and bridge images, actual flash offsets,
checksums, source fingerprints, recovery information, and attribution. Its final
S3 ELF must match the native console proof recorded by the matched build;
packaging rejects stale ELF/source/checker evidence and includes the proof JSON
and debugger text in `native-validation/`. The gate adds no hardware operation.
On a clean checkout, dependencies or generated SDK settings can change on the
first build. Review the changes, then rerun the matched build before packaging.
The compatibility entry points `build_sentinel.py` and `package_sentinel.py`
also produce Unchained packages in this repository.

Run the host checks separately from firmware builds:

```sh
python -m pip install ziglang==0.14.1
python tools/run_host_checks.py --jobs 2
python tools/check_build_unified.py
python tools/check_field_kit.py
```

The runner covers 107 named suites and uses separate Zig caches. Run it against
stable sources, separately from firmware builds. See [validation](VALIDATION.md)
and [the current contribution and remaining work](CONTRIBUTION_REV93.md). The
[original rev81 contribution](CONTRIBUTION_REV81.md) remains historical context.

## Recovery and matched processors

Use all three images from the same Unchained package. The temporary bridge
runs on the S3 to reach the onboard C5; the final S3 application then replaces
the bridge. Preserve each chip's destination and generated offsets. A C5
bootloader starts at `0x2000`; never write C5 images to the S3.

For S3 ROM download mode, hold **S3 BOOT**, tap **S3 RESET**, then release
BOOT. See the inherited [USB bridge protocol and recovery guide](C5_USB_BRIDGE.md)
for the board workflow. Its Sentinel release labels are historical. Restoring
only the S3 does not update or verify the C5. Current factory partitions have
no OTA rollback slot.

## Installer and release separation

The inherited Sentinel 1.0.0 binaries are removed from the current web asset
tree and preserved under `reference/sentinel-1.0.0/`, with their original
checksums and notices. They also remain in Git history and the Sentinel project.
`web/public/firmware/latest.json` explicitly records that no Unchained release
is available. The browser refuses installation in that state and rejects
Sentinel product manifests.

After a matched Unchained package has passed the appropriate checks, validate
it and deliberately select it for a local installer build:

```sh
python tools/export_web_release.py dist/<package> --verify-only
python tools/export_web_release.py dist/<package> --set-latest
cd web
npm ci
npm run build
```

Exporting files does not publish the site, flash a board, or turn a development
candidate into a stable release. Document device acceptance against the exact
image hashes before a stable release. The manual firmware workflow only saves
candidate artifacts after the five-target gate; no workflow deploys a site or
promotes a production release. Artifacts follow the repository's access policy;
they are not inherently private in a public repository.

## Sharing fixes

The imported baseline is tagged `upstream-rev81` at
`531f666d0600604c2502bdb64dd41d2a1a25a37e`.
Fresh clones do not inherit an `upstream` remote. When a Sentinel fix is
needed, configure a separate remote for `https://github.com/463N7/Cerberus-Sentinel.git`,
fetch it, and cherry-pick individual reviewed fixes when appropriate.
Keep attribution and review any feature dependencies before applying them.
