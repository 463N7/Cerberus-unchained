# Rev126: Recon priorities and navigation

Source version is `0.1.0-dev.rev126`; onboard C5 capability revision remains
69. Implementation and independent source/visual reviews are complete. All
136 host suites and five native targets pass on the same 1,120 unchanged
inputs. The linked resource audit passes; packaging and installation are the
remaining gates.
Installed firmware is rev124; the completed
[rev125 file-interchange package](DUAL_RADIO_REV125.md) remains a local,
uninstalled checkpoint whose changes are included in this follow-up.

The owner confirmed that the new dashboard appears and works, then requested
priority for recognized observations, a more intuitive layout, a visible radar
choice and the label ALERT instead of ANIMATED ALERT. That report does not
independently confirm every Pause/Resume, Back, recording or radar interaction.

The dashboard keeps ordinary observations accessible while presenting existing
classified hints first, then confidence and first-consumed time with stable
slot ties. Refreshes alone no longer reorder rows across sources. Dashboard
and Radar have large, explicit tabs on the main Recon page. Recognized category
headings are larger; the Radar legend uses one line per observation with a
complete RSSI value. The graph is labeled ALL LIST CHANGES and retains its
all-observation totals. Radar positions are list slots on an RSSI scale, not
bearing or distance. Back, Pause/Resume, Log, Options and optional SD recording
remain available. Category preferences now say ALERT.

The existing S3 snapshot already retains Meta identifier/context hints, but
rev124's Recon classifier does not consume them: its Smart glasses category is
demo-only. A bounded adapter now reuses accepted current original
advertisement evidence and present it as a Meta identifier hint, without
claiming glasses or model identity. No new signatures, parser or radio activity
is part of this bridge. Source, lease, identity, suppression and stale-evidence
checks remain required and are exercised by actual producer and integration
fixtures. Any existing Watch match takes precedence. Default MED alert
filtering remains unchanged: LOW Meta hints are retained in history/recording
without automatically producing a notification. No new radio operation, task,
observation bank or view/model allocation is added.

Focused validation passes 539 actual-producer Meta projection checks, 4,250
actual Recon UI/journal-adapter checks, 814 lifecycle/capture checks and 89
local rejection/acknowledgment checks. Inventory UI passes 1,178 checks, and
the existing renderer bounds/touch suite passes. These are source/host results,
not physical observations. The final host matrix also passes all 136 registered
suites; S3, onboard C5, bridge, expansion WROOM and expansion XIAO all pass native
builds. Resource measurements, packaging, installation and physical acceptance
will be recorded after they complete.
The full Marauder port and loaded radio/SD performance remain unfinished.

## Retained validation and resources

The final host receipt and its 136 hash-verified logs are retained under
`artifacts/rev126-host-evidence`. Native evidence for all five targets is in
`artifacts/rev126-native-evidence`. Nine actual-renderer previews with synthetic
data are bound to the final host receipt under
`artifacts/device-rev126/previews`; none is a physical screen capture.

`artifacts/device-rev126/native-memory-comparison.json` binds the installed
rev124 baseline receipt and both retained ELF sets. The unchanged model and
operation-allocation assertions pass.

| Resource | Installed rev124 | Rev126 |
|---|---:|---:|
| S3 application image | 1,973,984 B | 1,994,544 B |
| S3 fixed data/BSS | 169,356 B | 169,388 B |
| C5 fixed data/BSS | 83,184 B | 83,184 B |
| Optional Recon inventory/view store | 18,944 B | 18,944 B |
| Recon inventory model | 17,240 B | 17,240 B |
| Recon dashboard presentation | 760 B | 760 B |
| Dual Wi-Fi coordinator | 10,840 B | 10,840 B |
| Dual Wi-Fi live view | 5,664 B | 5,664 B |

Rev126 is 1,104 image bytes above the validated rev125 package and has the same
fixed S3 sections. The optional interchange allocations from rev125 remain:
10,168 bytes for the viewer, 75,784 for an AP JSON worker or 8,080 for an SSID
text worker. Existing BLE/AirTag/native Wi-Fi worker allocations and configured
task stacks remain unchanged. These figures exclude allocator overhead and do
not establish concurrent runtime peaks or fragmentation.

Selected native entry frames include 1,088 bytes for the UI loop, 320 for
`watch_draw`, 160 for the dashboard renderer and 224 for the outer Watch view
draw routine (16 above rev124). Some helpers are inlined and have no standalone
measurement; they are not treated as free. The audit is not a complete call
chain or worst-case runtime stack bound. Physical navigation, priority behavior
over real sweeps, repeated sessions, SD workflows and loaded performance remain
separate acceptance checks.
