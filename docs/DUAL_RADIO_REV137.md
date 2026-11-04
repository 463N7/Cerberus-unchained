# Rev137: streaming dual-radio Wardriving and configurable GNSS

This milestone replaces the integrated Wardriving scan-result polling path
with passive receive workers and bounded observation queues on both chips.
It also makes GNSS status available before recording and adds a single
Settings → GPS Receiver page. The public release and broad Marauder parity
remain separate work; this is a development candidate.

## Radio roles

| Mode | S3 | Onboard C5 |
| --- | --- | --- |
| Wi-Fi | Continuous 2.4 GHz channel sweeps | Continuous 5 GHz channel sweeps |
| Bluetooth | Continuous passive 1M BLE scanning | Passive 1M and coded-PHY BLE scanning |
| Wi-Fi + Bluetooth | A 2.4 GHz sweep followed by a 1.6-second BLE window | Continuous 5 GHz channel sweeps |

The S3 has one shared RF receiver. Combined mode therefore has explicit RF
windows; it cannot receive Wi-Fi and Bluetooth at the same instant. There is
no deliberate idle interval between sweeps. Channel changes, controller
transitions and task scheduling still take time. At least one scheduler tick
is yielded during a receive dwell so GPS, SPI and SD work cannot be starved
by a nominal 5 ms delay rounding to zero at the configured 100 Hz tick rate.

Wi-Fi reception captures beacons and observed probe responses without sending
probes or associating. S3 visits channels 1–11; C5 visits the configured 5 GHz
channel list with 120 ms dwell. Unsupported channels are skipped and counted;
the driver remains responsible for regulatory support. C5 explicitly selects
the 5 GHz band at entry and restores its previous band mode, channel and
receive filter before releasing ownership. BLE duplicate filtering
is disabled. Incomplete extended advertising fragments and malformed frames
are rejected rather than turned into invented observations.

The S3 queue holds 512 observations in PSRAM; C5 holds 128 in internal RAM.
RF callbacks only validate/decode and enqueue. The S3 writer alternates bounded
batches from both queues, writes CSV, and services GPS/expansion status while
RF reception continues independently. Queue overflow and high-water counters
are visible in diagnostics. Bounded buffers cannot guarantee lossless capture
under arbitrary traffic or a stalled SD card.

Every accepted sighting is a separate observation, including identical MAC,
SSID, RSSI and manufacturer fields. The Recent view also shows repeated
observations. Exact transport sequence ACKs prevent a lost SPI response from
appending a second copy of the *same* queued event; this is transport retry
handling, not RF identity deduplication. A failed SD append is not replayed
because partial writes are ambiguous. Such a session reports an SD error and
remaining queued records may be discarded during cleanup.

Stop first quiesces both receivers, drains accepted queues, closes and syncs
the files, then releases ownership. Failed cleanup retains its owner for retry.
A ten-second host-contact lease stops a receiver if its host disappears.
The onboard wire extension requires C5 firmware capability 75; install a
matched S3/C5 pair.
The temporary USB flashing bridge retains Bluetooth-disabled stubs and does
not link the receive engine's NimBLE implementation.

## GNSS settings and status

The default receiver configuration is AUTO baud, S3 RX GPIO 1, S3 TX assignment
GPIO 2 and PPS assignment GPIO 46. Receiver TX connects to S3 RX, and vice
versa. The LC29H-AA preset listens at 115200 baud; ATGM336H uses 9600 baud.
Manual mode selects a supported baud and board-free pin assignments. Presets
preserve the selected pins. Reserved board pins and conflicting assignments
are rejected. Configuration is stored in its own validated `gps_v1` NVS blob;
it is not added to the older settings-backup wire format.

The current adapter receives NMEA only. It does not transmit receiver commands
on GPIO 2, and GPIO 46 is reserved for future PPS use rather than clock
disciplining. The GPS settings screen identifies PPS as reserved.

AUTO starts at 115200, then tries 9600, 38400, 57600, 19200, 4800, 230400,
460800 and 921600 baud. Two checksum-valid sentences establish the baud;
ten seconds without valid NMEA restarts detection. UART input buffering is
4096 bytes and the reader processes bounded 256-byte chunks. The Wardriving
page and GPS settings can preview the receiver without starting a capture.
Preview and recording share one UART reader, so Start preserves an acquired
fix and leaving a preview does not stop an active recording's GPS.

The dashboard exposes receiver/baud detection, fix state and satellite count.
The existing GPS Data view provides detailed NMEA inspection. The read-only
`gps` console command reports health, baud, assignments and fix age without
printing coordinates. GGA altitude is retained only when its fresh timestamp
matches the RMC epoch, including fractional seconds; sentence order no longer
silently loses same-epoch altitude.

## Files and location integrity

Located observations use the existing WiGLE 1.6, 14-column CSV formatter:
`MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type`.
Timestamps are UTC and CSV fields are quoted/escaped. Repeated observations
are retained. Buffered writes sync after 256 rows or two seconds, and at Stop.
This reduces repeated SD synchronization without placing file I/O in RF
callbacks. Unexpected power loss can still lose buffered data.

A row needs a fresh, valid GPS position and UTC near its RF observation time.
RF age is transported from C5 and retained from S3; a delayed queue is not
silently located at the writer's later position. Observations without a
qualifying fix go to a separate unlocated CSV. No zero/default coordinates,
uptime dates, interpolated positions or HDOP-as-metres are invented. Binary
labels that cannot be represented faithfully in the upload format also remain
in the unlocated diagnostic output. Upload only the WiGLE file to a mapping
service. No automatic upload or service credentials are introduced.

The six-C5 expansion path remains integrated and uses the same writer when
enabled in a Wi-Fi-capable mode. Its physical backboard and ATGM336H acceptance
remain deferred; Bluetooth-only mode does not start Wi-Fi expansion modules.

## Evidence and reference boundaries

New host fixtures exercise the actual shared receiver on both target branches,
including C5 extended scanning, both 100 and 1000 Hz scheduling, repeated
reports, overflow, lost-host timeout, failed setup and failed cleanup. Writer
fixtures cover lost START/read responses, exact ACK progression, Stop draining
and failed SD writes. Strict frame/security decoding, 100,000 repeated FIFO
observations, repeated CSV rows, stale queue location routing, AUTO baud and
shared preview/session ownership are also covered.

Final matched-build, package and hardware results are recorded separately
after validation. A successful build or synthetic test does not establish RF
coverage, GPS reception, upload acceptance or comparative performance.

References reviewed for this implementation:

- [JustCallMeKoko ESP32DualBandWardriver](https://github.com/justcallmekoko/ESP32DualBandWardriver): channel scheduling and buffering reference. No source was copied into the new receive engine.
- [HaleHound-CYD](https://github.com/JesseCHale/HaleHound-CYD): public behavior documentation. The inspected repository tree did not contain its firmware implementation; no source-level efficiency comparison is claimed.
- [WiGLE Android uploader](https://github.com/wiglenet/wigle-wifi-wardriving): current CSV field order and UTC format.
- [WDGWars format documentation](https://www.wdgwars.pl/press): WiGLE 1.6 input compatibility. Live import is unverified.
- [Quectel LC29H specification](https://www.quectel.com/content/uploads/sites/3/2022/06/Quectel_LC29H_GNSS_Specification_V1.2.pdf): receiver baud reference.
- [Espressif S3 coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.0.2/esp32s3/api-guides/coexist.html) and [C5 coexistence](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/api-guides/coexist.html): shared RF constraints.

Existing third-party notices and attribution remain in force. These references
do not constitute endorsement or a measured claim that Cerberus outperforms
another device.
