# Rev109: saved C5 full-original family evidence

This local candidate carries rev108's decoded C5 family evidence through SD
save, reopen and offline analysis. Rev106 is the last verified installed
build. Rev109 has not been flashed or accepted on the physical device.

## Evidence and file contract

The v5 archive appends the canonical 48-byte C5 family record to each existing
216-byte v4 row. Its header remains 32 bytes; rows are 264 bytes, and the
maximum 64-row report is 16,928 bytes. There are at most 32 observations per
receiver. All earlier field offsets and meanings are preserved, including
S3 matched-response evidence and C5 receive metadata. The checksum covers the
whole encoded file and detects corruption, not authenticity.

The saved family record belongs to the same frozen C5 original as the raw
preview, receive envelope, typed address, receiver boot and full scan epoch.
An invalid nonzero record is rejected rather than discarded or replaced with
a prefix-only interpretation. Canonical unavailable evidence remains distinct
from a valid advertisement with no supported family hint. S3 and LEGACY rows
cannot carry a C5 family record.

The reader continues to accept v1, v2, v3 and v4. Missing fields remain
explicitly unavailable; neither old prefixes nor a saved address reconstruct
missing full-original evidence. Existing files are not rewritten when opened.
Encoding and decoding remain transactional, bounded and independent of native
struct padding. Details are in [the portable format specification](BLE_ARCHIVE_FORMAT.md).

## Capture, storage and display

Saving a selected C5 row copies its already frozen proof, including family
evidence. Saving both receivers takes only the committed banks and keeps
receiver identity separate. Subsequent advertisements, family changes or
receiver restarts cannot replace the report queued for saving.

The existing worker owns an immutable PSRAM document and encoded buffer. Its
storage limit grows with the file format; cancellation, SD ownership, close,
flush and rename handling retain their existing contracts. No additional
radio task, observation bank or per-advertisement allocation is introduced.

Saved C5 details add HINTS beside RAW and RX INFO. The hint body is shared with
the live view without depending on a live owner, receiver, filesystem or
current clock. Original and response evidence retain their own meanings.
Saved ages are historical ages at save, never current freshness. Every
supported family and retained Meta identifier/context pair remains reachable.

The file preserves decoded full-original evidence; it does not preserve a
complete 255-byte raw payload. The preview is still limited to 31 bytes.
Offline reports do not create a live target, connection permission or active
operation. The C5 contributes the source evidence during reception; opening
the report later does not start either radio.

## Validation and remaining scope

All **124 host suites** and **five native targets** pass against unchanged,
matching inputs. The targets are S3, onboard C5, temporary bridge, expansion
WROOM and expansion XIAO. Exact receipt-bound logs and native ELFs are retained
under `artifacts/rev109-host-evidence` and `artifacts/rev109-native-evidence`.
The two existing unused Wi-Fi relation helper warnings remain in S3/bridge;
there are no new compiler warnings in this checkpoint.

The production codec passes 8,323,571 assertions, including independent v5
byte vectors and unchanged v1-v4 compatibility vectors. Checks cover all six
families beyond the preview boundary, 64 mixed rows, repaired-checksum hostile
fields, complete preflight and failed-operation atomicity. The unchanged S3
capture suite passes 2,523 assertions; C5 capture preserves frozen source proof.

Actual filesystem fixtures pass 362,879 checks across 48 isolated cases.
Injected write, flush, sync, close and rename failures preserve the previous
published report. The asynchronous worker passes 17,116 checks; live capture,
serialization and reopening pass 1,270. A saved full-255-byte HC-05 decoded
hint survives live HC-06 replacement, changed receiver boot/bank/family and
reopening with a zero loading clock. Rendering makes no clock or radio calls.

The saved viewer passes 12,608 assertions and its list helper 170,281 checks.
Six actual-renderer previews were inspected, then their unchanged image hashes
were bound to the final host receipt. They cover saved full-original hints,
frozen selection, extreme historical ages, the last Meta pair, old-file
unavailable evidence and the updated live save label. These are synthetic
renderer fixtures, not hardware screenshots or touchscreen acceptance.

Measured native layouts compare retained rev108 and rev109 ELFs:

| Measurement | Rev108 | Rev109 | Change |
| --- | ---: | ---: | ---: |
| Archive row | 240 B | 288 B | +48 B |
| Archive document | 15,376 B | 18,448 B | +3,072 B |
| Optional viewer store in PSRAM | 32,376 B | 38,768 B | +6,392 B |
| Optional worker context in PSRAM | 30,000 B | 36,144 B | +6,144 B |
| Viewer and worker together | 62,376 B | 74,912 B | +12,536 B |
| S3 internal data + BSS | 172,828 B | 172,828 B | 0 B |
| C5 data + BSS | 83,152 B | 83,152 B | 0 B |
| Decode entry frame | 320 B | 368 B | +48 B |
| Saved viewer draw entry frame | 240 B | 272 B | +32 B |

The viewer and worker can coexist during SAVE/LOAD; completed worker storage
persists until release, and pinned readers can defer its free. Documents are
embedded in those stores and are not added again to the combined total. The
PSRAM sum excludes allocator overhead, fragmentation and other live stores.
The companion, live discovery and Analyzer store sizes are unchanged.

The existing worker requests a separate 6,144-byte task stack; its entry frame
remains 48 bytes. The main task allocation remains 8,192 bytes with a
1,136-byte UI entry frame. Encode increases from 48 to 64 bytes. The shared
hint renderer has a 176-byte entry frame; the live detail wrapper decreases
from 400 to 352 bytes, but now calls that helper. These measurements must not
be interpreted as a reduction in total nested-call stack use or subtracted
from task allocations to claim runtime headroom. Task-control and allocator
overhead are separate. No new task or observation bank is introduced.

The native measurement audit and independently reviewed lifetime accounting
are retained in `artifacts/device-rev109/native-ui-comparison.json`. Its pass
status certifies evidence binding and measurement checks, not runtime memory
safety. Host fixtures cannot establish physical SD durability, RF reception,
touchscreen behavior or runtime heap headroom.

Physical selected/both-receiver save/reopen, rebooted historical reports,
family changes and return-to-scan checks remain pending. Full raw-payload and
session-history recording, C5 response acquisition, extended-format reception
acceptance, sustained load and full Marauder feature parity remain incomplete.
GPS and the six-radio backboard are not required for this report workflow.

## Matched local package

Implementation commit `f2253f2ea7c84766344da0ab7c3a9108764abcf2` is retained
locally. Package
`dist/cerberus-unchained-0.1.0-dev.rev109-20261101T114044Z` passes hash, chip
header, partition and image-bound verification. Its S3, onboard C5 and bridge
application hashes match the retained five-target native matrix. Package
source hashes match the final host/native receipts and the clean implementation
checkout.

- Source SHA-256: `ec58013cc58c511e28aa0d540bc54ba09b9dd10955498e7d6eb9261b1a8c287b`.
- Manifest SHA-256: `8c90136fc7028162f862d906d040522e520e3f3bc6e071c94a186fd18f999a08`.
- S3 application: 1,919,136 bytes; C5: 1,672,176 bytes; bridge: 255,296 bytes.

`artifacts/device-rev109/local-milestone.json` binds the exact implementation
commit, package, retained ELFs and unchanged-input validation. Its `installed`
field is false. Rev106 remains the last verified installed baseline; rev107,
rev108 and rev109 are local candidates. This checkpoint did not access the
device, flash firmware, push commits, open a pull request or select a public
installer release.

The next device acceptance should use a matched paired update, then selected
and both-receiver saves, reopening after leaving discovery or rebooting,
historical HINTS/RAW/RX INFO navigation, Stop/Back and a following normal Wi-Fi
scan. Those checks require the physical device and remain pending while the
user is unavailable. The checkpoint closes the decoded-family SD persistence
gap; full Marauder feature parity and sustained dual-radio acceptance remain
unfinished.
