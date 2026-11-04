# Rev138: streaming writer efficiency and verified CSV readback

The installed rev137 combined-mode run completed and its two files were read
back from SD. All 16,592 accepted observations are accounted for in the saved
files, with no reported queue drop or radio/SD service error. This establishes
the tested receive-to-file path, not RF completeness or comparative performance.
The LC29H delivered checksum-valid NMEA at 115200 baud but lost its navigation
fix partway through this stationary run. The unlocated file preserves those
sightings without inventing positions.

## Changes

Streaming previously called the legacy 160-slot duplicate-history lookup for
each Wi-Fi/BLE observation, then ignored its decision. Rev138 skips that work
entirely. Every received observation remains eligible for its own CSV row.
Legacy snapshot consumers keep their history behavior and failed appends still
cannot commit a history entry. This removes known CPU/PSRAM work; no measured
increase in RF detections is claimed.

The WiGLE metadata pre-header now contains the documented Earth fields
`star=Sol,body=3,subBody=0`, the actual firmware build version, and accurate
timing provenance for onboard streaming versus expansion snapshots. CSV row
data and ordering are unchanged. No GNSS accuracy is fabricated from HDOP.

`tools/validate_wardrive_csv.py` reads complete CSV files without changing or
uploading them. It checks UTF-8/CSV structure, the ordered 14-column header,
metadata, addresses, UTC dates, finite/bounded coordinates, channel/frequency,
RSSI, optional numeric measurements, manufacturer IDs and a final newline.
It supports the Wi-Fi/BLE subset emitted by Cerberus. Error reports contain
line numbers and codes, not observed addresses, names or positions. Repeated
rows are preserved and counted. Unknown accuracy/altitude produce explicit
warnings, not replacement measurements. A local pass is not a server receipt.

```sh
python tools/validate_wardrive_csv.py path/to/WIGLE001.CSV
```

## Live evidence

| Installed rev137 combined run | Result |
| --- | --- |
| Duration | 392.458 seconds |
| S3 2.4 GHz Wi-Fi observations | 3,290 |
| S3 BLE observations | 11,338 |
| C5 5 GHz Wi-Fi observations | 1,964 |
| Located observations | 2,291: 685 Wi-Fi, 1,606 BLE |
| Unlocated observations | 14,301 |
| Reported queue drops | S3 0, C5 0 |
| Pending observations after Stop | S3 0, C5 0 |
| RF owners after Stop | Both released |
| Saved files | Two, both read back |

Private local evidence is under `artifacts/device-rev137/sd-readback` and the
`combined-*` diagnostic snapshots. The pre-header-corrected local copy preserves
the original column header and observation bytes exactly. Its preflight result
is PASS_WITH_WARNINGS for 2,291 missing accuracy measurements; server acceptance
is NOT_TESTED. The original on-card file is preserved.

## Validation status

All 160 host suites pass, including ten new file-level preflight tests and
cross-checking actual C formatter output with an independent CSV reader. Routing
fixtures verify 1,000 repeated located observations, a BLE observation and stale
queue routing while asserting that streaming performs zero legacy lookups.
Native builds, matched packaging and installation of rev138 are pending.

Further acceptance needs sustained GPS fix/UTC, repeated combined-mode runs,
separate-mode tests, endurance and a real service import result. Channel dwell
and S3 Wi-Fi/BLE allocation should be tuned with controlled coverage comparisons;
shorter sweeps alone do not establish better reception.

The user requested saved Wi-Fi and three upload destinations after the capture
and export work. That scope is recorded in [the upload design](WARDRIVE_UPLOADS.md).
It is not implemented or represented as an available device feature by rev138.

Sources: [WiGLE's CSV 1.6 specification](https://api.wigle.net/csvFormat-1_6.html),
[official Android export implementation](https://github.com/wiglenet/wigle-wifi-wardriving/blob/main/wiglewifiwardriving/src/main/java/net/wigle/wigleandroid/background/ObservationUploader.java),
and [Quectel LC29H protocol specification](https://www.quectel.com/content/uploads/2022/02/Quectel_LC29H_SeriesLC79HAL_GNSS_Protocol_Specification_V1.4.pdf).
Quectel documents GST error statistics in metres. Receiving and epoch-matching
those fields is future work; support/output settings depend on the receiver
variant and firmware. A configurable constant or HDOP-as-metres is not a substitute.
