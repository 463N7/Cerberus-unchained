# BLE SD snapshots and offline analysis

This guide describes the rev99 source candidate. [The rev99 checkpoint](DUAL_RADIO_REV99.md) records current build, installation and acceptance status separately. Earlier contribution evidence remains in [CONTRIBUTION_REV93.md](CONTRIBUTION_REV93.md); it does not establish physical acceptance of the new dual-receiver report.

Cerberus can save retained S3 and C5 Bluetooth observations in one SD report and reopen them for analysis after leaving the scan or rebooting. It supports the persistence outcome of saving received BLE evidence, including a selected Find My observation. It does not establish full Marauder Save/Load Airtags parity: Cerberus uses its own versioned `.BLE` format, does not import Marauder's JSON files, and never loads saved observations into live targeting, GATT or sound actions.

## Save an observation or retained list

Run a Bluetooth scan and select a row from the S3 list or the C5 companion pane. **SAVE SD** saves that page's frozen typed observation and original advertisement. The C5 selection also freezes its committed boot, epoch, generation and index when the row is painted/selected; a later bank or peer restart cannot lend newer provenance to the old row. A selection without the required copied evidence reports an error instead of fetching a different current row.

For the retained list, open **Bluetooth → SD SNAPSHOTS → SAVE BOTH RECEIVERS**. The menu also offers **SAVE SELECTED S3 ROW** and **BROWSE SD FILES**; save a selected C5 row from its own detail page. A report holds up to 64 source observations, at most 32 from each receiver. Either populated receiver may contribute when the other has no retained rows. Identical typed addresses heard by both remain two source-labelled observations, not two claimed physical devices.

Saving copies retained evidence at the request; the S3 inventory can include earlier sweeps, and the C5 contribution comes only from its fully published bank. The two sources are not claimed to be one simultaneous capture. Sort, source-pane and recent-filter presentation do not limit this save to the currently painted rows or make it a complete nearby-device capture. An entirely empty report or invalid contributing evidence reports an error.

The UI then waits for normal S3/C5 discovery-owner cleanup before submitting the file worker. It does not fetch new observations while waiting. Back cancels this pending save; a cleanup timeout reports an error without starting the worker. Once submitted, the worker owns an immutable copy and writes a new file such as `/CERBERUS/BLE/B00001.BLE`. Existing files are never silently overwritten. After completion the UI opens the exact saved document. Back during an in-flight filesystem save requests cleanup, but that save may still finish and publish its file.

## Open saved evidence

Open **Tools → SD Files**, enter `/CERBERUS/BLE`, and tap a `.BLE` file. Files copied or moved to another safe SD folder can also be opened there. A dedicated historical view provides a four-row paginated list, frozen selected details, raw advertisement bytes, and paginated advertising-data fields. Back returns to the original file browser location or Bluetooth page. The existing address privacy setting applies to the offline display.

The same browser opens new v2 reports and old v1 files. Old files show **LEGACY** receiver identity because v1 did not encode the receiver. The viewer never starts reception, connects to a peer or promotes a loaded address into the live selection. A failed or canceled load cannot partially replace a committed offline document. S3 rows show exact historical **age at save**; C5 rows show a conservative **upper bound at save**, or unknown. Opening after reboot never compares either value with the new boot's clock. No wall-clock date, GPS location, current presence or physical-device identity is inferred.

## What is retained

Each row includes its receiver and typed address, bounded peer-name projection, RSSI semantics/validity, manufacturer and UUID hints, peer flags, original raw ADV bytes/flags, report count, generation/index and available source metadata. S3 rows retain sweep-peak RSSI and exact historical source/copy times. C5 rows retain latest-original-ADV RSSI, the published or selected full-width C5 boot/epoch provenance, and an optional conservative age bound. C5 exact S3 source/copy times remain absent; an age bound may exceed saved S3 uptime and cannot be converted into an exact S3 packet timestamp. Unknown timing stays explicit.

Both archived sources retain at most **31 raw original advertisement bytes** per row. Raw scan-response bytes are absent. Responses or older reports may have contributed aggregate name/UUID/manufacturer hints; the archive does not claim those hints came from the saved ADV. A projected name may have lost bytes after an embedded NUL, and the C5 display projection is already sanitized. Full original name/extended-payload recovery, scan-response evidence and classifier-pointer structures are not provided by this format.

Malformed or truncated raw advertisement bytes remain inspectable. The viewer validates AD boundaries and labels malformed data; parsed field values remain unverified claims. Unknown flag bits are retained. CRC and schema validation detect corruption and incompatible structure, not authenticity. See [the format specification](BLE_ARCHIVE_FORMAT.md).

## Resource and failure behavior

The v2 binary is bounded to 64 rows and 9,248 encoded bytes; the reader still accepts v1's 32-row/4,128-byte format. The UI uses on-demand PSRAM for committed/incoming documents and copied selections. The worker owns a separate immutable PSRAM document and codec buffer, with no internal-RAM fallback, and requests a 6 KiB stack only for a job. Host type measurements are **10,768 bytes per document** and **20,784 bytes for the worker context**. These are not ESP32 heap/stack measurements or peak-runtime acceptance.

Copying a completed document pins the worker context, copies outside the interrupt-disabled critical section, and releases that reader pin afterward. Concurrent Release rejects additional readers and defers disposal and a replacement job until the already accepted copies finish. No worker borrows UI document memory, so leaving the screen cannot free its in-flight data.

Filesystem operations occur in the worker and hold the storage mutex through cleanup. Unmounted or USB-owned storage and conflicting capture, Wardriving or Recon recording are rejected. These operations have bounded byte counts, not a fixed filesystem latency. The UI does not wait on SD I/O.

Save creates an exclusive `.TMP` file, checks the complete write, flush, synchronization and close, and publishes `.BLE` only after those succeed. Errors retain partial files for inspection. Names are bounded to B00001–B99999, with at most 1,024 candidate names examined per attempt. There is no claim of power-loss atomicity or storage-medium durability beyond the reported successful calls.

Load checks safe paths, file type and size, complete reads, EOF, close, version, exact encoded length, reserved fields, CRC, per-source row limits, receiver+typed-address duplicates and receiver-specific timing/provenance rules before publication. Back requests cancellation and deferred worker cleanup.

The read-only `ble_archive` console command reports the worker's state, job ID, ownership/result flags, error, complete byte count, row count and file path. It cannot initiate a save, load or radio operation.

## Validation scope

Focused host checks exercise the codec, capture mappers, offline viewer, UI adapter, worker and storage routines. The codec retains independent v1 and v2 golden vectors, full 64-row validation, source collisions and failed-decode atomicity. C5 mapping preserves copied provenance and checks unknown/backward/overflow age bounds. Worker checks inject concurrent Release and nested copies during the actual out-of-lock memcpy, plus allocation/task failure, cancellation, stale jobs and file faults. Prior viewer/SD checks retain their revision-specific scope; the checkpoint records the completed rev99 validation set and rendered reviews.

Physical acceptance remains pending: save selected S3/C5 observations and both retained banks, inspect each source's RSSI/time labels, reopen an old v1 file, reboot and reopen v2, and repeat save/load and return-to-scan transitions. Real-card failure recovery, power interruption and prolonged operation are not established by host fault injection. Source-specific filtering, broader history/session recording, complete raw/scan-response capture, family parity, AP/SSID persistence and earlier Wi-Fi feature errors remain separate work. GPS and expansion radios are not required for this workflow.
