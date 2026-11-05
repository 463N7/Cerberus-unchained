# Rev141: manual uploads and receipt error clarity

This update keeps Upload and Upload All manual, with the existing explicit
confirmation. One v1 JSON profile holds home Wi-Fi, WiGLE API name/token,
WDGWars token, Wardrift token and destination switches. The public example is
blank. Importing settings, stopping capture or rebooting never starts an upload.
No private values are compiled into firmware or included in this contribution.
Capture must stop before the existing worker takes SD and S3 home Wi-Fi/TLS
ownership. There is no additional task or network polling while recording.

The reported rev140 error was separate from profile import. Read-only device
flags confirmed saved home/WiGLE/WDGWars settings; physical SD inspection confirmed
the imported JSON was removed. A guarded restart preserved the saved readiness
flags and destination mask, with C5 online and no previous-reset fault. That check
ran with the card on the PC and did not connect or upload. The recorded CSV hash
was unchanged. Its journals
contained WiGLE SENDING→QUEUED and WDGWars SENDING→UNKNOWN. Both journals had valid
lengths and CRCs. A documented read-only WiGLE API query found the matching queued
transaction. A WDGWars `/api/me` query authenticated the key, but does not prove
acceptance of that CSV. No retry, receipt deletion or new upload was performed
by these checks. Provider processing/credit and the original WDGWars HTTP failure
remain unresolved; the rev140 journal did not retain that cause.

The worker now names the service when a saved uncertain receipt blocks a retry,
instead of leaving `VALIDATING SAVED CSV` above `ESP_ERR_INVALID_STATE`. New
attempts expose HTTP status, response length/completeness and transport error,
without credentials, response text or transaction IDs. Home Wi-Fi initialization
stages are identified before the operation that may fail. `upload_status` is a
read-only console snapshot; current-attempt diagnostics are not persisted.

Focused host checks cover 52 strict JSON cases, 13 actual worker/storage
scenarios, the mixed queued/uncertain retry, uncertainty holds, cleanup, HTTP
diagnostics and actual UI rendering. These use synthetic inputs and do not
establish provider delivery or physical uploader cancellation. Native/full
regression and installation evidence will be recorded separately after their
checks finish.

The [upload guide](WARDRIVE_UPLOADS.md) contains setup, limits and retry behavior.
The [rev140 checkpoint](DUAL_RADIO_REV140.md) retains installation evidence.
Wardrift remains provisional; full Marauder parity and longer capture/upload
endurance are outside this checkpoint.
