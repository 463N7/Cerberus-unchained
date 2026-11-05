# Rev140: one private upload profile

This milestone adds a single JSON profile for home Wi-Fi, WiGLE API name/token,
WDGWars token and Wardrift token to the existing Wardriving upload setup. It
changes no scan allocation or attack behavior. S3 owns parsing, NVS and SD;
C5 remains available for its existing receive work after this branch closes.

The [blank example](../config_examples/wardrive_upload.example.json) and
[setup guide](WARDRIVE_UPLOADS.md) describe the exact schema and SD path.
**IMPORT JSON FROM SD** appears in the existing setup screen and confirms
replacement of the whole saved profile. Parsing checks the complete bounded
JSON, exact fields, UTF-8, field lengths and enabled-service readiness before
writing. The worker verifies NVS readback before removing the temporary SD
copy. Failures retain the file; saved-but-not-removed is a distinct result.
No connection or upload starts as a side effect of setup.

The parser reuses the bounded tokenizer used for service replies; it does not
add a general-purpose JSON heap or a persistent task. Existing upload worker
scratch is cleared after parsing, and temporary decoded/settings copies are
cleared after use. The read-only `upload_profile` console command exposes only
readiness flags and the destination mask. The public example has no credentials;
filled local profiles and physical evidence remain outside Git.

Focused checks cover 52 JSON/encoding/bounds cases, the actual NVS adapter and
secret-free diagnostic, 173 existing upload-protocol cases, actual setup/import
touch handlers/rendering, and 12 manager/storage lifecycle cases. Import cases
include invalid input, save failure, readback failure, removal failure and
cancellation after a completed save. They exercise the real worker and leased
storage with synthetic credentials, without accessing a live service.

Full build, package, installation and physical import evidence will be recorded
at the checkpoint after validation. The measured capture and log-navigation
baselines remain [rev138](DUAL_RADIO_REV138.md) and [rev139](DUAL_RADIO_REV139.md).
Live home association/TLS, provider processing/credit and upload cancellation
remain separate acceptance tasks. The Wardrift adapter remains provisional.
This is not a claim of complete Marauder parity or encrypted credential storage.
