# Rev139: integrated saved-log management and uploads

This milestone adds two connected capabilities to the main firmware: a paged
Wardriving log manager and saved home-Wi-Fi/three-service uploads. The installed
capture baseline is rev138. Rev139 installation/physical acceptance is separate
from source/build validation.

## Saved captures

One **Scan → Wardriving → LOGS / UPLOAD** branch lists finalized native files,
validates complete contents, displays counts/CSV UTC, and provides confirmed
Delete/Delete All. Its background worker holds an exact SD lease, excluding
capture, USB handoff and File Tools mutations while the reviewed catalog is
active. Stale confirmations cannot affect a newer catalog. Interrupted captures
and unrelated SD resources are excluded.

## Home network and transfers

The same branch contains masked AP/API setup, destination toggles, Upload/Upload
All, progress, Cancel and separate results. S3 performs station/TLS/SD work after
capture stops; C5 receives no redundant network work. Existing receive allocation
is unchanged. A dedicated NVS blob stores configuration; temporary application
buffers are scrubbed. No encryption or eFuse provisioning is implied.

Transfers use bounded buffers, retain every observation and split large files
at complete rows. Durable per-part receipts bind the complete source hash and
exact credentials. WiGLE acknowledgement remains QUEUED; ambiguous delivery
cannot become success or be blindly retried. The [setup guide](WARDRIVE_UPLOADS.md)
describes actual navigation, credential handling, contracts and limitations.

## Capture baseline in this contribution

Rev137 introduced continuous callback queues, three capture modes, S3 2.4 GHz
plus C5 5 GHz allocation, dual-receiver BLE-only mode, and GPS AUTO, LC29H-AA,
ATGM336H/manual settings. Rev138 removed unused duplicate-history work and fixed
the native WiGLE pre-header. Its 31.415-second run produced 1,438 observations:
263 S3 Wi-Fi, 121 C5 Wi-Fi and 1,054 S3 BLE. Physical SD readback accounts for
all 1,436 located and two unlocated rows, with zero reported queue/radio/SD
errors. Missing accuracy and two missing altitudes remain warnings. See
[rev137](DUAL_RADIO_REV137.md) and [rev138](DUAL_RADIO_REV138.md).

## Validation scope

Six new registered suites cover the streaming log parser, config/NVS adapter,
provider replies/lossless part plans/receipt corruption, real manager/leased
storage, real HTTP adapter with controlled SDK failures, and actual touch
routes/rendering. The existing Wardriving UI suite checks the three-button
Start/Recent Nets/Logs row. Full matrix/package evidence is recorded after the
final source is frozen.

This does not finish Marauder parity. Live service acceptance, WiGLE processing
reconciliation, physical upload/Cancel transitions, long-drive RF completeness,
sustained GPS lock, dense BLE performance and six-C5 hardware remain separate
tasks. Wardrift follows a public proposal rather than a tested live contract.
The source audit retains develop `79cc6bcba1db6679baee1dab2b0c1ad5010e6363`,
rechecked through the read-only GitHub API on 2026-11-05 UTC. This milestone
changes no attack behavior. Private captures, coordinates and credentials are
excluded from the contribution.
