# Rev123: retained Find My JSON interchange

This local candidate adds Marauder-compatible retained-list import, offline
inspection, copy save and export from Cerberus BLE snapshots. The source
version is `0.1.0-dev.rev123`; C5 capability revision remains 69. Rev113 is
the last verified installed build. Source implementation, focused codec and
worker/storage and UI checks, and independent reviews are complete. The first
complete 132-suite host run passed on 1,096 unchanged inputs. All five native
builds, the linked resource audit and matched package verification pass. Rev123
remains a local, unflashed candidate; these are software/package checks.

## Reference and scope

The pinned reference is justcallmekoko/ESP32Marauder develop
[`79cc6bcb`](https://github.com/justcallmekoko/ESP32Marauder/tree/79cc6bcba1db6679baee1dab2b0c1ad5010e6363).
At 2026-11-01 19:45:27 UTC a read-only GitHub comparison still reported develop
identical to this revision; the response is retained in
`artifacts/device-rev123/marauder-develop-check.json`.

`RunSaveATList()` and `RunLoadATList()` in `esp32_marauder/WiFiScan.cpp`
save and append a root JSON array containing `mac`, `payload` and
`payload_size`. The pinned `esp32_marauder/utils.h` defines
`byteArrayToHexString()` (line 91) and `hexStringToByteArray()` (line 111);
these helpers encode/decode space-separated `0xXX` byte tokens. Upstream loading uses
`/Airtags_0.log`; it does not record address type, receiver, RSSI, timestamps,
source epochs or whether bytes came from an advertisement or a response.
The original retained list may include Find My, FMNA and DULT observations.

The Cerberus codec and integration are original implementations of this
interchange schema. Existing license notices remain unchanged. An imported
MAC is untyped and is not coerced into a public address or a v6 LEGACY row.
Imported records are historical file data; importing them does not populate
live discovery, connect to a device, or authorize an action. This distinction
also means offline interchange alone is not full upstream live-list parity.

## Workflow

An explicit file action opens `.LOG` or `.JSON` as an AirTag JSON list. Normal
file opening keeps its existing behavior. The viewer preserves file order,
duplicate MAC records, empty payloads and opaque bytes. Its details identify
missing metadata as unknown and label bytes as imported payload, without
inventing ADV/RSP provenance. Paging and selection use the committed model;
address privacy also masks raw bytes. Save Copy writes a new file.

A validated native `.BLE` report can be exported through a separate
confirmation explaining the format's metadata loss. Each complete qualifying
original or matched response is considered separately, in row/ADV/response
order. Complete raw Find My evidence must be present in the saved packet;
decoded hints cannot reconstruct missing bytes. Truncated, malformed,
unavailable or insufficiently proven packets are omitted with explicit counts.
Duplicate MAC records remain distinct. More than 64 eligible packets fails
atomically rather than publishing a truncated list.

The native `.BLE` format and its v1-v6 readers remain separate. Export creates
a compatibility copy and does not replace the richer original report.
Imported unknown extension members are validated but not retained; canonical
copy saves preserve the three schema fields, record order and payload bytes,
not the source file's whitespace, case or unknown extensions.

## Resource and ownership contract

The interchange model has explicit Cerberus bounds: 64 records, 8,192 total
payload bytes and 49,152 JSON bytes, with bounded JSON nesting. These are
resource limits, not claims about BLE or upstream format maxima. Oversized or
malformed input is rejected without publishing partial data. Decoder and
encoder preflight precedes publication; neither uses a heap or native struct
serialization. Ignored extension values still receive complete syntax and
Unicode validation, with at most 12 nested containers including the root list
and record object.

The existing archive worker serializes native and JSON operations with exact
nonreused job IDs. A tagged allocation preserves the ordinary `.BLE` job size.
JSON export first copies the immutable native source into its scratch space,
converts into a separate imported model, and then reuses that scratch for
encoded bytes. The operations are sequential, so codec inputs and outputs
never overlap. Only the selected allocation arm is allocated and initialized;
ordinary native jobs do not inherit the larger JSON arm. The viewer uses one
optional committed JSON model. During export, the underlying native viewer
remains available for Back, so both viewers may coexist with the worker. No
additional worker, radio procedure or result bank is added to either receiver.

The linked native audit confirms the operation-specific allocations and agrees
with the focused host model/worker sizes. Compared with verified local rev122:

| Measured resource | Rev122 | Rev123 |
|---|---:|---:|
| S3 fixed data/BSS | 174,124 B | 174,156 B (+32 B) |
| C5 fixed data/BSS | 83,184 B | 83,184 B |
| Ordinary native BLE worker allocation | 57,648 B | 57,648 B |
| JSON worker allocation | Not present | 58,760 B |
| Imported JSON model, embedded in viewer/worker | Not present | 8,836 B |
| Optional JSON viewer allocation | Not present | 9,440 B |
| Existing native archive viewer | 61,112 B | 61,112 B |
| Archive worker entry frame | 48 B | 64 B |
| UI task entry frame | 1,072 B | 1,088 B |

The JSON viewer plus JSON worker requests 68,200 bytes of optional PSRAM. When
export retains the existing native viewer too, the corresponding total is
129,312 bytes. Embedded documents must not be counted again. These totals
exclude allocator/task overhead, fragmentation and other retained stores.
The JSON job requests 1,112 bytes more than the ordinary native worker arm;
ordinary `.BLE` jobs retain their prior allocation.

The existing worker stack allocation remains 6,144 bytes. The recursive JSON
`skip` entry frame measures 48 bytes per call, with nesting bounded by the
parser. Entry-frame sizes are not a full call-chain bound or runtime stack
margin; recursion, callees and interrupt use must be considered separately.
No running heap, SD-latency or throughput improvement is established by these
native measurements.

S3 performs file validation, conversion, display and SD IO. C5 contributes its
previously acquired, source-labelled observations through the validated native
snapshot; offline file operations do not query either radio or a current clock.
This division avoids moving file parsing to C5 simply to keep it busy.

Storage uses the existing firmware/USB/streaming-owner exclusion and safe
path checks. Saves choose a new `A00001.LOG`-style name under `/CERBERUS/BLE`,
use an exclusive `A00001.TMP`-style temporary file and preserve failures for
diagnosis. The generated range is A00001-A99999, with at most 1,024 candidate
names examined per save. They do not replace the source file or an existing `/Airtags_0.log`. Loads reject incomplete reads,
oversized files and trailing bytes beyond the declared file size.

An accepted result is immutable while copied outside the critical section;
reader pins defer release. Canceling a load discards its unpublished result.
A save already inside storage reports the actual outcome; cancellation does
not imply that a published file was rolled back. Configured byte bounds do not
provide a filesystem-latency or power-loss durability guarantee.

## Validation and limits

Focused tests using actual production codec, worker and storage functions pass:

- Codec: 2,214,494 assertions, including independent canonical bytes, duplicate
  addresses, empty payloads/lists, malformed Unicode and nesting, aggregate
  limits, complete source-bound ADV/RSP export and transactional failures.
- Storage: 1,675,246 assertions across 89 isolated file cases, including safe
  `.LOG`/`.JSON` imports, maximum-size reads, exclusive-name collisions, USB
  exclusion and injected read/write/flush/close/publication failures.
- Async worker: 35,776 assertions, preserving native v1-v6 cases and exercising
  typed result rejection, allocation canaries, immutable inputs, nested reader
  pins, stale jobs, exhaustion, allocation/task failure, conversion cancellation
  and the distinct in-flight LOAD versus SAVE/EXPORT outcomes.

Production UI validation also passes: 1,692 new JSON checks, 19,218 archive
renderer assertions, 4,371 live-adapter assertions and 170,281 saved-list
assertions. The separate directory suite also passes. Independent codec, IO/storage and UI reviews found no concrete
blocker. The final IO log is `artifacts/build/rev123-archive-io-final.log`.

The first complete host matrix passed all 132 suites on 1,096 unchanged inputs;
its receipt and logs are retained under `artifacts/rev123-host-evidence`.
All five native targets passed on the matching inputs, retained under
`artifacts/rev123-native-evidence/build-report.json`. The linked resource audit
passes at `artifacts/device-rev123/native-memory-comparison.json`.

| Native application | Bytes |
|---|---:|
| S3 main | 1,978,976 |
| Onboard C5 | 1,679,920 |
| Temporary S3/C5 bridge | 255,296 |
| Expansion WROOM | 926,400 |
| Expansion XIAO | 930,624 |

The matched package is
`dist/cerberus-unchained-0.1.0-dev.rev123-20261101T200841Z`.
It contains three targets (S3 application, onboard C5 and temporary bridge),
with nine images in total: bootloader, partition table and application for each
target. The matched package build and verify-only pass; image hashes, chip
headers, partitions and bounds were checked without changing package files.
The three packaged applications match the retained native build outputs.

- Manifest SHA-256:
  `17c04932b2f4d5b3ce9520886b56372bb9dbced8c14b4a0e53f2111fedce027b`.
- Final native audit SHA-256:
  `e4aeb42e49a4fd50a8208e537f581776837585617b00f09b47d657253e49ef4f`.

The initial audit's candidate-scope label named rev122; it was corrected to
rev123 and the audit rerun. Measurements and input/image bindings were
unchanged. The original review evidence and correction record are preserved
under `artifacts/device-rev123/native-audit-review` and
`artifacts/device-rev123/audit-correction.json`.

The finalizer writes `artifacts/device-rev123/local-milestone.json` only after a
clean source commit. That receipt binds the exact commit, retained host/native
inputs, resource audit and verified matched package, and records
`installed: false`. It is a local checkpoint receipt, not an installation
receipt. No physical SD, touch, BLE, GPS or expansion operation is part of this
milestone. Host fault injection does not establish real-card durability or
filesystem latency.

Synthetic manual-review inputs are prepared locally under
`artifacts/device-rev123/sd-ui-fixtures`, with a manifest identifying their
synthetic origin. `CERBERUS_SYNTHETIC.LOG` includes five ordered records,
a duplicate address, an empty payload and a 260-byte payload spanning three
raw pages. `CERBERUS_INVALID_SIZE.JSON` exercises rejection. These examples
have not been copied to the device and are not captured tag observations or
physical acceptance evidence.

Full live-list persistence parity, complete received payload retention beyond
existing previews, continuous callback history, AP/SSID list persistence and
broader backup/restore remain separate work. The ledger keeps its existing
partial/hardware/review statuses until the relevant outcomes are proven.
