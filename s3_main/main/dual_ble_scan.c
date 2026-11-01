#include "dual_ble_scan.h"
#include <string.h>

enum { OFF, INFO, START, POLL, META, NAME, UUID, RAW, TIME, FENCE, TRACK, STOP, RELEASE, FAILED };

void cerberus_dual_ble_init(cerberus_dual_ble_scan_t *s, const cerberus_dual_ble_ops_t *ops)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    if (ops) s->ops = *ops;
}

bool cerberus_dual_ble_released(const cerberus_dual_ble_scan_t *s)
{ return !s || !s->start.job_id; }

esp_err_t cerberus_dual_ble_begin(cerberus_dual_ble_scan_t *s, uint8_t mode,
    const uint8_t address[6], uint8_t type)
{
    if (!s || !s->ops.exchange || mode > CERBERUS_DUAL_BLE_TRACK ||
        (mode == CERBERUS_DUAL_BLE_TRACK && (!address || type > 3U))) return ESP_ERR_INVALID_ARG;
    if (s->start.job_id || s->enabled) return ESP_ERR_INVALID_STATE;
    memset(&s->start, 0, sizeof(s->start));
    s->start.mode = mode;
    s->start.version = CERBERUS_DUAL_BLE_VERSION;
    if (mode == CERBERUS_DUAL_BLE_TRACK) {
        memcpy(s->start.address, address, 6U);
        s->start.address_type = type;
        s->has_track = false;
    }
    s->enabled = true; s->stop_requested = false; s->error = ESP_OK;
    s->phase = INFO; s->retries = 0; s->next_poll_ms = 0; s->last_tick_ms = 0;
    return ESP_OK;
}

void cerberus_dual_ble_stop(cerberus_dual_ble_scan_t *s)
{
    if (!s) return;
    s->enabled = false; s->stop_requested = true; s->next_poll_ms = 0;
    s->phase = s->start.job_id ? STOP : OFF;
}

static void fail(cerberus_dual_ble_scan_t *s, esp_err_t error)
{
    s->error = error; s->enabled = false; s->next_poll_ms = 0;
    s->phase = s->start.job_id ? STOP : FAILED;
}

static cerberus_dual_ble_binding_t binding(const cerberus_dual_ble_scan_t *s)
{ return (cerberus_dual_ble_binding_t){s->start.boot_id, s->start.job_id}; }

static bool status_valid(const cerberus_dual_ble_scan_t *s, const cerberus_dual_ble_status_t *r)
{
    return r->version == CERBERUS_DUAL_BLE_VERSION && !r->reserved &&
        r->job_id == s->start.job_id && r->state <= CERBERUS_BLE_SCAN_ERROR &&
        r->count <= CERBERUS_DUAL_BLE_CAPACITY && !(r->flags & ~(CERBERUS_DUAL_BLE_ATTACHED |
            CERBERUS_DUAL_BLE_OWNED | CERBERUS_DUAL_BLE_EXTENDED)) &&
        (!(r->flags & CERBERUS_DUAL_BLE_ATTACHED) || r->epoch != 0U);
}

static bool same_bank(const cerberus_dual_ble_scan_t *s, const cerberus_dual_ble_status_t *r)
{
    return status_valid(s, r) && r->epoch == s->remote.epoch &&
        r->generation == s->remote.generation && (r->flags & CERBERUS_DUAL_BLE_ATTACHED);
}

static esp_err_t exchange(cerberus_dual_ble_scan_t *s, uint8_t cmd,
    const void *request, size_t request_size, void *reply, size_t reply_size)
{
    memset(reply, 0, reply_size);
    return s->ops.exchange(s->ops.context, cmd, request, request_size, reply, reply_size);
}

static void no_owner(cerberus_dual_ble_scan_t *s)
{
    s->start.job_id = 0U; memset(&s->remote, 0, sizeof(s->remote));
    s->retries = 0U; s->next_poll_ms = 0U;
    s->phase = s->enabled ? INFO : s->error ? FAILED : OFF;
}

static void step(cerberus_dual_ble_scan_t *s, uint64_t now)
{
    esp_err_t error = ESP_OK;
    cerberus_dual_ble_binding_t b = binding(s);
    cerberus_dual_ble_status_t status = {0};
    uint8_t phase = s->phase;
    if (phase == OFF || phase == FAILED || now < s->next_poll_ms) return;
    if (phase == INFO) {
        cerberus_dual_ble_info_t info;
        error = exchange(s, CERBERUS_SPI_COMMAND_DUAL_BLE_INFO, NULL, 0U, &info, sizeof(info));
        if (error == ESP_OK) {
            bool reserved = false;
            for (size_t i = 0; i < sizeof(info.reserved); ++i) reserved |= info.reserved[i] != 0;
            if (!info.boot_id || info.version != CERBERUS_DUAL_BLE_VERSION || reserved ||
                info.capacity != CERBERUS_DUAL_BLE_CAPACITY || info.raw_bytes != CERBERUS_DUAL_BLE_RAW_BYTES ||
                (info.flags & ~(CERBERUS_DUAL_BLE_READY | CERBERUS_DUAL_BLE_EXTENDED | CERBERUS_DUAL_BLE_ATTACHED)))
                error = ESP_ERR_INVALID_RESPONSE;
            else if (!(info.flags & CERBERUS_DUAL_BLE_READY) || (info.flags & CERBERUS_DUAL_BLE_ATTACHED))
                error = ESP_ERR_INVALID_STATE;
            else if (!info.next_job_id) error = ESP_ERR_INVALID_STATE;
            else {
                s->start.boot_id = info.boot_id; s->start.job_id = info.next_job_id;
                s->job_counter = info.next_job_id;
                s->extended = (info.flags & CERBERUS_DUAL_BLE_EXTENDED) != 0;
                s->phase = START;
            }
        }
    } else if (phase == START) {
        error = exchange(s, CERBERUS_SPI_COMMAND_DUAL_BLE_START, &s->start, sizeof(s->start), &status, sizeof(status));
        if (error == ESP_OK) {
            if (!status_valid(s, &status)) error = ESP_ERR_INVALID_RESPONSE;
            else if (!(status.flags & CERBERUS_DUAL_BLE_ATTACHED)) {
                s->start.job_id = 0U; error = status.error_code ? status.error_code : ESP_ERR_INVALID_RESPONSE;
            } else { s->remote = status; s->phase = POLL; }
        }
    } else if (phase == POLL || phase == FENCE || phase == STOP || phase == RELEASE) {
        uint8_t cmd = phase == STOP ? CERBERUS_SPI_COMMAND_DUAL_BLE_STOP :
            phase == RELEASE ? CERBERUS_SPI_COMMAND_DUAL_BLE_RELEASE : CERBERUS_SPI_COMMAND_DUAL_BLE_STATUS;
        error = exchange(s, cmd, &b, sizeof(b), &status, sizeof(status));
        if (error == ESP_OK) {
            if (!status_valid(s, &status)) error = ESP_ERR_INVALID_RESPONSE;
            else if (status.error_code == ESP_ERR_NOT_FOUND && !(status.flags & CERBERUS_DUAL_BLE_ATTACHED)) {
                if (phase != STOP && phase != RELEASE) { s->error = ESP_ERR_INVALID_STATE; s->enabled = false; }
                no_owner(s); return;
            } else if (phase == RELEASE && !(status.flags & CERBERUS_DUAL_BLE_ATTACHED) &&
                status.epoch == s->remote.epoch && !status.error_code) { no_owner(s); return; }
            else {
                /* A START may reach C5 even when all its replies are lost.
                 * The bound STOP reply may establish cleanup identity only;
                 * it must never publish that unacknowledged result bank. */
                if (phase == STOP && !s->remote.epoch &&
                    (status.flags & CERBERUS_DUAL_BLE_ATTACHED)) s->remote = status;
                if (!same_bank(s, &status)) error = ESP_ERR_INVALID_RESPONSE;
                else if (phase == STOP) {
                s->remote = status;
                s->phase = status.flags & CERBERUS_DUAL_BLE_OWNED ? STOP : RELEASE;
                s->next_poll_ms = status.flags & CERBERUS_DUAL_BLE_OWNED ? now + 250U : 0U;
            } else if (status.error_code || status.state == CERBERUS_BLE_SCAN_ERROR) {
                error = status.error_code ? status.error_code : ESP_FAIL;
            } else if (phase == FENCE) {
                if (status.state != CERBERUS_BLE_SCAN_COMPLETE || (status.flags & CERBERUS_DUAL_BLE_OWNED) ||
                    status.count != s->remote.count) error = ESP_ERR_INVALID_RESPONSE;
                else {
                    memcpy(s->rows, s->staging, sizeof(s->rows)); s->count = status.count;
                    s->published_epoch = status.epoch;
                    if (s->sweeps != UINT32_MAX) ++s->sweeps;
                    s->phase = RELEASE;
                }
            } else if (status.state != CERBERUS_BLE_SCAN_RUNNING &&
                status.state != CERBERUS_BLE_SCAN_COMPLETE) error = ESP_ERR_INVALID_RESPONSE;
            else {
                s->remote = status;
                if (s->start.mode == CERBERUS_DUAL_BLE_TRACK) s->phase = TRACK;
                else if (status.state == CERBERUS_BLE_SCAN_COMPLETE && !(status.flags & CERBERUS_DUAL_BLE_OWNED)) {
                    memset(s->staging, 0, sizeof(s->staging)); s->read_index = 0U;
                    s->phase = status.count ? META : FENCE;
                } else { s->next_poll_ms = now + 250U; }
            }
            }
        }
    } else if (phase == TRACK) {
        cerberus_dual_ble_track_t track;
        error = exchange(s, CERBERUS_SPI_COMMAND_DUAL_BLE_TRACK_STATUS, &b, sizeof(b), &track, sizeof(track));
        if (error == ESP_OK) {
            if (track.job_id != s->start.job_id || track.epoch != s->remote.epoch ||
                (track.flags & ~(CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID)) ||
                ((track.flags & CERBERUS_DUAL_BLE_RSSI_VALID) && (track.rssi < -127 || track.rssi > 20)))
                error = ESP_ERR_INVALID_RESPONSE;
            else {
                if (track.flags & CERBERUS_DUAL_BLE_SEEN) { s->track = track; s->track_request_ms = now; s->has_track = true; }
                if (s->remote.state == CERBERUS_BLE_SCAN_COMPLETE && !(s->remote.flags & CERBERUS_DUAL_BLE_OWNED)) {
                    if (s->sweeps != UINT32_MAX) ++s->sweeps;
                    s->phase = RELEASE;
                } else { s->phase = POLL; s->next_poll_ms = now + 250U; }
            }
        }
    } else if (phase == META) {
        cerberus_ble_scan_result_request_t request = {s->remote.generation, s->read_index};
        error = exchange(s, CERBERUS_SPI_COMMAND_BLE_SCAN_RESULT, &request, sizeof(request), &s->metadata, sizeof(s->metadata));
        if (error == ESP_OK) {
            const cerberus_ble_scan_result_payload_t *m = &s->metadata;
            if (m->generation != s->remote.generation || m->index != s->read_index || m->address_type > 3U ||
                m->name_length > CERBERUS_BLE_NAME_MAX_LENGTH || (m->uuid_length && m->uuid_length != 2U && m->uuid_length != 4U && m->uuid_length != 16U) ||
                m->advertisement_length > CERBERUS_DUAL_BLE_RAW_BYTES || m->advertisement_event_type > 4U ||
                (m->advertisement_flags & ~(CERBERUS_BLE_ADVERTISEMENT_TRUNCATED | CERBERUS_BLE_ADVERTISEMENT_MALFORMED)) || m->reserved[0] || m->reserved[1])
                error = ESP_ERR_INVALID_RESPONSE;
            else {
                cerberus_dual_ble_row_t *row = &s->staging[s->read_index];
                memcpy(row->address, m->address, 6U); row->address_type = m->address_type;
                row->flags = m->flags; row->manufacturer_id = m->manufacturer_id; row->reports = m->reports_seen;
                row->uuid_length = m->uuid_length; row->raw_length = m->advertisement_length;
                row->raw_flags = m->advertisement_flags; row->event_type = m->advertisement_event_type;
                s->copied = 0U; s->phase = NAME;
            }
        }
    } else if (phase == NAME || phase == UUID || phase == RAW) {
        cerberus_dual_ble_row_t *row = &s->staging[s->read_index];
        uint8_t length = phase == NAME ? s->metadata.name_length : phase == UUID ? row->uuid_length : row->raw_length;
        if (s->copied == length) {
            if (phase == NAME) for (uint8_t i = 0U; i < length; ++i)
                if ((unsigned char)row->name[i] < 32U || (unsigned char)row->name[i] > 126U) row->name[i] = '?';
            s->copied = 0U; ++s->phase; return;
        }
        uint8_t kind = phase == NAME ? CERBERUS_BLE_DATA_NAME : phase == UUID ? CERBERUS_BLE_DATA_SERVICE_UUID : CERBERUS_BLE_DATA_RAW_ADVERTISEMENT;
        cerberus_ble_scan_data_request_t request = {s->remote.generation, s->read_index, kind, s->copied / CERBERUS_BLE_DATA_CHUNK_SIZE};
        cerberus_ble_scan_data_payload_t data;
        error = exchange(s, CERBERUS_SPI_COMMAND_BLE_SCAN_DATA, &request, sizeof(request), &data, sizeof(data));
        if (error == ESP_OK) {
            uint8_t amount = length - s->copied;
            if (amount > CERBERUS_BLE_DATA_CHUNK_SIZE) amount = CERBERUS_BLE_DATA_CHUNK_SIZE;
            if (data.generation != request.generation || data.index != request.index || data.kind != kind ||
                data.chunk != request.chunk || data.byte_count != amount) error = ESP_ERR_INVALID_RESPONSE;
            else {
                uint8_t *destination = phase == NAME ? (uint8_t *)row->name : phase == UUID ? row->uuid : row->raw;
                memcpy(destination + s->copied, data.bytes, amount); s->copied += amount;
            }
        }
    } else if (phase == TIME) {
        cerberus_dual_ble_row_request_t request = {b, s->read_index, 0U};
        cerberus_dual_ble_row_time_t stamp;
        error = exchange(s, CERBERUS_SPI_COMMAND_DUAL_BLE_ROW_TIME, &request, sizeof(request), &stamp, sizeof(stamp));
        if (error == ESP_OK) {
            if (stamp.epoch != s->remote.epoch || stamp.job_id != s->start.job_id || stamp.index != s->read_index ||
                stamp.generation != s->remote.generation || stamp.reports != s->metadata.reports_seen ||
                (stamp.flags & ~(CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID)) ||
                ((stamp.flags & CERBERUS_DUAL_BLE_RSSI_VALID) && (stamp.latest_rssi < -127 || stamp.latest_rssi > 20)))
                error = ESP_ERR_INVALID_RESPONSE;
            else {
                cerberus_dual_ble_row_t *row = &s->staging[s->read_index];
                row->rssi = stamp.latest_rssi; row->timing_flags = stamp.flags;
                row->callback_age_ms = stamp.age_ms; row->request_started_ms = now;
                ++s->read_index; s->phase = s->read_index < s->remote.count ? META : FENCE;
            }
        }
    }
    if (error != ESP_OK) {
        if (phase == STOP || phase == RELEASE) { s->error = error; s->phase = STOP; s->next_poll_ms = now + 250U; }
        else if (error == ESP_ERR_TIMEOUT && s->retries++ < 2U) { s->next_poll_ms = now + 100U; }
        else fail(s, error);
    } else s->retries = 0U;
}

void cerberus_dual_ble_tick(cerberus_dual_ble_scan_t *s, uint64_t now)
{
    if (!s || !s->ops.exchange) return;
    if (now < s->last_tick_ms) { fail(s, ESP_ERR_INVALID_STATE); return; }
    s->last_tick_ms = now;
    uint64_t started = now;
    for (unsigned i = 0; i < 4U; ++i) {
        if (s->ops.now_ms) now = s->ops.now_ms(s->ops.context);
        if (now < started || now - started >= 8U) break;
        uint8_t before = s->phase;
        step(s, now);
        if (s->phase == before || s->phase == OFF || s->phase == FAILED || now < s->next_poll_ms) break;
    }
}

void cerberus_dual_ble_view(const cerberus_dual_ble_scan_t *s, cerberus_dual_ble_view_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out)); if (!s) return;
    out->enabled = s->enabled; out->attached = s->start.job_id != 0U;
    out->receiving = (s->remote.flags & CERBERUS_DUAL_BLE_OWNED) != 0;
    out->reading = s->phase >= META && s->phase <= FENCE;
    out->stopping = s->phase == STOP || s->phase == RELEASE;
    out->extended = s->extended; out->count = s->count; out->sweeps = s->sweeps; out->error = s->error;
}

bool cerberus_dual_ble_row(const cerberus_dual_ble_scan_t *s, uint8_t index, cerberus_dual_ble_row_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out)); if (!s || index >= s->count) return false;
    *out = s->rows[index]; return true;
}

void cerberus_dual_ble_track_view(const cerberus_dual_ble_scan_t *s, uint64_t now,
    cerberus_dual_ble_track_view_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out)); if (!s || !s->has_track) return;
    out->seen = (s->track.flags & (CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID)) ==
        (CERBERUS_DUAL_BLE_SEEN | CERBERUS_DUAL_BLE_RSSI_VALID);
    out->rssi = s->track.rssi; out->reports = s->track.reports;
    out->age_upper_ms = cerberus_dual_ble_age_upper(s->track.age_ms, s->track_request_ms, now);
    out->fresh = out->seen && out->age_upper_ms <= CERBERUS_DUAL_BLE_STALE_MS;
}
