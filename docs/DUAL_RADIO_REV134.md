# Rev134: Packet Count progress and diagnostics

Rev133 is the installed baseline. The owner reported that Packet Count showed
S3 NOT ASSIGNED and appeared inactive after Start. The single-channel plan
intentionally assigns reception to C5 and control, analysis and presentation
to S3. That role label was misleading, and reception has not been established
by the owner's report or the available rev133 console fields.

## Reproduced scheduling defect

The controller checks status every second. If UI ticks are at least one second
apart and each successful exchange consumes the 8 ms tick budget, every tick
can perform only STATUS. This renews an accepted staging session without
uploading targets or starting reception. The same priority can prevent live
counter reads. A regression using 1.2 s tick spacing and 9 ms exchanges fails
against the prior implementation because START is never reached.

Rev134 gives each validated, exact-session STATUS one phase-progress credit.
While that proof is younger than half the native 10 s lease, one target upload,
Start or readout operation can precede another due status check. The credit is
consumed even if the operation fails. Stop, pending Release and missing control
proof retain priority. This does not add a second blocking operation after the
tick budget has expired, promise progress at arbitrary delays, or bypass
session and lease validation. No C5 protocol or native receiver change is
required; capability 74 remains current.

The host reproduction identifies a real failure path, not yet the confirmed
cause of the physical report. Rev133 diagnostics showed a C5 reservation but
could not distinguish staging, queued setup and actual reception.

## Screen and read-only diagnostics

For a C5-only plan, the S3 panel says ANALYSIS and CONTROL / READOUT. The C5
panel distinguishes checking, reservation, target upload, starting, queued
work and receiving. Readout reports copied parts; RX duration stays unknown
until a validated publication exists. A quiet channel with validated zero
counts is distinct from a session whose counters have not arrived.

The new USB console command `packet_count` reports copied scalar state: source
phase and ownership, channel/configuration, validated counters, recording
errors, UI tick gaps and last phase progress. Status heartbeats alone do not
advance the progress timestamp. Unknown or stale evidence is not reported as
known zero. The snapshot contains no target addresses or frame payload and
performs no radio calls or lease renewal. It is bounded and independently
copied for the console; the console handler remains a separate stack frame.

## Validation boundary

Focused controller, UI and diagnostic fixtures are being checked before the
full 158-suite host run, five native targets, resource inspection and matched
packaging. No rev134 image has been installed at this source checkpoint.
The previous rev133 package and installed-image receipt are retained separately
for recovery. Final build, package and installation evidence will be added
after those steps succeed.

Required physical checks are Packet Count setup and counter publication on an
active owner AP, Stop/Back followed by a normal Wi-Fi scan, and a repeat without
reset. Optional recording and multi-channel coverage need their own acceptance.
Mesh Save/Open, real Mesh/Locate reception, GPS and six-radio expansion remain
separate pending work. Full Marauder parity and serial packet capture remain
unfinished. A read-only upstream check retained the existing develop revision
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363` on 2026-11-02.
