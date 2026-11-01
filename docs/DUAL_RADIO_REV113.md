# Rev113: compact Bluetooth response-result transfer

Rev113/C5 firmware 68 is **built and package-verified, not installed**.
The verified installed baseline remains [rev112](DUAL_RADIO_REV112.md).
This implements the receive-side proposal in
[BLE_RESPONSE_TRANSFER_AUDIT.md](BLE_RESPONSE_TRANSFER_AUDIT.md). The wider
Marauder port and physical feature acceptance remain incomplete.

## Transfer behavior

Optional command 134 projects the same frozen C5 response bank with a compact
format version at offset 158 and response state at offset 159. The original
158-byte prefix is unchanged. The S3 reconstructs the existing full row before
its normal original/response validation and mandatory final snapshot fence.

| Frozen response state | Compact bytes | Data reads | Previous full-format reads |
| --- | ---: | ---: | ---: |
| UNKNOWN | 160 | 16 | 31 |
| NONE | 174 | 18 | 31 |
| PRESENT | 310 | 31 | 31 |

UNKNOWN reconstructs a canonical all-zero sidecar. NONE preserves the full
64-bit original sequence and canonical zero fields; it means no admitted
matching response, not proof that none occurred on air. PRESENT retains all
existing evidence. The producer checks omitted bytes before shortening a
record, projects existing immutable segments, and never rewrites its frozen
bank or the original command 133 representation.

The firmware gate requires 68 only for the optional compact command. Response
acquisition START and full response reads still require 67. INFO, protocol 2,
flags, RF scan modes, duration, acquisition defaults and family admission are
unchanged. Older peers return NOT_SUPPORTED locally before the unsupported
command is sent. The reader then discards all unpublished staging and rereads
the same frozen handle in full format. It retains the first-BEGIN time anchor
and does not restart scanning or begin another snapshot. Other errors cannot
trigger this fallback. Unsupported stays disabled across automatic sweeps and
is retried on a new explicit run.

Original and response metadata, raw previews, hints, RSSI and age bounds remain
separate. Stop retains priority; partial banks never publish. The existing
four-exchange/eight-ms tick budget remains, including its in-flight transaction
caveat. The change adds no radio mode, task, heap allocation or result bank.
Existing live selection and v6 SD evidence consume the reconstructed row.

## Validation

All 124 host suites passed with unchanged inputs. The focused tests use the
production C5 callbacks/service, S3 coordinator and transport gate:

- Three actual callback states in ordinary and focused response modes need
  65 compact reads instead of 93 full reads. Every omitted UNKNOWN/NONE field
  mutation, row/chunk bound and relevant ownership invalidation is checked.
- The reader fixture's 32 rows (11 UNKNOWN, 11 NONE, 10 PRESENT) need 684 reads
  instead of 992. Fallback and Stop are exercised at every one of those 684
  transfer positions, including after complete rows have entered staging.
- The end-to-end fixture's different 32-row mix (10 UNKNOWN, 10 NONE,
  12 PRESENT) needs 712 reads. Its reconstructed observations match the actual
  full-format projection of the same frozen handle, including SD capture data.
- Lost BEGIN replies, mutation during download, source/age preservation,
  malformed framing/body/fence, bounded retries, retained-bank atomicity and
  firmware 67/68 compatibility are covered. Error responses do not silently
  disable response acquisition or bypass validation.

All five native targets passed: S3 application, onboard C5, temporary bridge,
expansion WROOM and expansion XIAO. Their unchanged input maps match the host
run. Retained evidence is under `artifacts/rev113-host-evidence` and
`artifacts/rev113-native-evidence`. Independent implementation and evidence
reviews found no blocking issue.

The offline audit compares the retained final native images with the verified
installed rev112 images, excluding the earlier rev112 first-pass builds:

| Measured native resource | Rev112 | Rev113 |
| --- | ---: | ---: |
| S3 fixed data plus BSS | 173,612 B | 173,612 B |
| C5 fixed data plus BSS | 83,160 B | 83,160 B |
| S3 dual-BLE coordinator type | 21,496 B | 21,496 B |
| C5 optional response context | 11,736 B | 11,736 B |
| S3 dual-BLE tick entry frame | 448 B | 448 B |
| C5 snapshot-command entry frame | 240 B | 240 B |
| C5 SPI task entry frame | 160 B | 160 B |

The new control boolean fits existing native structure padding. Published
rows, wire scratch, frozen banks, archive types and configured task allocations
are unchanged. The new compact helper has 136 bytes of linked code; the audit
does not infer stack cost from an absent recognized prologue. Selected entry
frames are not complete call-chain bounds or runtime stack headroom. The
canonical omission scan is bounded to 152 bytes per compact request while the
C5 scanner mutex is held; its lock duration has not been measured on hardware.
The audit is retained as `artifacts/device-rev113/native-memory-comparison.json`.

These host/native results do not measure RF coverage or wall-clock latency.
Physical publication latency, dense mixed results, Stop/Back/next-scan
transitions, real-card save/reopen and sustained heap/stack headroom remain
pending. No speed multiplier is claimed.

All 95 mode assignments and six workstream statuses retain their prior values;
this transfer improvement does not establish full upstream feature parity.

## Matched local package

The package is
`dist/cerberus-unchained-0.1.0-dev.rev113-20261101T142106Z`.
Its hashes, chip headers, partition tables and image bounds pass verification.
The three applications match the retained full native matrix:

| Image | Application bytes | SHA-256 |
| --- | ---: | --- |
| S3 | 1,934,480 | `0d1372b2fb03fc0c34f4ced57e265198b0d0418e2804f5ce28d228137de2955b` |
| C5 | 1,678,960 | `c92425dc2a2a30e5d172a942ce305a3771314367c379b20888bc94f747200079` |
| Bridge | 255,296 | `a10efc28b6209e03a878275b3336b508715edb9d987c105e47e2b4269c022aa6` |

The source fingerprint is
`563cac3c796a0fd283a05c620e1132f110768f7bfd4c84157351e3a554625cac`;
the manifest SHA-256 is
`86d83fb5c944b548ff2795319d22c0e42d3c41e36cb5b16ab78ec7327ebb652f`.
`artifacts/device-rev113/local-milestone.json` binds the clean source commit,
package, host/native receipts and resource audit with `installed:false`.
No device access, flashing, PR, push or release was performed for this
checkpoint. Rev112 remains the installed baseline.
