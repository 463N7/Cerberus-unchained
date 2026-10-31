# BLE SD snapshots and offline analysis

This guide describes the contributed source. New workflow acceptance on physical hardware remains pending; the installed local Unified checkpoint and the unflashed Unchained adaptation are distinguished in [CONTRIBUTION_REV93.md](CONTRIBUTION_REV93.md).

Cerberus can save retained Bluetooth observations to SD and reopen them for analysis after leaving the scan or rebooting. This covers the persistence part of Marauder's reachable Save/Load Airtags workflow and extends it to ordinary BLE observations. Cerberus uses its own versioned `.BLE` format; it does not import Marauder's JSON files or load saved observations into live targeting, GATT or sound actions.

## Save an observation or retained list

Run a Bluetooth scan and select a row. **SAVE SD** appears alongside the Back control on the selected device's page. It saves the frozen typed address and retained original advertisement that belong to that selection. A selection without frozen raw evidence reports an error instead of fetching a different current row.

For the retained list, open the Bluetooth tools menu and choose **SD SNAPSHOTS → SAVE RETAINED LIST**. The same screen also offers **SAVE SELECTED DEVICE** and **BROWSE SD FILES**. The list contains up to 32 retained typed observations from the ordinary discovery inventory; it can include earlier sweeps and is not guaranteed to represent one simultaneous bank. Current focus, sort and age presentation do not turn it into a complete capture of nearby traffic. Empty or unavailable inventory reports an error.

Saving copies the data before returning to the UI. The worker writes a new file such as `/CERBERUS/BLE/B00001.BLE`. Existing files are never silently overwritten. The UI shows a saving state and, after completion, opens the exact saved document. Back cancels waiting and requests cleanup; an in-flight save may still finish and publish its file.

## Open saved evidence

Open **Tools → SD Files**, enter `/CERBERUS/BLE`, and tap a `.BLE` file. Files copied or moved to another safe SD folder can also be opened there. A dedicated historical view provides a four-row paginated list, frozen selected details, raw advertisement bytes, and paginated advertising-data fields. Back returns to the original file browser location or Bluetooth page. The existing address privacy setting applies to the offline display.

The viewer never starts reception, connects to a peer or promotes a loaded address into the live selection. A failed or canceled load cannot partially replace a previously committed offline document. The display labels source times as historical S3 boot uptime and shows **age at save**, not age against the current boot's clock. No wall-clock date, GPS location, current presence or physical-device identity is inferred.

## What is retained

Each row includes its typed address, bounded peer-name projection, sweep-peak RSSI with validity, manufacturer and advertised UUID projection, peer flags, original raw ADV bytes and flags, report count, generation/index, and available scan-epoch/source-time metadata. Original source time, copy ceiling and save time remain distinct. Selected observations from a path without full-width source timing retain explicit unknown timing.

The current receiver retains at most **31 raw original advertisement bytes**. It does not retain raw scan-response bytes. Responses may have contributed to the peer's name or UUID projection, but the archive stores no response payload or classifier-pointer structure. A projected name may have lost content after an embedded NUL during the earlier scanner conversion; it is not claimed to be the complete on-air name. Displayed RSSI is the original-report sweep peak, independently labelled from the saved raw packet timestamp.

Malformed or truncated raw advertisement bytes remain inspectable. The viewer validates AD boundaries and labels malformed data; parsed field values remain unverified claims. Unknown flag bits are retained. CRC and schema validation detect corruption and incompatible structure, not authenticity. See [the format specification](BLE_ARCHIVE_FORMAT.md).

## Resource and failure behavior

The binary format is bounded to 32 rows and 4,128 encoded bytes. The UI uses an on-demand PSRAM store containing the committed document, incoming scratch document and copied selections. A separate worker owns an immutable PSRAM document and encoding buffer, with no fallback to internal RAM. It requests a 6 KiB stack only for a save/load job. Native structure sizes and static RAM deltas are measured in the installation evidence; they do not establish peak runtime memory or stack high-water marks.

Filesystem operations occur in the worker and hold the storage mutex through cleanup. Unmounted or USB-owned storage and conflicting capture, Wardriving or Recon recording are rejected. These operations have bounded byte counts, not a fixed filesystem latency. The UI does not wait on SD I/O.

Save creates an exclusive `.TMP` file, checks the complete write, flush, synchronization and close, and publishes `.BLE` only after those succeed. Errors retain partial files for inspection. Names are bounded to B00001–B99999, with at most 1,024 candidate names examined per attempt. There is no claim of power-loss atomicity or storage-medium durability beyond the reported successful calls.

Load checks safe paths, file type and size, complete reads, EOF, close, format version, exact encoded length, reserved fields, CRC, typed-address duplicates and source-time ordering before publication. Back requests cancellation and deferred worker cleanup. A worker never borrows UI document memory, so leaving the screen cannot free an in-flight file operation's data.

The read-only `ble_archive` console command reports the worker's state, job ID, ownership/result flags, error, complete byte count, row count and file path. It cannot initiate a save, load or radio operation.

## Validation scope

Focused tests exercise the actual codec, capture mapping, offline viewer, UI job adapter, worker and storage routines. They include raw-byte round trips, repaired-checksum hostile metadata, incomplete and corrupt files, filesystem faults, concurrent storage ownership, allocation/task failure, cancellation, stale jobs, frozen selection, historical clocks and Back navigation. Rendered previews cover the list, detail, raw data, malformed data, saving, error and unavailable screens.

Physical acceptance remains pending: save one selected observation and a retained list, inspect both from SD, reboot and reopen a saved file, and repeat save/load without resetting. Real-card failure recovery, power interruption and prolonged operation are not established by host fault injection. Full Marauder parity, AP/SSID persistence, remaining Bluetooth suboptions and earlier Wi-Fi feature errors remain separate work. GPS and expansion radios are not required for this workflow.
