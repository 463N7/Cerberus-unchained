# Rev112: independent dual-receiver Bluetooth Fox Hunt

The **rev112 / C5 firmware67 local candidate is built and package-verified**.
It is not flashed. Rev111 remains the last verified installed baseline.
This checkpoint addresses the source audit in
[BLE_DUAL_FOX_AUDIT.md](BLE_DUAL_FOX_AUDIT.md), using the existing receive-side
exact-address-and-type Fox Hunt. The wider Marauder port remains incomplete;
build/package checks are separate from physical acceptance.

After the shared discovery drain, the S3 tracker attaches and starts
independently of the C5 TRACK request. A failed S3 attachment or RF start no
longer suppresses the available C5. RETRY S3 and RETRY C5 target only the failed
receiver; the healthy source keeps its target and session. Reentry while Fox
already owns or drains work is rejected instead of overwriting its target and
losing permission to cancel the old scan.

The screen has independent S3 and C5 status, RSSI meters, ages and counts. S3
uses its local tracker age, while C5 uses a conservative upper bound. A large
C5 bound says RECENCY UNCONFIRMED, rather than proving that a sample is stale.
Unknown timing is explicit. A sample retained across an automatic C5 job
restart says LAST JOB / RETAINED READING; it is not promoted to current-job
evidence because the next job is listening. Pending target changes and cleanup
suppress the previous target's samples on both panels.

S3 reports include original and response callbacks and reset with its host.
C5 counts matching complete original callbacks for the reported job, including
typed RSSI observations whose AD container may be malformed; partial reports
and scan responses are excluded. Counts are never summed across receivers or
jobs. An unavailable S3 tracker is distinct from an attached tracker with a
measured zero count. These are callback counts, not physical device counts.

STOP BOTH RECEIVERS, the STOP AND BACK footer, and sleep use the same
cancellation path. It cancels a pending C5 request even before INFO acquires a
job and preserves the coordinator's full-width clock. Cleanup continues after
the 12-second warning until the owned radios release. Failed or uncertain
cancellation retains the barrier; a deadline does not establish release.
An S3 retry that times out keeps draining its failed sweep, but a late release
requires explicit retry rather than automatically restarting. Generation and
host-epoch changes revoke permission to stop a replacement S3 scanner. A
repeated startup timeout repaints after retry. The existing generic console
Fox error exposes shared startup/cleanup timeouts; it does not add fields or
change the receiver-specific diagnostic meanings.

The implementation reuses the existing tracker, cached C5 coordinator and
passive tracking mode. It requires no wire protocol change, new radio mode,
task or history bank. C5 capability firmware remains 67. Rendering must not
perform transport operations. Two nearby receivers do not establish distance,
bearing or triangulation.

All 124 host suites passed with unchanged inputs. The focused production
extraction checks cover independent attach/start failures and retry,
foreign-owner protection, target reentry, shared-barrier timeout, repeated
Back, sleep, pre-INFO cancellation, the full clock beyond the 32-bit rollover,
and cleanup held past its deadline followed by release. The native-live UI
fixture reports 1,251 checks. Presenter fixtures cover both/single/no-sample
views, typed-target mismatch, unavailable versus zero counts, C5 job rollover,
repeated cached reads, unknown/saturated ages and counts, privacy and display
failure propagation. The generated 320 by 480 previews were inspected for fit.
Independent source and fixture review found no remaining blocker.

All five native targets also passed with unchanged inputs matching the host
run: S3 application, onboard C5, temporary bridge, expansion WROOM and expansion
XIAO. Retained evidence is under `artifacts/rev112-host-evidence` and
`artifacts/rev112-native-evidence`. The earlier successful pass is retained
separately as `rev112-first-pass-*`; it predates the timeout repaint and generic
diagnostic follow-ups and is not the final package evidence.

The offline resource audit compares the final native images with the retained
installed-rev111 images and installation receipt:

| Native resource | Rev111 | Rev112 | Change |
| --- | ---: | ---: | ---: |
| S3 fixed data plus BSS | 173,596 B | 173,612 B | +16 B |
| C5 fixed data plus BSS | 83,160 B | 83,160 B | 0 B |
| Optional archive viewer plus worker types | 118,760 B | 118,760 B | 0 B |
| Those types plus retained dual discovery stores | 161,696 B | 161,696 B | 0 B |
| Native UI entry frame | 1,136 B | 1,136 B | 0 B |

The new temporary Fox cached-view type is 60 bytes and the linked Fox draw
entry frame is 304 bytes. There was no separately linked draw symbol in the
baseline, so its absence is not treated as zero cost. The Fox tick entry frame
remains 48 bytes. Main and NimBLE task allocations are unchanged. Selected
entry frames are not whole-call-chain bounds or runtime stack headroom;
optional type sums exclude allocator overhead and fragmentation. The audit
adds no radio operations and is retained in
`artifacts/device-rev112/native-memory-comparison.json`.

The matched package is
`dist/cerberus-unchained-0.1.0-dev.rev112-20261101T133750Z`.
Its hashes, chip headers, partitions and image bounds pass verification, and
its three application images match the retained final native matrix:

| Image | Application bytes | SHA-256 |
| --- | ---: | --- |
| S3 | 1,934,240 | `f8043012cf27230f7fbbc490a3572b70404adcc891b536f596839057f713933f` |
| C5 | 1,678,720 | `4fb73f0ba01782abe13e46f41df214f6bc0375c282caead70898e5cd3c03021a` |
| Bridge | 255,296 | `1a2f5b2aa7d097de535fb5bdddc46a8361013aaba42b84c242e2a872f29a48d1` |

The source fingerprint is
`790ebb410c77f78f0a97a5bcfb165a0e3b913d010271dd23f98c0b40da6b17fe`;
the manifest digest is
`ad62209702098a02b1fcc23bd95c3c61826dca67745aa3b539910d4b380cdb90`.
`artifacts/device-rev112/local-milestone.json` binds the clean source commit,
package, final host/native receipts and resource audit, with `installed:false`.

Physical movement, single-receiver reception, repeated transitions, error
recovery and loaded memory still require device acceptance. No RF speed,
coverage improvement or full-feature validation follows from these checks.
No device access, flash, PR, push or release was performed for this revision.
The coverage ledger retains the same 95 mode-status assignments and six
workstream statuses; it is not a completion percentage.
