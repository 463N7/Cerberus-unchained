# Radio memory recovery candidate

Rev95 follows the installed rev94 split-scan checkpoint. Its primary acceptance
criterion is repeated BLE discovery -> Stop/Back -> S3 2.4 GHz plus C5 5 GHz
scanning without rebooting. The candidate is not yet physically accepted.

## Observed problem

On rev94, the owner confirmed initial split Wi-Fi scans, Wi-Fi Stop/restart/Back,
and activity on both BLE receivers. After leaving BLE, the S3 host was still
initialized and synchronized but had no owner. Internal free RAM remained
46,588 bytes, below the Wi-Fi scanner's 65,536-byte admission requirement.
The screen correctly showed S3 unavailable and selected C5 all-band fallback.
This establishes a retained-memory limitation, not a reason to report the
split feature fully optimized. Full measurements and evidence limits are in
[the rev94 scan record](DUAL_RADIO_SCANNING.md).

## Candidate changes

The S3 selects the supported NimBLE `EXTERNAL` allocation mode with its existing
explicit PSRAM policy. Generic malloc remains caps-only. Controller/DMA memory,
normal task stacks, packet-pool counts and the resident host lifetime are
unchanged. Eligible host allocations move to PSRAM; this is not a claim that
every Bluetooth allocation is external.

The shared scanner/advertiser/central admission path checks that PSRAM is
initialized and registered before entering the external-only allocator. A
board without PSRAM can still boot; BLE admission returns `ESP_ERR_NO_MEM`.
This presence guard does not guarantee success under later PSRAM exhaustion.
The SDK's ordinary allocation-failure behavior is unchanged. No lower Wi-Fi
admission threshold, host teardown cycle, reduced scan pool or global malloc
policy change is introduced.

Fixed lifecycle samples record heap counters before/after initialization,
after synchronization, on release and on refusal. They describe process-wide
heap at those boundaries, not precise Bluetooth allocation totals. Largest
block queries remain in the explicit memory command because they walk the heap.

An unknown C5 firmware cache now triggers one passive status probe inside the
existing transport deadline. Confirmed older firmware remains unsupported;
a transient timeout is no longer treated as proof of an old peer. Existing
session/target ownership and stale-reply fences remain in place.

## Diagnostics

| Command | Scope |
| --- | --- |
| `wifi_dual` | UI-published assignment, counts, errors, cleanup, rebind and S3 ownership |
| `dual_ble` | UI-published companion state and independently bound S3/C5 Fox samples |
| `ble_memory` | Cached S3 BLE lifecycle heap samples |
| `memory` | Current heap counters and largest-block queries; existing C5 status request |

The three new commands read fixed local snapshots. They do not start RF,
query C5, renew leases, allocate memory or read UI-owned stores directly.
Publication sequence/time identify the snapshot; cached freshness applies at
that publication time and is not refreshed by repeatedly reading the console.

The BLE UI publisher uses a local scanner getter while Fox Hunt is bound. That
getter takes the scanner mutex and can reconcile a host reset; the publisher
is therefore not a hard real-time or strictly side-effect-free operation.
Similarly, the existing SPI software deadline cannot interrupt an in-flight
SDK polling transfer. No hard touch-latency bound is claimed.

## Validation status

Focused host checks pass for the actual host in INTERNAL, EXTERNAL and
Bluetooth-disabled configurations, missing PSRAM and repeated claims,
ownership races, scan lifecycle, immutable target publication, cached snapshot
coherence, clock/sequence limits, console formatting, and firmware-cache
recovery. All 119 suites passed with no changed source/configuration inputs
in the full rev95 run; the receipt and logs are retained under
`artifacts/host-checks/rev95-full-run`. Synthetic console checks
validate the stack-checker's parser contract, not native stack frames.

All five native targets pass with unchanged inputs. The generated S3 config
selects `CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL` and
`CONFIG_SPIRAM_USE_CAPS_ALLOC`. Native console frame analysis passes with
1,200 bytes beyond the measured frames within the 4,096-byte allocation;
this is not a whole-program stack bound. The isolated build environment uses
the same compiler/SDK versions and compiler binary hashes as the rev94 receipt.

The matched development package
`cerberus-unchained-0.1.0-dev.rev95-20261101T035627Z` passes source, image hash,
chip header, partition and image-bound verification. It contains S3, onboard
C5 and the temporary USB bridge. The two expansion targets are compile-tested
separately; no expansion hardware is attached or accepted.

Hardware must still measure the recovered warm internal RAM, scan completion, both receiver
counts and cleanup across repeated transitions. Full Marauder parity, active
Wi-Fi errors, expansion/GPS hardware, calibrated Fox Hunt performance and
sustained load remain separate work.
