# Rev133 development: selected AP/client Packet Count

This is a developing source checkpoint, not an installed-device or completed
Marauder-parity claim. Rev132 remains the installed baseline. Its paired boot and
storage mount were verified; historical Mesh Save/Open, real Mesh reception and
Locate acceptance remain separate from the owner's confirmed rev131 menu/Back
navigation. No GPS or expansion board is required for Packet Count.

## Intended behavior

Open a frozen Wi-Fi AP detail and choose **PACKET COUNT**, or select an observed
client and choose **COUNT THIS STATION**. Opening a page copies the selection;
it does not start a radio. A separate basket holds up to16 AP/client identities.
The existing archive save selection and action target lists retain their own
meanings. Fixed-channel, selected-channel and all-supported-channel plans use
explicit Start/Stop controls and ordinary Back navigation.

Both receivers use the same original bounded header parser and counter model.
Each selected target independently counts immediate transmitter/receiver address
matches; an AP match does not suppress a selected client in the same frame.
BSSID context is counted separately. Signal strength belongs only to observed
transmissions by that target. Retry, fragment and protected-frame subsets remain
visible. These are delivered SDK observations, not unique physical packets,
authenticated devices or proof of association. Totals for different targets can
overlap. Cumulative counts are not mislabeled packets per second.

For multiple channels the controller assigns disjoint coverage to the receivers.
The C5 handles5GHz; the S3 can cover2.4GHz channels concurrently. A single-channel
plan uses the C5 while the S3 handles presentation, validation and optional
storage. A definite clean S3 admission rejection can use the C5 fallback; held or
uncertain S3 ownership cannot be bypassed. Receive-window durations describe the
software's accepting windows, not continuous RF coverage. SDK channel rejection,
source errors and coverage remain explicit. A manual retune fences the previous
segment before accumulating its totals and starting the next plan.

## Optional packet recording

Count-only operation allocates no raw packet queue or recording worker. Enabling
recording adds a bounded two-packet queue per receiver, each with a4096-byte
frame limit, plus one optional S3 worker. Counter observations continue when a
recording queue overflows or SD recording fails. Queue losses and file/transport
errors are separate from the receive totals.

The worker uses full session IDs, an independent raw-metadata pin, per-frame
CRC32, exact part identities and acknowledged FIFO retirement. The C5 raw read
uses the existing64-byte SPI transaction capacity to carry40 data bytes per
part; ordinary control/metric commands remain32-byte transfers. Empty queues
back off between polls. This improves transfer capacity without allocating
larger SPI DMA buffers; it is not a measured over-the-air throughput claim.

One exact storage lease creates `/CERBERUS/PCOUNT/Pnnnnn/`, with separate lazy
`S3.PCAPNG` and `C5.PCAPNG` files. Source clocks are independent and monotonic;
PCAPNG uses untimestamped Simple Packet Blocks instead of invented UTC times.
Files begin as exclusive `.TMP` entries. Checked writes, flush, sync and close
precede publication; failed partial files remain available for inspection.
This is not a power-loss durability guarantee. USB handoff, unmount and
conflicting stream sessions cannot take over the active lease. A lost ACK
cannot append a record a second time. Stop/Back waits for source release,
recording drain and file cleanup before disposing of the associated state.

## Validation and remaining work

Implementation and focused checks are in progress. Early production-bound
model, C5/S3 service, controller, recorder, SPI and storage fixtures have passed;
integration review, the complete host matrix, native matrix, measured compiled
resource audit, packaging and any installation are not yet final for this
checkpoint. Do not infer standalone behavior from source or host checks.

Physical AP/client receive accuracy, split channel coverage, recording file
contents on the real SD card, repeated Stop/Back/rescan and sustained-load
behavior remain acceptance work. Optional counter CSV export and Marauder's
serial packet-capture transport are not implemented by this checkpoint.

The pinned Marauder develop reference is
[`79cc6bcba1db6679baee1dab2b0c1ad5010e6363`](https://github.com/justcallmekoko/ESP32Marauder/commit/79cc6bcba1db6679baee1dab2b0c1ad5010e6363),
resolved again on2026-11-02. Credit JustCallMeKoko and the ESP32 Marauder
contributors for the Packet Count workflow. Cerberus's implementation is
original, with provenance described in [Marauder attribution](MARAUDER_ATTRIBUTION.md)
and the [Packet Count contract](WIFI_PACKET_COUNT_PLAN.md). Full feature parity
remains unfinished.
