# Next major milestone and requested pause

The owner requested one more major milestone, followed by an update to existing
PR #1 in `463N7/Cerberus-unchained`, then a pause of the unfinished broader mission.
Preserve the existing PR branch history and its remote CI fixes; do not open a
second overlapping PR or force-push the stale local contribution branch.
Do not treat completion of this checkpoint as full Marauder feature parity.

The chosen milestone is a complete passive Probe Sniff workflow: a dedicated
receive entry, bounded deduplicated transmitter/requested-network inventories,
stable selected details, and explicit SD evidence saving. The C5 owns passive
Wi-Fi capture; the S3 owns collection validation, aggregation, presentation and
storage. Processor roles should improve the workflow rather than duplicate
work. The existing Wardrive probe-clue path must retain its current semantics.

Requirements include exact SSID bytes (including wildcard/binary names),
strict frame/IE validation, exact receive-session and snapshot identity,
checked Stop/Back/hidden/sleep cleanup, explicit retention/drop limits, and
honest channel/time/source evidence. Observed probes are not authenticated
clients, confirmed associations or proof of physical-device identity.

The rev127 reliability checkpoint has passed its full validation and local
package verification. Rev128 implements the Probe Sniff workflow in the same
worktree. Final integrated checks and packaging must bind the same frozen
source inputs; earlier passing receipts remain historical evidence if a
subsequent correction changes those inputs.

Before the requested PR, finish the new workflow, focused regression coverage,
complete host/native checks, memory/resource review, and feasible connected
device acceptance. Verify the live upstream base and current GitHub identity,
preserve attribution and integrate the accumulated contribution honestly.
Clearly separate source/build evidence from physical acceptance and record any
remaining hardware or upstream-parity gaps in the PR.

Once the major milestone and requested PR are complete, mark the existing goal
paused at the owner's explicit request. Leave a concrete resumption checkpoint
and stop further feature work. No additional PR or open-ended continuation is
authorized by this stopping condition.
