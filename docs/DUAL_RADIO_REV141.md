# Rev141: single-profile automatic uploads and receipt error clarity

This update extends the existing Wardriving branch. One JSON profile holds home
Wi-Fi, WiGLE API name/token, WDGWars token, Wardrift token, destination switches
and `auto_upload_after_stop`. The public example is blank and defaults to manual.
V1 JSON/NVS profiles remain readable with automatic mode OFF. No private values
are compiled into firmware or included in this contribution.

An opt-in automatic pass is armed only by explicit Wardriving Stop. Capture must
reach STOPPED before the existing log worker opens SD, connects to the saved AP
and processes located logs. There is no extra task or network polling while
recording. S3 owns the upload; the C5 capture owner is released first. Back, sleep,
errors and a new Start cancel pending triggers. The worker rechecks the saved
policy, and automatic passes never resend rejected or uncertain parts.

The reported rev140 error was separate from profile import. Read-only device
flags confirmed saved home/WiGLE/WDGWars settings; physical SD inspection confirmed
the imported JSON was removed. The recorded CSV hash was unchanged. Its journals
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

Focused host checks cover v1 migration without accidental opt-in, 60 strict JSON
cases, 17 actual worker/storage scenarios, the mixed queued/uncertain retry,
automatic policy enforcement, rejection/uncertainty holds, stop cleanup gates,
sleep/Back/new-Start cancellation, HTTP diagnostics and actual UI rendering.
These use synthetic inputs and do not establish physical automatic-upload
acceptance. Native/full-regression and installation evidence will be recorded
separately after their checks finish.

The [upload guide](WARDRIVE_UPLOADS.md) contains setup, limits and retry behavior.
The [rev140 checkpoint](DUAL_RADIO_REV140.md) retains installation evidence.
Wardrift remains provisional; full Marauder parity and longer capture/upload
endurance are outside this checkpoint.
