# Rev144: explain missing Wardriving logs

This checkpoint addresses a report that no Wardriving logs were visible.
The installed rev143 diagnostic captured a stopped, approximately three-second
session with 138 observations, zero SD rows, zero saved files and 138 observations
skipped for missing valid GPS/UTC. The receiver was reading checksum-valid NMEA
at 115200 baud but reported no fix or valid UTC. No service or SD error was
reported. These measurements explain why that session created no CSV; they do
not establish whether earlier files were visible on the touchscreen.

The Wardriving page now explicitly says **STOPPED / NO CSV SAVED**, retains the
reason after Stop, and distinguishes it from **CSV SAVED / OPEN LOGS BELOW**.
A later GPS fix cannot rewrite the outcome of a completed session. During a
running scan without valid GPS/UTC, the state explains that it is waiting for
GPS. Radio discovery and the existing S3/C5 division of work remain unchanged.

The log browser shows the unsuccessful last-session outcome above any older
files. An empty, successfully read catalog explains the save sequence and the
`/CERBERUS/WARD` location; a catalog failure says **LOG LIST UNAVAILABLE** rather
than claiming there are no files. Successful recordings continue to use one
`CERBWardrive###.CSV` with valid GPS/UTC rows. No placeholder coordinates,
header-only successful recordings, extra UNLOC files or automatic uploads are
introduced.

A read-only `ward_logs` diagnostic uses the same leased SD catalog as the log
browser and reports only finalized native filenames, sizes and numeric errors.
It does not read CSV rows, start the upload worker, connect to a network, delete
files or alter upload receipts. Its PSRAM allocation and SD lease are released
before console output. Active SD ownership is respected.

Regression fixtures exercise stopped sessions with GPS disabled, missing fix,
unmatched observation times, unsupported rows, no observations, and a successfully
saved CSV; later GPS recovery; an empty catalog versus a catalog error; older
files remaining visible after an unsaved session; and the actual diagnostic
catalog through leased storage, including busy and allocation failures. These
fixtures do not prove physical touchscreen acceptance or a newly recorded file.

Build, installation and on-device catalog results are recorded separately after
verification. WDGWars retry remains deferred. The user requested this bounded
fix and a pause; complete Marauder parity is not claimed.
