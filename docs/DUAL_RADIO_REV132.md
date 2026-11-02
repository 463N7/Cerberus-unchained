# Rev132: saved Mesh observation maps

Development source following installed rev131. The owner has accepted rev131's
menu-first Mesh entry and Back controls. Rev132 implements native saved-map
reopening; final host/native matrix, packaging and physical acceptance are
pending at this source checkpoint. No complete Mesh/CYM/Marauder parity claim
is made. The [implementation plan](MESH_ARCHIVE_PLAN.md) records its boundaries.

## User workflow

The existing five-tool Mesh menu stays intact. Inside the Mesh survey, SAVE MAP
or STOP & SAVE saves a new `.MSH` file, then OPEN MAP opens the historical view.
SAVED opens `/CERBERUS/MESH` in the existing Files browser when acquisition and
storage work are released. Files also recognizes copied `.MSH` documents.

The historical view offers PAN contexts, addresses, directed observed traffic
and retained raw-frame details. Its selected rows are frozen copies, and the
existing address privacy setting also hides raw bytes. Back from a detail
returns to the map; Back from the map returns to Files. It exposes no Start or
Locate action. Historical source errors, partial channel coverage, clock faults
and saturated C5 counts remain visible independently of successful file loading.

EXPORT CSV creates the existing Mesh report schema under `/CERBERUS/LOGS`, then
offers OPEN CSV. Existing CSV and Locate report codecs remain supported. Failed
loading preserves the previous historical document; Back or sleep cancels and
releases only the view's exact storage job. A save already inside a filesystem
write reports its actual outcome, rather than implying rollback.

## Evidence and processor roles

C5 retains the existing passive receive, bounded FIFO and exact Stop/drain/
release path. No new C5 command, receiver task or capability revision is needed;
the onboard capability remains 73. S3 validates and copies only released frozen
evidence into a separate historical model, then uses the existing serialized SD
worker. Saved session identifiers describe the old capture and confer no live
radio authority.

The archive persists every retained PAN/address/edge aggregate and final source
summary, plus the first 64 raw witnesses. Counts and observations after those
first 64 records are preserved directly, with explicit omissions. Replaying the
raw subset cannot reconstruct them. The map represents observed MAC addressing,
not authenticated devices, actual delivery, network membership or routing.

Version 1 uses explicit little-endian fields, a 32-byte header, parser revision
1, exact count/length checks, canonical flags/reserved bytes and CRC32. Its
encoded length is 268..18,332 bytes. CRC detects corruption, not authenticity.
Failed decode leaves the destination unchanged and uses explicit caller-owned
scratch instead of a large stack object. Live CSV and historical CSV share a
borrowed evidence validator/renderer; the live wrapper still requires its
original frozen/released controller boundary.

Save/load/CSV operations have separate optional PSRAM allocation arms. Existing
BLE, Wi-Fi, interchange, Probe, Mesh CSV and Locate arms retain their own sizes.
The live Save conversion briefly overlaps an archive copy with the live UI and
save worker. OPEN MAP may overlap the old stopped UI, historical UI and load
worker until the next UI cleanup tick. Native allocation/frame measurements
must account for those transitions; host structure sizes alone are insufficient.

## Validation checkpoint

Focused codec checks cover empty/full/late-aggregate captures, typed identities,
known PAN zero versus unknown, unknown clocks, source errors, all truncations,
every-byte CRC corruption, semantic corruptions with a repaired CRC and alias/
atomicity boundaries. Independent Python bytes verify an empty golden document
and the complete maximum layout. The historical export matches the
pre-refactor CSV renderer byte-for-byte for 0, 80 and 1,000 records. Archive
checks also reject contradictory callback times while preserving valid zero
and first-valid timestamps arriving after the retained raw prefix.

Focused worker/storage checks exercise actual asynchronous publication, exact
typed jobs and result pins, release/reentry, cancellation, exclusive filenames,
short writes, flush/sync/close/rename failures, bounded EOF and input/output
alias rejection. The UI fixture uses the actual production controller/model,
codec and route excerpts with a synthetic framebuffer. It checks more than 64
observations, historical details/privacy, retained-map errors, CSV export,
Back/sleep cleanup, and the absence of live controls. Host-rendered previews
are not photographs or touchscreen acceptance.

The registry now has 151 host suites. Full matrix, native resource audit,
matched package verification and real SD save/reopen remain pending. A build or
successful flash alone will not establish physical reception, loaded memory,
storage durability or complete feature parity.
