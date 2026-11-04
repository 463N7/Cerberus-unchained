# Integrated Wardriving logs and upload scope

Requested on 2026-11-04, after reliable capture/export validation. This document
records the intended implementation and verified contracts; it does not claim
an uploader is installed. No credentials or logs have been uploaded during this
work. Existing standalone Wardriving sources include WiGLE/WDGWars upload code
that can inform integration after adapting ownership, storage and UI handling.

## Navigation and behavior

Use one Wardriving → Logs & Uploads branch. A list shows completed session files,
size and per-destination status. Selecting one file offers Upload and Delete.
The list offers Upload All and Delete All. Upload lets the user select configured
destinations, including all three. Delete All confirms the named Wardriving
folder and file count, never targets other SD assets, and does not auto-delete
after uploading. Unlocated evidence remains available for local inspection and
deletion but is not offered to geolocation upload services.

One setup page stores a home Wi-Fi profile and separate WiGLE API name/token,
WDGWars token and Wardrift token. Entry is masked. Credentials stay outside CSV,
diagnostics, logs and general settings exports. Keep them in a dedicated device
configuration namespace, support explicit replacement/forget, and document
actual at-rest protection instead of claiming encryption that is not configured.
Do not burn security eFuses or change provisioning as a side effect of setup.

Recording must finish and both radio owners must be released before networking
starts. The uploader leases the SD card so USB handoff, capture and deletion
cannot race the file being streamed. Transfer in bounded chunks, verify TLS
certificates and host names, establish valid wall time, release resources on
Stop/failure, and preserve the original CSV. Network loss cannot mark a file as
accepted. UI remains responsive with file/byte progress and an explicit Cancel.

Receipts are separate for each destination and bound to file content and account.
Track pending, sending, queued/processing, accepted, failed and unknown result.
An HTTP success that merely queues import is not a completed import. An ambiguous
timeout after sending is checked against provider history when supported before
resending. Retry only unfinished destinations, respect Retry-After and service
limits, and show server-confirmed counts independently of locally recorded rows.
Upload All never silently changes the requested observation-retention policy.

## Contract verification

| Destination | Confirmed public interface | Remaining verification |
| --- | --- | --- |
| WiGLE | WiGLE 1.6 CSV; official client exports UTC and Wi-Fi/BLE types. Existing standalone code uses HTTPS file upload and API Basic authentication. | Current upload/status schema, documented missing-accuracy handling, rate limits and actual import test. |
| WDGWars | `POST https://wdgwars.pl/api/upload-csv`, `X-API-Key`, multipart `file`; the official press/API page documents WiGLE 1.6, 40 MB CSV limit, sequential requests and rate limiting. | Parse real success/failure and asynchronous status responses; authenticated import test. |
| Wardrift | User requests token-backed upload. | Public homepage returned a browser challenge during research. Verify the official endpoint, token header, file format, response/status schema and limits before implementing the adapter. Do not guess an endpoint or copy another provider's authentication. |

Provider fixtures must cover malformed/partial replies, HTTP 200 with a service
error, authentication failures, rate limits, redirects, lost connections after
the body, cancellation, short SD reads and interrupted receipt writes. Live tests
must use an explicitly selected real capture and account; never upload synthetic
fixture networks to production services. A service import receipt is required
before describing that destination as tested.

References:

- [WiGLE CSV 1.6](https://api.wigle.net/csvFormat-1_6.html)
- [WiGLE's official client](https://github.com/wiglenet/wigle-wifi-wardriving)
- [WDGWars developer API and limits](https://www.wdgwars.pl/press)
- [Wardrift](https://wardrift.net/)
- Preserved local reference: `integration/baselines/wardriving/s3_main/main/wigle_uploader.c`
  in the parent development workspace; it is not linked into the current main application.
