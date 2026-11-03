# Rev135: C5 callback payload compatibility

Rev134 is the installed baseline for this source checkpoint. Its new diagnostic
identified the owner's zero-total report at the native receive-metadata gate:

| Live session | Channel | Callbacks | Invalid metadata | Valid frames | Selected matches |
| --- | ---: | ---: | ---: | ---: | ---: |
| First | 48 (5 GHz) | 1,926 | 1,926 | 0 | 0 |
| Second | 10 (2.4 GHz) | 5,267 | 5,267 | 0 | 0 |

These were different exact sessions after the owner changed the selection.
Both had zero malformed, unsupported, outside-window and invalid-clock counts,
and no reported source or controller error. The first sample's UI tick gap was
19 ms, with a 140 ms maximum. Thus the conditional slow-tick defect fixed in
rev134 is not established as the cause of these captures. Receive windows and
counter readout advanced; all callbacks failed before address matching.

## Callback contract and correction

The installed ESP-IDF 6.0.1 public callback type defines the data/management
payload extent through `rx_ctrl.sig_len`. Its C5-supported `simple_sniffer`
example rejects `WIFI_PKT_MISC` and nonzero receive status, then consumes
`sig_len - 4` bytes, excluding FCS. The native HE metadata also contains
`dump_len`, but equality with the callback extent is not a requirement of that
public example. Sources:

- [Versioned callback type](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_wifi/include/local/esp_wifi_types_native.h#L109-L113).
- [Versioned sniffer implementation](https://github.com/espressif/esp-idf/blob/v6.0.1/examples/network/simple_sniffer/main/cmd_sniffer.c).

Count had additionally required `dump_len == sig_len - 4`; Probe Sniff had
required a nonzero dump length and the same equality. Rev135 removes those
extra conditions and uses the documented callback extent. Count admits only
management/data types before payload access. Probe Sniff retains management
type admission. Receive errors and short lengths remain rejected; strict
header/element parsing and raw-recording limits remain in force. Zero-payload
MISC callbacks must never reach a header read or raw copy.

The existing diagnostic establishes the rejection stage, not the individual
physical values of `rx_state` and `dump_len`. The source correction is grounded
in the same-version SDK contract. A fresh device test must establish whether
valid frames and selected matches now advance. No new wire projection, radio
command, task or per-callback allocation is introduced. Capability 74 remains
the compatible protocol version.

## Processor roles and display

A single-channel plan uses C5 reception. The S3 panel now describes its work
positively: PROCESSING, COUNTS + DISPLAY, RECEIVER: C5. The S3 cannot receive
5 GHz; a multi-channel plan can assign its supported 2.4 GHz channels separately
from C5 coverage. Both processors contribute without presenting duplicated
receptions as unique packets.

When a validated publication has metadata rejections and no valid frames, the
source card reports METADATA REJECTED. Unknown counters do not trigger the
warning. Fault, Stop, error and setup states keep their priority; readout
progress and recording-loss information remain visible.

## Validation boundary

Focused native Count, Probe Sniff and UI regressions are being prepared for the
old-reject/new-accept callback cases, receive errors, short lengths, inaccessible
MISC payload and raw-recording extent. Full validation remains the 158-suite
host registry, five native targets, exact resource audit and matched package.
No rev135 image is installed at this source checkpoint. Build and installation
results will be recorded after they succeed.

The required physical check is fresh AP discovery followed by Packet Count
with PCAP off, advancing validated frames and selected totals, then Stop/Back
and another Wi-Fi scan. Both bands, optional recording and Probe Sniff need
their own acceptance. Full Marauder parity, serial capture, Mesh Save/Open and
Locate reception, GPS and expansion-board validation remain unfinished.
