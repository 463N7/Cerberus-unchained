# Rev100: matched response evidence and bounded C5 discovery

Rev100 is a source candidate. All 120 host suites pass on unchanged inputs;
all five native targets pass and installation is pending. Rev99 remains installed;
its boot and pending physical checks are recorded in the prior checkpoint.
The full Marauder port remains incomplete.

## S3 response evidence

The S3 retains up to 31 bytes of the latest scan response admitted by its
existing typed-address, generation, sequence, SID, completeness and two-second
binding checks. Delivered length remains separate from retained length. Raw ADV
and response bytes, clocks and sequences never replace one another. An explicit
raw-original sequence prevents a newer response from attaching to old raw bytes
after a same-millisecond or invalid original replacement. Existing RF mode and
active eligibility rules are unchanged.

Frozen discovery selections now retain the full scan epoch as well as their
original time and copy ceiling. The archive mapper validates that exact frozen
original before attaching response evidence. Legacy selection paths without
full clock/session proof remain explicitly unknown rather than borrowing a newer
session. UNKNOWN means unavailable correlation or bytes; NONE means no admitted
matched response for this proven original; PRESENT means retained matched bytes.
NONE is not proof that no response was transmitted.

The v3 archive uses 208-byte wire rows and a 13,344-byte maximum file, still at
most 64 observations and 32 per receiver. Existing v1/v2 files remain readable
with response state UNKNOWN. C5 observations remain response-unavailable. The
same raw viewer switches between advertisement and response, retaining bounded
hex/AD-field inspection, independent historical response age and explicit
truncation. No second browser or live targeting state is created.

## C5 inventory admission and reporting

Only v2 dual-receiver DISCOVERY enables the new full-bank policy. It keeps 32
slots and replaces the least recently qualified original with a complete,
independently valid new original. Malformed/partial reports, invalid typed
identities, stale epochs, backward clocks and exhausted ordering counters cannot
cause replacement. The victim is fully cleared. Legacy scans, v1 sessions and
TRACK retain their prior admission behavior; reception mode is unchanged.

Replacement and capacity-drop counts freeze with each immutable snapshot.
Optional command 129, gated at onboard firmware 63, carries the same handle and
count as the rows. The S3 publishes counters with that bank only after its final
snapshot fence. Unsupported or failed metadata stays unknown. Retries remain
bounded and Stop overrides them. These counts describe admitted replacements
and rejected reports at capacity, not unique physical devices or RF packet loss.
The C5 list and cached diagnostic console show the retained bank's counters.

## Memory and validation scope

Linked native data/BSS increases by **96 bytes on S3** and a net **160 bytes on
C5**, measured against retained rev99 ELFs. The C5 admission state is 160 bytes
and its frozen metadata grows eight bytes; selected-symbol changes are not the
same as the net section change. Evidence is in
`artifacts/device-rev100/native-memory-comparison.json` and
`artifacts/device-rev100/c5-native-memory-comparison.json`.

Native S3 optional PSRAM objects measure 10,192 bytes for the scanner snapshot,
19,792 for the discovery live store including inventory/staging/selection, and
2,084 for focused bindings. Snapshot plus live-store growth is 4,040 bytes;
focused bindings add 320 more. The companion store is 7,544 bytes (+32). The
archive document is 14,864 bytes, its transient worker context 28,976, and its
UI store 31,240. Worker plus UI therefore uses 60,216 bytes of optional PSRAM
objects (+16,704), excluding allocator overhead and other retained objects.
These are native type sizes, not live heap measurements. No permanent task or
additional full scan bank was introduced.

Selected native stack inspection found the S3 collector frame grows 368 to
400 bytes; one inspected NimBLE-to-focused-parser frame subtotal grows 1,456
to 1,488 bytes within the configured 4,096-byte task stack. C5 callback and SPI
dispatcher frames are unchanged; the new validator uses 48 bytes and command
129 uses 64. Archive worker encode/decode selected subtotals are 288/512 bytes,
with its 6,144-byte stack unchanged. These are partial frame sums, not complete
NimBLE/SPI/storage/ROM/interrupt or error-path bounds, and no runtime stack
high-water acceptance is claimed. Full measurements and limits are in
`artifacts/device-rev100/s3-response-native-audit.json`,
`artifacts/device-rev100/c5-admission-native-stack.json` and
`artifacts/device-rev100/s3-archive-native-stack.json`.

Focused checks cover actual ordinary/focused response admission and scanner
copy paths, every response length 1–255, equal-millisecond invalidation, malformed
input, typed identity/SID, clocks, frozen selections, v1/v2/v3 compatibility,
failure atomicity, SD-worker lifecycle, UI selectors and optional metadata faults.
The C5 production-to-production fixture verifies that replacement during a frozen
transfer cannot change its rows/counters or a previously copied observation.
Rendered response/detail/unknown/truncated screens were inspected at 320×480.
Focused assertion counts are fixture work, not hardware trials.

The complete 120-suite host run passed with pinned Python 3.14.2 and Zig 0.14.1.
Its unchanged-input receipt and all logs are retained in
`artifacts/host-checks/rev100-full-run`. Shared UI harnesses run sequentially in
that matrix. Focused tests report 5,322,664 codec assertions, 2,523 S3 capture
assertions, 273,048 storage checks, 15,013 worker checks, 784 archive controller
checks and 1,537 viewer assertions. Those counts do not measure physical RF or SD
behavior. S3, onboard C5, S3-to-C5 bridge, expansion WROOM and expansion XIAO
native targets all pass; the current receipt is
`artifacts/integration/build-report.json` with build output in
`artifacts/build/rev100-native-all.log`. Installation and physical acceptance
remain pending.

## Pending physical acceptance

- Capture a controlled advertiser with distinct ADV and response contents, save
  both receivers, and reopen the report. Check separate bytes and historical ages.
- Select an S3 observation, allow live state to change, then save the selection.
  Its original/response pair and scan epoch must remain the selected observation.
- Exercise C5 discovery with more than 32 controlled advertisers; check replacement
  counters and later-arriving rows without claiming a measured RF improvement yet.
- Repeat Stop/Back/save/reopen and a following split Wi-Fi scan. Existing rev98/99
  physical acceptance, prior Wi-Fi errors, GPS and expansion-board checks remain open.

The upstream develop branch was rechecked at
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363`; no new upstream revision was observed.
