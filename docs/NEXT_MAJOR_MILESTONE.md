# Next major milestone and requested pause

The owner requested one more major milestone, followed by an update to existing
PR #1 in `463N7/Cerberus-unchained`, then a pause of the unfinished broader mission.
Preserve the existing PR branch history and its remote CI fixes; do not open a
second overlapping PR or force-push the stale local contribution branch.
Do not treat completion of this checkpoint as full Marauder feature parity.

The implemented milestone is the passive Probe Sniff workflow: a dedicated
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

The rev127 reliability checkpoint and rev128 implementation both have verified
local packages. Rev128 passes 138 host suites, five native targets and the
linked resource audit on 1,142 unchanged inputs. Its matched package and all
package checksum entries verify; see [the final checkpoint](DUAL_RADIO_REV128.md).
Rev126 remains installed. Probe reception, saving/reopening, warm repeated use
and loaded responsiveness still require physical acceptance.

Update the existing PR with the accumulated contribution and final local
validation, preserving its later remote CI fixes and upstream attribution.
Keep hosted CI and physical acceptance separate from the completed local
checks. No new image was installed during this milestone's preparation.

Once the major milestone and requested PR are complete, mark the existing goal
paused at the owner's explicit request. Leave a concrete resumption checkpoint
and stop further feature work. No additional PR or open-ended continuation is
authorized by this stopping condition.
