# rev145: upload certificate verification

The last WDGWars attempt failed in the S3 TLS handshake before receiving an HTTP response. Its error did not establish an API-key rejection.

On November 7, a credential-free TLS 1.2 check found this server chain:

```text
wdgwars.pl → WE1 → GTS Root R4 → GlobalSign Root CA
```

The current firmware bundle contains GTS Root R4 and verifies WE1's signature with that key. It does not contain the old GlobalSign root. ESP-IDF's default bundle path follows the supplied cross-signed certificate to the missing root, matching [Espressif's documented failure](https://github.com/espressif/esp-idf/issues/18674).

The S3 configuration now enables cross-signed root selection and certificate date checks. The [certificate handler backport](../third_party/esp_crt_bundle/README.md) supplies Espressif's later fixes for allocation cleanup, synthetic-root dates, bundle bounds and detach behavior. The project replaces one component source during the build; the installed SDK package is unchanged. An SDK source mismatch stops configuration for review.

Uploads still require the saved home AP, current UTC, CA and hostname verification, and an explicit user action. Receipt handling and service retry confirmations are unchanged.

## Validation

The SDK integration test reproduces the old rejection and verifies the corrected chain path using real Mbed TLS/PSA cryptography. It rejects unknown roots, incorrect hostnames, expired/future leaf and intermediate certificates, and a modified signature. It completes 128 repeated verifications and 16 allocation-failure trials without retained allocations.

All 166 standard host suites and all five firmware targets passed at `e7d909a`, with unchanged input hashes. The additional SDK integration check passed separately.

A TLS 1.2 handshake from the PC, using the actual SDK verifier and firmware certificate bundle, gave these results. The test stopped after TLS; it sent no HTTP request or API credentials.

| Host | Original handler | Corrected handler |
| --- | --- | --- |
| `wdgwars.pl` | `-0x3000`, matching the recorded device error | Verified |
| `api.wigle.net` | Verified | Verified |
| `wardrift.net` | Verified | Verified |

The candidate has not been installed; the connected board still reports rev144. On-device WDGWars TLS, actual upload acknowledgement and repeated-upload memory use remain open. No logs were uploaded or receipts changed during this investigation. A saved uncertain receipt still requires the existing service retry confirmation.
