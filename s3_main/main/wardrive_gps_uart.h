#pragma once

#include "esp_err.h"
#include "wardrive_gps_core.h"

/* 9600 baud produces at most 960 bytes/s with 8N1 framing. The reader polls
 * every 100ms; this ring retains over one second of input without a 4KiB pool. */
#define CERBERUS_WARDRIVE_GPS_RX_BUFFER_BYTES 1024U
#define CERBERUS_WARDRIVE_GPS_TASK_STACK_BYTES 3584U
#define CERBERUS_WARDRIVE_GPS_TRACE_CAPACITY 8U
#define CERBERUS_WARDRIVE_GPS_TRACE_PAYLOAD_BYTES (CERBERUS_WARDRIVE_GPS_LINE_CAPACITY - 1U)

enum {
    CERBERUS_WARDRIVE_GPS_TRACE_PAYLOAD_COMPLETE = 1U,
    CERBERUS_WARDRIVE_GPS_TRACE_RECEIPT_VALID = 2U,
    CERBERUS_WARDRIVE_GPS_TRACE_MASKED = 4U,
};

typedef struct {
    uint64_t sequence;
    uint64_t receiver_epoch;
    uint64_t received_ms; /* UART read receipt, not GNSS fix/capture time. */
    uint32_t total_length;
    uint8_t retained_length;
    uint8_t result; /* cerberus_wardrive_gps_line_result_t */
    uint8_t termination; /* cerberus_wardrive_gps_termination_t */
    uint8_t flags;
    uint8_t payload[CERBERUS_WARDRIVE_GPS_TRACE_PAYLOAD_BYTES];
} cerberus_wardrive_gps_trace_row_t;

typedef struct {
    cerberus_wardrive_gps_snapshot_t gps;
    uint64_t receiver_epoch;
    esp_err_t error; /* Current failure while resources remain held. */
    esp_err_t historical_error; /* Last failure after clean release. */
    bool driver_active;
    bool reader_running;
    bool starting;
    bool stopping;
    bool clock_valid;
} cerberus_wardrive_gps_uart_state_t;

typedef struct {
    uint64_t observer_token;
    uint64_t receiver_epoch;
    /* Evictions count retained rows replaced by newer rows. Discarded counts
     * overflow/control/frame-invalid input, not all checksum/field failures
     * and not bytes lost before the UART reader. Those outcomes remain per row. */
    uint32_t observed_lines;
    uint32_t evicted_lines;
    uint32_t overflow_lines;
    uint32_t discarded_lines;
    uint8_t count;
    bool capturing;
    bool sequence_exhausted;
    cerberus_wardrive_gps_trace_row_t rows[CERBERUS_WARDRIVE_GPS_TRACE_CAPACITY];
} cerberus_wardrive_gps_trace_snapshot_t;

/* Optional receiver on UART2 RX GPIO1, 9600 8N1. The reader and UART driver
 * exist only during an integrated Wardriving session. Start/Stop are serialized
 * by that service's worker; observers never control the driver. A failed Stop
 * retains ownership for an exact same-owner retry. No PPS pin is claimed. */
esp_err_t cerberus_wardrive_gps_uart_start(void);
esp_err_t cerberus_wardrive_gps_uart_stop(void);
void cerberus_wardrive_gps_uart_snapshot(
    cerberus_wardrive_gps_snapshot_t *out, esp_err_t *uart_error);

/* Value-only local state. No UART operations, allocation or cleanup. */
esp_err_t cerberus_wardrive_gps_uart_get_state(cerberus_wardrive_gps_uart_state_t *out);

/* An explicit observer of an already-running reader. OFF/starting/stopping
 * returns NOT_FINISHED without allocation. Only one observer may attach.
 * The eight-row SPIRAM bank has no internal fallback/per-line allocation.
 * Opening, reading and ending never start or stop UART/radios/logging.
 * Read returns oldest-to-newest values. Masking zeroes every payload and
 * retained_length, including unknown/malformed lines. Stop retains historical
 * rows; a new receiver session clears them without reusing sequence numbers.
 * End requires the exact token and releases only this observer's storage.
 * Observer lock attempts are bounded; End TIMEOUT retains the token/storage
 * for an exact retry. A sequence-exhausted ring is retained, not capturing. */
esp_err_t cerberus_wardrive_gps_uart_trace_begin(uint64_t *observer_token);
esp_err_t cerberus_wardrive_gps_uart_trace_read(uint64_t observer_token,
    bool mask, cerberus_wardrive_gps_trace_snapshot_t *out);
esp_err_t cerberus_wardrive_gps_uart_trace_end(uint64_t observer_token);
