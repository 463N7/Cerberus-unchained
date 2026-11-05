# Rev143: one CERBWardrive CSV and direct upload recovery

The owner found the separate WIGLE/UNLOC outputs confusing and requested one
Marauder-style WiGLE-compatible CSV named CERBWardrive for each recording.
Production Wardriving now lazily opens **CERBWardrive001.CSV** through
**CERBWardrive999.CSV** when its first qualifying observation arrives. Wi-Fi and
BLE rows from both onboard receivers, and located expansion observations, share
that one file. A GPS outage does not open another file or stop radio discovery.
Only observations with valid position, UTC, supported fields and acceptable RF
age enter the CSV. Repeated qualifying observations remain separate rows.

Missing fix/UTC, uncertain RF age and unsupported CSV observations have separate
counters. The screen displays the total not saved and the no-fix count, plus a
scan-only/waiting-for-fix message when appropriate. No placeholder coordinates
or receipt-time substitutes are added to an old RF observation. A session with
no qualifying rows creates no CSV. The existing 64 MiB per-file bound remains:
reaching it stops the session and preserves successfully written rows in its
single file, with a size-limit message, instead of silently rotating another
CSV. Write/sync failures retain recoverable TMP files rather than publishing
them as complete. Legacy non-production host fixtures retain their historical
evidence-stream tests; the ESP-IDF task always selects the production stream.

The log catalog accepts the longer names and shows new CERBWardrive recordings
before legacy WIGLE and UNLOC files. Old recordings are not renamed, rewritten
or deleted. UNLOC entries are labeled **LEGACY NO GPS**, and their details
explain that they cannot upload. The catalog is bounded to 2,997 possible native
names, with its entries allocated in PSRAM. Upload part names preserve the full
CERBWardrive stem. Existing receipt identities are unchanged.

For a selected log with one enabled uncertain service, the primary details
button now reads **RETRY WDGWARS UPLOAD** (or the corresponding service). It opens
the existing file/service confirmation directly. Cancel returns to the log.
If several services are uncertain, the upload confirmation asks the user to
choose one; its main button cannot accidentally issue an ordinary upload that
immediately hits the same hold. No HTTP request occurs before confirmation.
Other services and acknowledged parts remain skipped during a service retry.
Uploads remain manual and use the existing single v1 configuration profile.

The CSV schema follows WiGLE's [official v1.6 specification](https://api.wigle.net/csvFormat-1_6.html),
including the pre-header, Earth metadata, fixed 14 columns, UTC and CSV escaping.
JustCallMeKoko and the Marauder contributors' [WiFiScan.cpp](https://github.com/justcallmekoko/ESP32Marauder/blob/79cc6bcba1db6679baee1dab2b0c1ad5010e6363/esp32_marauder/WiFiScan.cpp#L6409)
is the behavioral reference for a shared WiGLE-compatible wardrive stream.
Cerberus's implementation and strict GPS/UTC gates are independent. Unlike that
reference's MAC history, the onboard stream continues to retain repeated valid
sightings, as requested. Format conformance is not proof of provider processing
or account credit.

Focused checks pass for GPS loss/recovery within one stream, no extra UNLOC
file, equal sightings, stale observations, full-file stop, longer-name parser
boundaries, actual storage/manager upload/delete paths, and rendered navigation.
The worker suite covers 24 scenarios, including the new name through the actual
storage and receipt lifecycle. Native builds, full regression, installation,
physical single-file capture and live WDGWars v2 acceptance are tracked
separately. The installed baseline is [rev142](DUAL_RADIO_REV142.md).
