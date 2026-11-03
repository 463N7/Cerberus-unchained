# Cerberus Unchained web flasher

The installer source is in `web/` in the public
[Cerberus-unchained repository](https://github.com/463N7/Cerberus-unchained).
It is separate from the original Sentinel installer. Do not replace the existing
`Cerberus-Flasher` website with this build.

The rev136 design uses the firmware's dark panels, cyan Cerberus emblem,
green terminal accents and a guided four-step installation. Fonts and their
license notices are bundled locally. No account or desktop application is needed.

## Current publication state

The rev136 GitHub prerelease is prepared as a **draft**. The redesigned flasher
is a **local preview**, pending the owner's requested visual review. No workflow
deploys it automatically. Pushing this source branch or passing web CI does not
publish a website. The download link will become public when the release is
published.

Publish to an Unchained-specific destination after review. The existing Sentinel
flasher has its own audience, firmware selection and history.

## Supported hardware and browser

- Production Cerberus ESP32-S3: 16 MB flash, 2 MB quad PSRAM, ST7796/GT911.
- Onboard ESP32-C5: conservative 2 MB firmware layout.
- Desktop Chrome or Edge, with the page opened directly over HTTPS or localhost.
- A USB data cable and one board connected at a time. Close other serial monitors.

Embedded previews can block Web Serial. Visual preview does not test a physical
flash. Use a supported desktop browser for device installation and repeat a
complete physical installation and recovery check when qualifying the installer.

## Matched release preparation

Build the three onboard targets from unchanged firmware inputs:

```sh
python tools/build_unchained.py --target all --package
python tools/export_web_release.py dist/<matched-package> --verify-only
python tools/export_web_release.py dist/<matched-package> --release-id cerberus-unchained-0.1.0-dev.rev136 --set-latest
python tools/export_web_release.py --verify-site web/public/firmware
npm --prefix web ci
npm --prefix web run build
python tools/export_web_release.py --verify-site web/dist/firmware
```

If dependency resolution changes generated firmware inputs during the first
build, review them and rerun the matched build. Run host checks separately.
Exporting and packaging do not access USB, publish anything or qualify a
development candidate as stable. Full hardware acceptance remains in progress.

The exporter validates the package checksums, chip headers, partition tables,
image bounds and matching protocol-source fingerprint. Release directories are
immutable. `firmware/latest.json` selects one complete image set by manifest
SHA-256. The browser downloads and checks all nine images before installation.
The selected rev136 pair expects C5 capability revision 74 and protocol 2.

## Installation and recovery

1. Install the temporary bridge on the S3.
2. Select the same board's bridge port and install the C5 firmware.
3. Return USB control to the S3 and install the final Unchained application.
4. Check firmware identity, buffered display, online C5 and clean startup.

The installer checks processor identity and preserves the original board identity
across USB reconnects. It verifies each written image against the release digest.
C5 transfers remain at 115200 baud with 4 KiB blocks. No eFuses are written.
The separate bootloader/table/application writes preserve NVS; important settings
should still be backed up before installing.

For S3 download mode: hold **S3 BOOT**, tap **S3 RESET**, then release BOOT.
A blank screen while the temporary bridge runs is expected. Recovery controls
can resume C5 installation or restore the S3. Restoring only the S3 does not
update or verify the C5. There is no OTA rollback slot in the current layout.

## Review and publication

Run `npm --prefix web run dev -- --port 4173` for local preview. Review the page,
release notes, supported hardware, consent, disabled/error states and recovery
controls. No preview or automated browser check should initiate a physical flash.

Publish only the verified **contents of `web/dist/`** to the chosen static HTTPS
host. Relative build URLs support a GitHub Pages project path. Preserve required
font, dependency and firmware notices. Never publish build logs, board reports,
backups, `.git`, credentials, toolchains or the workspace as the site.

`web-checks.yml` verifies the source firmware tree, builds the locked web project,
verifies the compiled firmware tree and saves the static website as a review
artifact. It does not deploy or select another firmware release.

## Privacy and attribution

Board identifiers, USB diagnostics and progress stay in browser storage unless
the operator saves a report. Reports include MAC addresses. The installer does
not upload board reports or radio captures. Firmware files are publicly
downloadable. Third-party fonts are served locally.

Keep `notices.txt`, the font licenses, both Watch notice documents and all
firmware licenses with their respective distributions. The package retains
`LICENSES/ESP32Marauder.txt`; the public site's notices include that full license.
See [the attribution guide](MARAUDER_ATTRIBUTION.md) for credit to JustCallMeKoko
and the ESP32 Marauder contributors. These notices do not grant a license for
the entire Cerberus project.
