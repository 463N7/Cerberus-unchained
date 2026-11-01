# Rev111: single C5 response dispatch per receive callback

Rev111 follows [rev110](DUAL_RADIO_REV110.md), which implemented optional C5
response acquisition, source-separated live analysis and v6 SD evidence. This
checkpoint removes a redundant mode check from complete passive EXT callbacks
and corrects older coverage descriptions that no longer matched that code.

The C5 EXT callback dispatches the response-aware handler before response or
partial-report early returns. The legacy callback performs that dispatch in
its own wrapper after constructing legacy metadata. The common original
copier no longer repeats it. Each path checks once, preserving the newer-
original invalidation boundary and existing session/context checks.

This removes one unnecessary helper call and mutex take/give from each
complete passive EXT callback. RF intervals, windows, PHY choices, response
matching, task count and wire protocol are unchanged. C5 capability firmware
remains67; application version is `0.1.0-dev.rev111`. No measured radio speedup
is claimed.

All 124 host suites and five native targets passed with unchanged, matching
inputs. The actual callback fixtures verify one response-mode predicate call
on each legacy/EXT route across passive Discovery and response Discovery/Family.
The ordinary owner-free complete EXT path takes three mutex round trips,
compared with four in the previous code. The new regression fails against the
previous actual callback as expected. Partial, malformed, null-data and stale
callback cases remain covered; this is not a source-text-only assertion.

Retained evidence is under `artifacts/rev111-host-evidence` and
`artifacts/rev111-native-evidence`. The native audit compares against installed
rev109; `artifacts/device-rev111/rev110-native-comparison.json` also compares
the immediately preceding local rev110 images. Native types, fixed S3/C5 data
and BSS, and the optional allocation sizes are identical to rev110. Selected
C5 entry frames remain 64 bytes for GAP, 320 for the common original copier,
448 for response processing and 240 for snapshot commands. The unnecessary
nested call is removed, but entry frames do not prove whole-chain or loaded
runtime stack headroom.

The matched package is
`dist/cerberus-unchained-0.1.0-dev.rev111-20261101T125257Z`. Its hashes, chip
headers, partitions and image bounds pass verification. The local milestone
receipt binds its application images to the native matrix and exact source
commit before installation.

Rev109/C5 firmware66 is the verified installed baseline at the start of this
checkpoint. Rev110 was a verified local package, not an installed rollback
baseline. Fresh backups must match rev109 before a new write; installation
and subsequent boot evidence are recorded separately from local packaging.

The wider port remains incomplete. Physical response delivery, loaded memory,
repeated UI transitions, saved-file acceptance, GPS and six-radio expansion
remain separate from builds and source review. No PR or release is published
as part of this checkpoint.

The next planned receive-side work is the
[dual-receiver Fox Hunt usability and cleanup milestone](BLE_DUAL_FOX_AUDIT.md).
That source audit identifies independent-source state/retry and retained cleanup
improvements; Rev111 does not implement them or change Fox Hunt's RF behavior.
