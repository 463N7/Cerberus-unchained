# rev147: wardrive upload clock recovery

A manual WDGWars retry reached home Wi-Fi, then timed out at `VERIFYING UTC FOR
TLS`. It never opened an HTTP connection. The saved uncertain receipt therefore
remained `CHECK ACCOUNT`; this failure did not establish an API-key rejection.

The uploader previously required a new response from `pool.ntp.org` on every
connection. It now reuses a synchronization from this boot for up to 15 minutes,
provided the wall clock still agrees with the monotonic clock within two seconds.
A reset, backward clock, excessive drift or expired sample requires another
time source. A plausible RTC date by itself is not accepted.

After leaving the Wardriving GPS view, the uploader can also use its recent,
checksum-validated RMC time and associated valid fix. It advances that time by
the measured elapsed interval. The 15-minute limit starts at the original NMEA
receipt, not at each upload. This read-only handoff does not start the UART,
retain a receiver task, or relax coordinate freshness for CSV recording.

If neither source is available, the uploader tries `pool.ntp.org` and
`time.cloudflare.com` in turn, allowing 15 seconds per server with cancellation
checks every 100 ms. Both follow normal UTC rather than mixing leap-smear
policies. The SNTP client is released after each attempt. Failure displays
`UTC TIMEOUT / LOG NOT SENT` before any service request or receipt mutation.

The time is established when starting an upload connection; Upload All keeps
the system clock running during that operation. CA, hostname and certificate
date checks remain enabled. Uploads and uncertain-result retries remain manual.
No credential, upload endpoint, CSV format or receipt format changed.

`upload_status` now includes the selected log name and each destination's
completion flag, so a saved receipt can be distinguished from the last transfer
attempt and from an incomplete multipart upload. It does not expose credentials
or response bodies.

## Validation

The focused network test covers recent-time reuse, backup-server fallback,
stale/invalid GPS dates, leap-day conversion, monotonic reversal, clock drift,
cancellation, time-setting failure and SNTP cleanup. The GPS UART test exercises
the real reader/parser with RMC followed by GGA, normal Stop, holdover expiry,
lock failure and a no-fix sentence. It confirms that time holdover does not make
stopped coordinates available for logging.

Full host checks, native builds, packaging and installation are pending for this
revision. Actual WDGWars acceptance and repeated-upload memory testing are also
pending. No logs were sent during the software checks.

The time API follows [ESP-IDF 6.0.1's system-time interface](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/system/system_time.html).
The backup server is documented in [Cloudflare's NTP guide](https://developers.cloudflare.com/time-services/ntp/usage/).
