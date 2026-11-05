# Rev142: WDGWars queue support and manual service retry

The installed rev141 log details worked, but the owner could not upload again:
the worker held an uncertain WDGWars receipt with no on-device recovery route.
Rev142 adds an explicitly confirmed retry for one selected file and service.
Normal Upload/Upload All still skip acknowledged parts and hold uncertain ones.
The retry appends to the existing journal, preserves the CSV, skips acknowledged
parts of the same service and does not send to other destinations. No automatic
upload or retry is added; the single v1 JSON profile is unchanged.

Open the affected log, tap **UPLOAD THIS LOG**, then the service's
**REVIEW RETRY** button. The confirmation identifies the file and service and
explains that the earlier attempt may already have arrived. Cancel returns to
the upload confirmation, then Back returns to the file. **CONFIRM SERVICE RETRY**
requests only that service for the reviewed file. There is no bulk uncertainty
override, receipt deletion or bypass for damaged journals.

The current Marauder `develop` branch was checked on 2026-11-05 at
`79cc6bcba1db6679baee1dab2b0c1ad5010e6363`. Its `MenuFunctions.cpp` offers individual
WiGLE/WDGWars/Both actions and passes an explicit retry flag to `uploadFile`.
Its `WiFiScan.cpp` uses `/api/v2/upload-csv` and recognizes HTTP 202 for WDGWars.
Cerberus now uses that v2 endpoint with `Accept: application/json` and records
a complete `ok: true` response with a valid positive job ID as **QUEUED**.
The job ID is retained in the existing receipt. Queue admission is not proof
of processing, import counts or credit; automatic job polling is not implemented.
Synchronous 200/import-count replies remain supported. TLS CA/hostname validation,
bounded buffers, cancellation and redirect rejection remain in force.

Credit: JustCallMeKoko and the ESP32 Marauder contributors supplied the upload
workflow/API reference. Cerberus's worker, confirmation gates, receipt protocol
and strict response parsing are independent implementations. See the
[attribution record](MARAUDER_ATTRIBUTION.md), Marauder's
[uploader](https://github.com/justcallmekoko/ESP32Marauder/blob/79cc6bcba1db6679baee1dab2b0c1ad5010e6363/esp32_marauder/WiFiScan.cpp#L11997)
and [menu](https://github.com/justcallmekoko/ESP32Marauder/blob/79cc6bcba1db6679baee1dab2b0c1ad5010e6363/esp32_marauder/MenuFunctions.cpp#L4198).
The provider's [uploader reference](https://github.com/LOCOSP/pineapple_pager_wdgwars/blob/main/wdgwars/uploader/wdgwars.py)
also documents v2 job IDs and read-only upload history. A bounded authenticated
history check found no exact match for the affected generated filename among
the latest 50 entries. That is not proof of historical non-delivery; the
original uncertain attempt's HTTP response was not retained.

Focused validation passes 200 service/JSON cases, 23 actual worker/storage
scenarios and the production network/UI fixtures. Cases cover stale confirmation,
wrong file/service, disabled destination, cancellation, network failure, damaged
receipts, a receipt becoming acknowledged after review, preserving other-service
acknowledgements, two-part files and a queued retry being skipped on the next pass.
The actual rendered retry screens were checked for layout and Cancel/Back routing.
These fixtures use synthetic files and network boundaries. Full regression,
native build, installation and a real v2 acknowledgement are tracked separately.
The verified installed baseline remains [rev141](DUAL_RADIO_REV141.md) until the
new image passes installation checks.
