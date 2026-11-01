# Rev99: one saved BLE report from both receivers

Rev99 extends the existing passive BLE SD archive to observations from both
processors. All 120 host suites and five native targets pass on matching,
unchanged inputs. Full Marauder parity and physical SD/receive acceptance remain
open. The installed rev98 checkpoint remains distinct from the current
candidate until installation evidence is added below.

## Behavior and provenance

SAVE BOTH RECEIVERS freezes up to 32 retained S3 observations and 32 committed
C5 observations into one report. The same typed address heard by both radios
remains two source-tagged observations. These are retained records from their
own receive windows, not a simultaneous capture or a count of unique physical
devices. Saving a C5 selection uses the exact painted row and its frozen boot,
epoch, generation and index; a later sweep cannot replace its provenance.

The version2 writer retains S3 sweep-peak RSSI and available exact historical
S3 source time separately from C5 latest-ADV RSSI and conservative age at save.
C5 age is an upper bound, never a fabricated S3 packet timestamp. Aggregate
peer metadata remains distinct from the retained original raw ADV. Malformed
and truncated byte previews remain available for inspection. Raw scan-response
payloads, full extended reports and C5 family-priority retention remain gaps.

The existing SD browser opens both new v2 and old v1 files. Old files identify
their unencoded source as LEGACY. Loaded observations remain historical and
cannot become live targeting or connection state. The current bound is a
32-byte header plus 64 rows of 144 bytes: 9,248 encoded bytes. Files still use
`/CERBERUS/BLE/Bnnnnn.BLE`; no second browser or storage namespace is added.

## Lifecycle and memory

The UI freezes the report on Save, requests ordinary S3 and C5 discovery
cleanup, and continues their normal owner ticks. The SD worker starts only
after both receivers release. Back, sleep, an invalid clock or a 12-second
release timeout cancels a pending submission without discarding radio cleanup.
An already running SD save retains its existing outcome semantics: Back cannot
promise to roll back a completed filesystem operation.

The larger completed archive document is copied outside the interrupt-disabled
critical section. A bounded reader pin keeps its immutable worker context alive
through copying. Release rejects new readers and defers disposal and replacement
jobs until all accepted copies finish. No permanent worker or internal-RAM
fallback is introduced; the existing transient 6 KiB worker stack and memory
admission checks remain.

The actual S3 ELF confirms a 10,768-byte document, 20,784-byte transient worker
context and 22,728-byte optional archive UI store, all allocated in PSRAM.
The optional C5 reader grows from 7,504 to 7,512 bytes. Linked S3 internal data
remains 26,412 bytes and BSS grows from 145,904 to 146,032 bytes: 128 additional
fixed bytes, including 120 bytes of frozen painted/selected C5 provenance and
the reader pin. C5 linked internal data/BSS are unchanged at 19,552/59,808 bytes.
These are linked layouts, not live heap measurements or a whole-program stack
bound. Evidence is retained under `artifacts/device-rev99`.

Native disassembly confirms the full 10,768-byte result copy occurs between
critical-section exit and re-entry. The worker's own frame remains 48 bytes;
selected nested encode, decode and save/formatting chains measure 240, 400 and
1,344 bytes respectively, including the task wrapper. These are partial chain
measurements against the existing 6,144-byte allocation, not exhaustive bounds
or runtime stack headroom. The worker context and UI store together use 43,512
bytes of optional PSRAM, 23,680 more than rev98 before allocator overhead.

## Verification

All 120 host suites passed with pinned Python 3.14.2 and Zig 0.14.1 on unchanged
inputs. The receipt and individual logs are retained under
`artifacts/host-checks/rev99-full-run`. Focused tests cover v1 compatibility,
independent golden bytes, v2
64-row source limits, corruption and malformed metadata, unchanged output on
failure, C5 age/provenance mapping, actual production receiver publication,
storage faults, cancellation, concurrent Release and nested result readers.
The codec fixture completed 2,724,148 assertions, the actual storage fixture
189,958 checks, the async worker fixture 15,013 checks, the archive controller
703 checks, and the viewer 1,325 checks. These counts describe fixture work,
not independent hardware trials.

The S3, onboard C5, temporary bridge and both optional expansion variants pass
the full native matrix with the isolated ESP-IDF 6.0.1 toolchain. Its input
fingerprints match the host receipt and current sources. The complete native
receipt, logs, actual S3/C5 ELFs, S3 map and effective SDK configurations are
retained under `artifacts/rev99-final-native-evidence`. The matched package is
`dist/cerberus-unchained-0.1.0-dev.rev99-20261101T063102Z`; all three packaged
application hashes match the full-matrix images. Package hashes, chip headers,
partitions and image bounds pass verification, and the firmware source digest is
`f9c164b9695af09cb489cc9c337e651f299355e4b9670e19848167071ac18c6a`.
Installation is pending; expansion builds do not establish expansion-board
hardware acceptance.

## Deferred physical checks

- Save a report after both BLE receivers have observations. Reopen it from SD
  and confirm S3/C5 counts, source labels, peak/latest RSSI and exact/bounded age.
- Select a C5 row, allow another sweep, then save that selection. Confirm the
  saved row remains the selected identity and historical observation.
- Use Back while waiting for receiver release, then run a split Wi-Fi scan.
- Reboot and reopen a saved report; its historical age must not become current
  age. An older v1 archive should remain readable as LEGACY.

No GPS or expansion board is required for this workflow. Native builds and
host fault injection do not establish SD durability, real-card recovery,
physical response time, sustained coexistence or full upstream parity.
