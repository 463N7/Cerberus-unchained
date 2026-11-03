# Rev136: Packet Count recording memory checkpoint

The optional Packet Count recording worker previously allocated separate
4,096-byte raw-frame and 4,144-byte PCAPNG buffers. The existing serializer
already supports overlapping input/output: it moves the payload before writing
the PCAPNG and radiotap headers. Rev136 receives raw chunks directly into the
output buffer, validates their CRC, then encodes in place.

The source retains that scratch until its ACK completes. Consumed-sequence
marking still precedes storage append, so a lost ACK cannot serialize or append
the same frame again. The receive limit remains 4,096 bytes even though the
output buffer has additional header space. S3 and C5 still alternate after ACK;
no new task, queue, radio command or protocol capability is introduced.

The focused production-worker fixture checks complete raw payload preservation,
PCAPNG block extents, radiotap source-band metadata, padding, every short length
from 24 through 96, boundaries around each 40-byte transport chunk up to the
maximum, and 4,095/4,096-byte frames. Lost ACK, bad CRC/metadata, partial tails,
SD failure, source rebind and canceled staging cases remain covered.
The focused recorder passed 307,450 checks and the SPI adapter passed 338.
The host worker context is 4,344 bytes; native size is measured independently.

Full host/native validation, linked-resource comparison and matched packaging
are pending at this source checkpoint. Rev136 has not been installed.

## Installed baseline and remaining scope

Rev135 remains the installed and tested pair. Its channel-11 Count session
finished with 4,306 valid frames and 1,303 selected matches, zero metadata
rejections and normal Stop. A fresh subsequent Wi-Fi scan advanced generation
and successful C5 scans to two, returned 19 APs (10 S3, nine C5), and completed
without fallback or error after Count released its workspace. See
[the rev135 evidence](DUAL_RADIO_REV135.md).

This checkpoint will update existing Unchained PR #1 as requested, preserving
its branch history and earlier CI corrections. It does not select a public
binary release. The unfinished USB framing/transport/receiver draft is preserved
locally outside the build inputs and is not part of this contribution.

The latest read-only upstream develop comparison still resolves to
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363`. Existing attribution and licenses
remain intact. No feature is promoted to complete on the basis of this memory
optimization. Full Marauder parity, USB capture, SD recording acceptance,
5 GHz Count acceptance, Mesh Save/Open/Locate reception, GPS and six-C5 expansion
validation remain separate unfinished work.
