# Rev131: Mesh menu navigation and passive selected-source Locate

Development source; full host/native matrix, package and physical acceptance
are pending. The paired rev130 image remains installed. This original Cerberus
implementation uses CYM as a workflow reference; see the
[reference and scope plan](MESH_DETECTION_PLAN.md). It does not claim complete
CYM or Marauder parity.

## Menu-first workflow

The owner reported that rev130 still showed Radio Sense in Scan and skipped
the tool menu. Its Back button then exposed the menu, reversing the intended
hierarchy. Rev131 changes the actual Scan renderer and page entry, not only a
secondary menu string:

1. Home → Scan → Mesh Detection opens the five-tool menu without allocating a
   survey controller or starting a radio service.
2. Mesh Detection, Channel Energy, Zigbee Beacons, Frame Activity and Compare
   Snapshots remain individual tools in that menu.
3. Back from a tool returns to this menu; Back to Scan returns to Scan.

Locate is an action on a selected address inside the Mesh observation tool.
It does not introduce another Scan submenu or discard the preserved survey.

## Division of work

C5 receives IEEE 802.15.4 passively on the selected channel. Its new compact
service matches the complete source address, address type and PAN context using
the production MAC parser. Unknown PAN is distinct from a known PAN and is not
a wildcard. Destination-only observations acquire signal measurements only
after that address is heard as a source.

The callback coalesces the newest matching observation instead of sending all
unrelated traffic through the survey FIFO. It uses the existing worker and an
optional internal allocation; it adds no radio task. The C5 parser/callback
placement is checked against the final native ELF before a build can pass.
This check is not a full vendor call-graph, interrupt-stack or RF latency proof.

S3 validates each complete pinned source projection, retains a 32-sample signal
history in optional PSRAM, renders the graph and age bound, and saves stopped
reports through the existing serialized archive worker. Unchanged sequences
do not reread raw bytes or add duplicate history samples. Published samples,
coalesced match updates and omitted older history entries are separate counts.

The survey finishes its exact-session stop/drain/release before Locate starts.
Locate workspace is allocated before requesting that handoff, so allocation
failure does not destroy the ongoing survey. Stop retains the Locate result;
Back closes its session and returns to the preserved address detail. Hidden
page cleanup must finish before another C5 owner proceeds.

## Source integrity and storage

New commands 154–160 require C5 capability 73. START is attempted once per
session; an uncertain reply is resolved through STATUS. STATUS renews the
active ten-second lease. A stopped result has a separate 30-second retention
lease. Expiry is checked before renewal, and the C5 worker handles cleanup
without depending on another SPI call. PEEK freezes a compact projection and
READ retrieves it in bounded chunks; there is no FIFO ACK operation in Locate.

Full raw headers are checked against the selected source before publication.
An unchanged sequence cannot change its raw witness, callback time or signal.
Invalid clocks revoke the displayed age bound. The two chips' uptime clocks
remain separate; the S3 pre-PEEK anchor yields a conservative upper age bound,
not a synchronized timestamp or proof of continuous reception.

SAVE REPORT requires a stopped projection, matching final STATUS and confirmed
release. It copies an immutable snapshot before handing work to the shared SD
worker. `CERBERUS_MESH_LOCATE_CSV,1` is separate from the survey CSV schema and
uses `/CERBERUS/LOGS/Mxxxxx.CSV` names. It includes the selected typed source,
source counters/error, retained/coalesced history, age-at-anchor bound and final
raw witness. Only the final sample has a raw witness in this report. The codec
has a 16,384-byte cap plus NUL; old archive operations retain their own exact
allocation sizes. Cancel before storage skips writing; a save already in
progress reports its actual result.

This is passive source-signal observation. It does not join a network, transmit
to a device, establish authenticated identity or routing, identify a Matter
fabric, infer distance/bearing, or reopen a historical map. No GPS is required.

## Validation record

Focused production-bound host fixtures currently cover C5 receive/filtering,
S3 controller and SPI adapter, source identity/raw validation, lost responses,
clock failure, session cleanup, CSV and archive lifecycles, UI navigation and
the preserved survey/Locate handoff. The complete suite registry is expanded
from 146 to 150. Native resource measurements and the matched build/package
receipt will be recorded after the full matrix passes. Host structure sizes
are not native resource measurements or physical acceptance.
