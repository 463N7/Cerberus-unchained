# Wardriving logs, home Wi-Fi and uploads

Use **Scan → Wardriving → LOGS / UPLOAD** after stopping capture. Rev139 adds
the integrated log manager and uploader; rev140 adds one JSON profile for home
Wi-Fi and all three services. Installation and physical acceptance are recorded
separately in the [rev139](DUAL_RADIO_REV139.md) and [rev140](DUAL_RADIO_REV140.md)
checkpoints and the [rev141 update](DUAL_RADIO_REV141.md). Upload and Upload All
remain manual and require confirmation. Stopping capture, importing a profile
or rebooting never starts an upload. Paired rev141 installation preserves the
saved profile, and the owner confirmed log receipt details and Back navigation.
The owner's rev140 upload has a matching
queued WiGLE transaction; processing/credit are still unverified. WDGWars delivery
remains uncertain. CSV validation alone is not a WiGLE acceptance receipt.

## Import one JSON profile from SD

1. Copy [wardrive_upload.example.json](../config_examples/wardrive_upload.example.json)
   to your PC and fill in the home network and service fields. The distributed
   example contains empty values only. Keep your filled copy private.
2. Set `enabled` to `true` for each destination you want to use. WiGLE needs
   both `api_name` and `api_token`; WDGWars needs its 64-character hexadecimal
   token; Wardrift needs its token. Leave unused services empty and disabled.
3. Save as UTF-8 without a byte-order mark, named **UPLOAD.JSON**. Copy it to
   **/CERBERUS/CONFIG/UPLOAD.JSON** on the SD card. Create `CONFIG` if necessary.
   Safely eject the card or USB drive and return SD ownership to Cerberus.
4. Open **LOGS / UPLOAD → HOME WI-FI / API SETUP → IMPORT JSON FROM SD**.
   Review the replacement notice and tap **CONFIRM IMPORT**. This replaces the
   entire saved upload profile, including fields left empty or disabled.
5. Wait for **PROFILE SAVED / SD COPY REMOVED**. Reopen setup to check the
   SET/EMPTY labels and enabled destinations. The saved settings persist across
   restarts. Importing or saving never starts a connection or upload.

The JSON contains these exact sections:

```json
{
  "schema": "cerberus-wardrive-upload-v1",
  "home_wifi": { "ssid": "", "password": "" },
  "wigle": { "enabled": false, "api_name": "", "api_token": "" },
  "wdgwars": { "enabled": false, "token": "" },
  "wardrift": { "enabled": false, "token": "" }
}
```

All sections and fields are required; comments, unknown or repeated keys and
incomplete enabled destinations are rejected. Maximum file size is 4,096 bytes.
Limits are measured in UTF-8 bytes: SSID 32, password 64, WiGLE API name 64,
WiGLE token 128, WDGWars token exactly 64 when present, and Wardrift token 128.
JSON supports UTF-8 SSIDs even though the on-device keyboard is ASCII. The home
AP must offer 2.4 GHz; the authentication limits below still apply.

Malformed JSON leaves saved settings unchanged. The worker saves valid settings,
reads them back and compares them before removing the SD copy. If saving or
verification fails, the SD file stays available for review/retry. **PROFILE SAVED
/ SD COPY REMAINS** means settings were verified but SD cleanup failed; remove
that file after checking setup. File removal is not a secure erase. The private
PC copy also remains under the user's control. Filled profile filenames are
ignored by Git; never include credentials in an issue, PR or firmware image.

The read-only `upload_profile` console command reports readiness flags and the
enabled-destination mask only. It does not print the network name, password,
tokens or credential fingerprints. The rev141 read-only `upload_status` command
adds operation/stage, service, HTTP status, response length/completeness and
transport error. It exposes no response contents or transaction IDs. These
current-attempt diagnostics are not persisted in the receipt journal.

## Set up home Wi-Fi and services

1. Open **Scan → Wardriving**, finish recording with **STOP**, then open
   **LOGS / UPLOAD → HOME WI-FI / API SETUP**.
2. Tap **HOME WI-FI SSID**, enter the exact network name and tap **DONE**.
   Enter **HOME WI-FI PASSWORD** the same way. Upload uses the S3's **2.4 GHz**
   station interface; a 5 GHz-only AP cannot be used. A blank password means an
   open network. Enterprise authentication and captive-portal sign-in are not
   implemented. The current keyboard enters ASCII.
3. For **WiGLE**, obtain the **API name and API token** from your account's
   [API page](https://wigle.net/account?account=api). Enter those two fields,
   rather than your normal account password.
4. For **WDGWars**, generate the 64-character hexadecimal API key in your
   profile and enter **WDGWARS TOKEN**. See the provider's
   [developer guide](https://wdgwars.pl/press).
5. For **Wardrift**, enter your device/API key in **WARDRIFT TOKEN**. This
   provisional adapter follows [Marauder's Wardrift contribution](https://github.com/justcallmekoko/ESP32Marauder/pull/1634),
   revision `0b6ac93f996beafb69070fe04b68656ce604c2d0`, separate from the pinned
   develop tree. Wardrift's homepage did not expose readable API documentation
   during this audit; live account verification remains required.
6. Toggle **WIGLE**, **WDGW** and **DRIFT** ON for the desired services, then
   **SAVE SETUP**. Destinations default OFF. Saving does not connect or upload.
   **CANCEL SETUP / LOGS** discards unsaved edits.

Password/key entry is masked, with an explicit reveal control. Setup reports
SET/EMPTY without showing saved values. A dedicated `ward_upload` NVS namespace
stores a versioned, corruption-checked blob. Keys/passwords are excluded from
CSV, receipts, general settings exports and application diagnostic messages.
This build does **not** enable NVS encryption or burn security eFuses; physical
access can expose credentials. **FORGET HOME AP AND ALL KEYS** confirms removal
of this profile, leaving captures and receipts intact. Rotate compromised keys
with their providers.

## Review, upload and delete

The paged list includes finalized native `WIGLE001.CSV`–`WIGLE999.CSV` and
`UNLOC001.CSV`–`UNLOC999.CSV` in `/CERBERUS/WARD`, up to 1,998 files. It sorts
by filename, not FAT timestamps. Selected-file details validate complete CSV
contents and show size and rows. Located files also show 2.4/5 GHz Wi-Fi and BLE
counts, earliest CSV UTC, missing-measurement warnings and destination receipts.
Unlocated files explicitly mark radio subtotals unavailable and show no invented
UTC/location. Filename order across prefixes is not a timeline.

- Choose **UPLOAD THIS LOG** on a valid located file, or **UPLOAD ALL LOCATED
  LOGS** on the list. Review count, selected services and GPS/identifier
  disclosure, then **CONFIRM UPLOAD**.
- **DELETE THIS LOG** and **DELETE ALL FINALIZED LOGS** require separate
  confirmations bound to the reviewed catalog and SD lease. They exclude `.TMP`
  captures, unrelated folders/assets, credentials and receipts.
- Upload retains the original CSV. **CANCEL OPERATION** stops further work;
  **BACK / CANCEL AND CLOSE** waits for cleanup. Pending socket work can take
  its SDK timeout to exit. Canceling bulk deletion cannot restore files already
  deleted; the processed count records the partial operation.

Uploads obtain current UTC from NTP over the home network. Connection, clock
or certificate failure reports a stage/error; there is no insecure TLS fallback.
Captures more than 60 seconds ahead of synchronized UTC are rejected.

Unlocated logs stay local and are skipped by Upload All. Legacy pre-headers
missing Earth metadata remain inspectable but require PC export/review; the
device does not silently rewrite them. A malformed/truncated located file stops
the pass. Review it on a PC or select a valid file individually. Missing accuracy
or altitude remain warnings; HDOP is never substituted for measured accuracy.

## Results and retries

| Result | Meaning |
| --- | --- |
| NOT SENT | No matching receipt for these contents and credentials. |
| SENDING | Durable local intent, not a service acknowledgement. |
| QUEUED | WiGLE returned `success: true` and a transaction ID, or WDGWars v2 returned HTTP 202, `ok: true` and a positive job ID. Processing/credit remain unverified. |
| ACCEPTED | WDGWars returned synchronous `ok`/import counts, or Wardrift returned `accepted: true`. WDGWars imported/rejected-row counts are separate. |
| REJECTED | A complete recognized reply explicitly rejected the request. Correct its cause before manually retrying. |
| CHECK ACCOUNT | Ambiguous delivery, damaged receipt or unexpected response. No automatic resend. |

Queued/accepted parts are skipped on later uploads with the same contents and
credentials. Observation rows are never deduplicated. Receipts bind SHA-256 of
the **whole original file**, destination, credential fingerprint and versioned
part plan. Rotating a key creates a new receipt identity: check the account
before uploading old captures again. Receipts contain no raw keys, passwords,
observed MACs or coordinates.

After ambiguous delivery, check the provider account first. WiGLE transaction
IDs are retained in the journal; automatic transaction polling/reconciliation
is not implemented. Export `/CERBERUS/WARD/UPLOADS` and the original for PC
review. Do not remove receipts just to force retry without checking the account.
If a saved uncertain receipt blocks a retry, rev141 names the service, for
example **WDGWARS / CHECK ACCOUNT FIRST**. Rev140 could leave **VALIDATING SAVED
CSV / ESP_ERR_INVALID_STATE** on this path despite valid CSV and saved settings.
Rev142 adds a recovery route after that account review: open the affected log,
tap **UPLOAD THIS LOG**, tap **WDGWARS: REVIEW RETRY** (or the affected service),
then read the file/service scope and **CONFIRM SERVICE RETRY**. The earlier
attempt may already have arrived, so this can resubmit its data. Only the chosen
service is attempted; acknowledged parts and other services are not resent.
Cancel does not send. This does not clear journals, override corrupt receipts,
or enable retries across Upload All. The original CSV and journal history stay
on SD. A disabled service must first be enabled in setup.

WDGWars uploads now use `/api/v2/upload-csv`, matching the current Marauder
development implementation. A valid 202 job receipt is retained as QUEUED;
processing is asynchronous and automatic job-status polling is not implemented.
See the [rev142 source references and validation scope](DUAL_RADIO_REV142.md).

Interrupted journal writes and invalid CRCs fail closed. Authentication/rate
errors stop the pass without automatic retry. Requests are sequential, at least
2.5 seconds apart. No synthetic test records are uploaded to production services.

## Implementation and contracts

S3 owns UI, SD, home Wi-Fi and HTTPS. C5 does not duplicate uploads. Receive
allocation is unchanged: S3 2.4 GHz plus C5 5 GHz for Wi-Fi, S3 Wi-Fi/BLE windows
with C5 on 5 GHz in combined mode, and both receivers in BLE-only mode.

The worker and 16 KiB internal stack exist only while this branch is open.
Catalog, transfer workspace and TLS allocations use PSRAM. Transfers stream
4 KiB buffers and split files at complete rows into parts containing at most
1 MiB of data plus the original two headers. This is below WDGWars's documented
40 MB cap, preserves every observation and avoids full-file RAM allocation.
Wardrift's current limits still need live verification.

| Service | Request | Acknowledgement |
| --- | --- | --- |
| WiGLE | `POST https://api.wigle.net/api/v2/file/upload`; Basic API-name/token; multipart `file` | HTTP 200, `success: true`, exactly one transaction ID; QUEUED |
| WDGWars | `POST https://wdgwars.pl/api/upload-csv`; `X-API-Key`; multipart `file` | HTTP 200, `ok: true`, numeric `imported`; ACCEPTED with `no_gps`/`bad_rows` counts |
| Wardrift | `POST https://wardrift.net/v1/ingest/signals`; `X-API-Key`, `X-File-Name`; raw `text/csv` | HTTP 200, `accepted: true`; provisional until live verification |

HTTPS verifies CA chain/hostname and disables redirects. Complete responses are
bounded to 4 KiB and parsed structurally. HTML, truncated JSON, duplicate/escaped
decision keys, unexpected schemas and HTTP 200 service errors cannot mark success.

Primary references:

- [WiGLE CSV 1.6](https://api.wigle.net/csvFormat-1_6.html)
- [WiGLE upload response](https://github.com/wiglenet/wigle-wifi-wardriving/blob/main/wiglewifiwardriving/src/main/java/net/wigle/wigleandroid/model/api/UploadReseponse.java)
- [WiGLE endpoints](https://github.com/wiglenet/wigle-wifi-wardriving/blob/main/wiglewifiwardriving/src/main/java/net/wigle/wigleandroid/util/UrlConfig.java)
- [WDGWars client and contract](https://github.com/LOCOSP/pineapple_pager_wdgwars/tree/main/wdgwars/uploader)

Host fixtures exercise actual C codecs, leased storage/worker, SDK adapter and
touch handlers with synthetic inputs. They do not establish physical Wi-Fi/TLS
interoperability, service credit, power-loss durability or touchscreen acceptance.
