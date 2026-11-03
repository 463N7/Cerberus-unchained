/* Included after the common passive SPI helpers. No automatic mutation retry. */
static size_t packet_count_request_size(uint8_t command)
{
    switch (command) {
    case CERBERUS_PACKET_COUNT_COMMAND_INFO: return 0U;
    case CERBERUS_PACKET_COUNT_COMMAND_BEGIN: return sizeof(cerberus_packet_count_begin_t);
    case CERBERUS_PACKET_COUNT_COMMAND_TARGET: return sizeof(cerberus_packet_count_target_t);
    case CERBERUS_PACKET_COUNT_COMMAND_START:
    case CERBERUS_PACKET_COUNT_COMMAND_STATUS:
    case CERBERUS_PACKET_COUNT_COMMAND_STOP:
    case CERBERUS_PACKET_COUNT_COMMAND_RELEASE: return sizeof(cerberus_packet_count_request_t);
    case CERBERUS_PACKET_COUNT_COMMAND_PEEK: return sizeof(cerberus_packet_count_peek_t);
    case CERBERUS_PACKET_COUNT_COMMAND_READ:
    case CERBERUS_PACKET_COUNT_COMMAND_RAW_READ: return sizeof(cerberus_packet_count_read_t);
    case CERBERUS_PACKET_COUNT_COMMAND_RAW_ACK: return sizeof(cerberus_packet_count_ack_t);
    default: return SIZE_MAX;
    }
}

esp_err_t c5_spi_link_wifi_packet_count_exchange(uint8_t command, const void *request,
    size_t request_size, void *reply, size_t reply_size)
{
    bool raw = command == CERBERUS_PACKET_COUNT_COMMAND_RAW_READ;
    size_t expected = packet_count_request_size(command);
    if (!reply || reply_size != (raw ? sizeof(cerberus_packet_count_raw_chunk_t) :
        sizeof(cerberus_packet_count_control_t)) || expected == SIZE_MAX ||
        request_size != expected || (expected ? !request : request != NULL)) return ESP_ERR_INVALID_ARG;
    uint8_t query[CERBERUS_SPI_FRAME_PAYLOAD_SIZE] = {0};
    if (expected) memcpy(query, request, expected);
    memset(reply, 0, reply_size);
    uint64_t session = 0U;
    if (expected) {
        memcpy(&session, query, sizeof(session));
        if (!session) return ESP_ERR_INVALID_ARG;
    }
    if (!link_mutex) return ESP_ERR_INVALID_STATE;
    int64_t began = esp_timer_get_time();
    if (began < 0) return ESP_ERR_INVALID_STATE;
    uint32_t deadline = (uint32_t)((uint64_t)began / 1000U) + 20U;
    if (take_link_mutex(pdMS_TO_TICKS(20)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t error = ESP_OK;
    if (command == CERBERUS_PACKET_COUNT_COMMAND_INFO || command == CERBERUS_PACKET_COUNT_COMMAND_BEGIN) {
        cerberus_c5_status_t device = {0};
        error = get_status_v2_passive_locked(++sequence, deadline, &device);
        peer_firmware_version = error == ESP_OK ? device.firmware_version : 0U;
    }
    if (error == ESP_OK && peer_firmware_version < CERBERUS_PACKET_COUNT_MIN_FIRMWARE)
        error = ESP_ERR_NOT_SUPPORTED;
    if (error == ESP_OK && raw) {
        cerberus_spi_frame_t frame;
        uint8_t token = ++sequence;
        uint8_t tx[CERBERUS_PACKET_COUNT_RAW_TRANSFER_BYTES] = {0};
        uint8_t discarded[CERBERUS_PACKET_COUNT_RAW_TRANSFER_BYTES] = {0};
        cerberus_packet_count_raw_frame_t response = {0};
        if (!cerberus_protocol_prepare_frame(&frame, (cerberus_spi_command_t)command,
            token, query, request_size)) error = ESP_ERR_INVALID_ARG;
        else {
            memcpy(tx, &frame, sizeof(frame));
            error = spi_request_response_passive(tx, discarded, &response, sizeof(tx), deadline);
            if (error == ESP_OK && (!cerberus_protocol_packet_count_raw_valid(&response) ||
                response.sequence != token)) error = ESP_ERR_INVALID_RESPONSE;
            if (error == ESP_OK) memcpy(reply, &response.payload, reply_size);
        }
    } else if (error == ESP_OK) error = survey_session_frame_locked((cerberus_spi_command_t)command,
        expected ? query : NULL, expected, reply, reply_size, deadline);
    give_link_mutex();
    int64_t ended = esp_timer_get_time();
    if (error == ESP_OK && (ended < began || (uint64_t)(ended - began) >= 20000U))
        error = ESP_ERR_TIMEOUT;
    if (error == ESP_OK && expected) {
        uint64_t returned_session;
        memcpy(&returned_session, reply, sizeof(returned_session));
        if (returned_session != session) error = ESP_ERR_INVALID_RESPONSE;
    }
    if (error == ESP_OK && (raw || command == CERBERUS_PACKET_COUNT_COMMAND_READ)) {
        cerberus_packet_count_read_t q;
        memcpy(&q, query, sizeof(q));
        uint32_t pin;
        uint16_t part;
        memcpy(&pin, (uint8_t *)reply + 8U, sizeof(pin));
        memcpy(&part, (uint8_t *)reply + 12U, sizeof(part));
        if (part != q.part || (pin && pin != q.pin)) error = ESP_ERR_INVALID_RESPONSE;
        /* Canonical pin0 remote-error replies remain visible to the caller. */
    }
    if (error != ESP_OK) memset(reply, 0, reply_size);
    return error;
}
