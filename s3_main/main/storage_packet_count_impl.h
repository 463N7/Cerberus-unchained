/* Included in storage.c: all state and file operations share storage_mutex. */
static esp_err_t packet_count_storage_path(char path[128], unsigned source, bool final)
{
    if (!sd_packet_count || source > 1U) return ESP_ERR_INVALID_STATE;
    int n = snprintf(path, 128U, "%s/%s.%s", sd_packet_count->directory,
        source ? "C5" : "S3", final ? "PCAPNG" : "TMP");
    return n > 0 && n < 128 ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t cerberus_storage_packet_count_begin(uint64_t job, char name[8])
{
    if (!job || !name) return ESP_ERR_INVALID_ARG;
    name[0] = '\0';
    esp_err_t error = take_storage_mutex();
    if (error != ESP_OK) return error;
    if (!ble_archive_storage_available()) error = ESP_ERR_INVALID_STATE;
    packet_count_storage_t *context = NULL;
    if (error == ESP_OK) {
        context = heap_caps_calloc(1U, sizeof(*context), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!context) error = ESP_ERR_NO_MEM;
    }
    if (error == ESP_OK) error = recon_storage_directory(CERBERUS_STORAGE_MOUNT_PATH "/CERBERUS");
    if (error == ESP_OK) error = recon_storage_directory(
        CERBERUS_STORAGE_MOUNT_PATH CERBERUS_STORAGE_PACKET_COUNT_PATH);
    if (error == ESP_OK) {
        bool created = false;
        for (unsigned attempt = 0; attempt < 1024U; ++attempt) {
            unsigned index = sd_packet_count_next_index;
            sd_packet_count_next_index = index == 99999U ? 1U : index + 1U;
            snprintf(context->directory, sizeof(context->directory),
                CERBERUS_STORAGE_MOUNT_PATH CERBERUS_STORAGE_PACKET_COUNT_PATH "/P%05u", index);
            if (mkdir(context->directory, 0775) == 0) {
                snprintf(name, 8U, "P%05u", index);
                created = true;
                break;
            }
            if (errno != EEXIST) { error = ESP_FAIL; break; }
        }
        if (error == ESP_OK && !created) error = ESP_ERR_NOT_FOUND;
    }
    if (error == ESP_OK) {
        context->job = job;
        sd_packet_count = context;
    } else if (context) heap_caps_free(context);
    release_storage_mutex();
    return error;
}

static esp_err_t packet_count_storage_write(packet_count_storage_stream_t *stream,
    const uint8_t *data, size_t length)
{
    if (stream->error != ESP_OK) return stream->error;
    if (!stream->file) return stream->error = ESP_ERR_INVALID_STATE;
    if (stream->bytes > CERBERUS_STORAGE_WIFI_CAPTURE_MAX_BYTES ||
        length > CERBERUS_STORAGE_WIFI_CAPTURE_MAX_BYTES - stream->bytes)
        return stream->error = ESP_ERR_INVALID_SIZE;
    size_t wrote = fwrite(data, 1U, length, stream->file);
    stream->bytes += wrote;
    if (wrote != length || ferror(stream->file)) return stream->error = ESP_FAIL;
    int64_t now = esp_timer_get_time();
    if (++stream->unsynced >= 32U || now < stream->last_sync_us ||
        now - stream->last_sync_us >= INT64_C(1000000)) {
        if (!recon_storage_flush(stream->file)) return stream->error = ESP_FAIL;
        stream->last_sync_us = now;
        stream->unsynced = 0U;
    }
    return ESP_OK;
}

esp_err_t cerberus_storage_packet_count_open(uint64_t job, unsigned source,
    const uint8_t *header, size_t length)
{
    if (!job || source > 1U || !header || length != 68U) return ESP_ERR_INVALID_ARG;
    esp_err_t error = take_storage_mutex();
    if (error != ESP_OK) return error;
    if (!sd_packet_count || sd_packet_count->job != job ||
        require_firmware_owner_locked() != ESP_OK) error = ESP_ERR_INVALID_STATE;
    if (error == ESP_OK) {
        packet_count_storage_stream_t *stream = &sd_packet_count->source[source];
        if (stream->opened) error = stream->error;
        else {
            char path[128];
            error = packet_count_storage_path(path, source, false);
            if (error == ESP_OK) {
                int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0664);
                if (fd < 0) error = ESP_FAIL;
                else {
                    stream->opened = true;
                    stream->file = fdopen(fd, "wb");
                    if (!stream->file) { (void)close(fd); error = ESP_FAIL; }
                    else error = packet_count_storage_write(stream, header, length);
                }
            }
            if (error != ESP_OK) stream->error = error;
        }
    }
    release_storage_mutex();
    return error;
}

esp_err_t cerberus_storage_packet_count_append(uint64_t job, unsigned source,
    const uint8_t *record, size_t length)
{
    if (!job || source > 1U || !record || !length || length > 4144U) return ESP_ERR_INVALID_ARG;
    esp_err_t error = take_storage_mutex();
    if (error != ESP_OK) return error;
    if (!sd_packet_count || sd_packet_count->job != job ||
        require_firmware_owner_locked() != ESP_OK) error = ESP_ERR_INVALID_STATE;
    else error = packet_count_storage_write(&sd_packet_count->source[source], record, length);
    release_storage_mutex();
    return error;
}

esp_err_t cerberus_storage_packet_count_finish(uint64_t job,
    cerberus_storage_packet_count_result_t *result)
{
    if (!job || !result) return ESP_ERR_INVALID_ARG;
    memset(result, 0, sizeof(*result));
    esp_err_t error = ensure_storage_mutex();
    if (error != ESP_OK) return error;
    if (xSemaphoreTake(storage_mutex, portMAX_DELAY) != pdTRUE) return ESP_ERR_TIMEOUT;
    if (!sd_packet_count || sd_packet_count->job != job) {
        release_storage_mutex();
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned source = 0; source < 2U; ++source) {
        packet_count_storage_stream_t *stream = &sd_packet_count->source[source];
        if (stream->file) {
            if (!recon_storage_flush(stream->file)) stream->error = ESP_FAIL;
            FILE *owned = stream->file;
            stream->file = NULL;
            if (fclose(owned) != 0) stream->error = ESP_FAIL;
        }
    }
    /* Close both receiver files before publishing either result. */
    for (unsigned source = 0; source < 2U; ++source) {
        packet_count_storage_stream_t *stream = &sd_packet_count->source[source];
        if (stream->opened) {
            snprintf(result->file_name[source], sizeof(result->file_name[source]),
                "%s.TMP", source ? "C5" : "S3");
            char temporary[128], final[128];
            if (stream->error == ESP_OK) {
                stream->error = packet_count_storage_path(temporary, source, false);
                if (stream->error == ESP_OK) stream->error = packet_count_storage_path(final, source, true);
                if (stream->error == ESP_OK) stream->error = recon_storage_absent(final);
                if (stream->error == ESP_OK && rename(temporary, final) != 0) stream->error = ESP_FAIL;
                if (stream->error == ESP_OK) snprintf(result->file_name[source],
                    sizeof(result->file_name[source]), "%s.PCAPNG", source ? "C5" : "S3");
            }
        }
        result->bytes[source] = stream->bytes;
        result->error[source] = stream->error;
        if (error == ESP_OK) error = stream->error;
    }
    heap_caps_free(sd_packet_count);
    sd_packet_count = NULL;
    release_storage_mutex();
    return error;
}
