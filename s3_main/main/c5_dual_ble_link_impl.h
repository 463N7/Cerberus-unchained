/* Included once by c5_spi_link.c. One bounded transaction, no retry loop and
 * no UI-thread bank transfer. Scoped START is idempotent after a lost reply. */
esp_err_t c5_spi_link_dual_ble_exchange(uint8_t command, const void *input,
    size_t input_size, void *output, size_t output_size)
{
    bool allowed = command >= CERBERUS_SPI_COMMAND_DUAL_BLE_INFO &&
        command <= CERBERUS_SPI_COMMAND_DUAL_BLE_TRACK_STATUS;
    allowed = allowed || command == CERBERUS_SPI_COMMAND_BLE_SCAN_RESULT ||
        command == CERBERUS_SPI_COMMAND_BLE_SCAN_DATA;
    if (!allowed || !output || !output_size || output_size > CERBERUS_SPI_FRAME_PAYLOAD_SIZE ||
        input_size > CERBERUS_SPI_FRAME_PAYLOAD_SIZE || (!input && input_size)) return ESP_ERR_INVALID_ARG;
    memset(output, 0, output_size);
    if (!link_mutex) return ESP_ERR_INVALID_STATE;
    int64_t start = esp_timer_get_time();
    if (start < 0) return ESP_ERR_INVALID_STATE;
    uint32_t deadline = (uint32_t)((uint64_t)start / 1000U) + 20U;
    if (take_link_mutex(pdMS_TO_TICKS(5U)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t error = peer_firmware_version < CERBERUS_DUAL_BLE_MIN_FIRMWARE ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
    if (error == ESP_OK) {
        cerberus_spi_frame_t request, response = {0};
        uint8_t discard[CERBERUS_SPI_TRANSFER_SIZE] = {0};
        uint8_t token = ++sequence;
        if (!cerberus_protocol_prepare_frame(&request, (cerberus_spi_command_t)command,
            token, input, input_size)) error = ESP_ERR_INVALID_ARG;
        else {
            error = spi_request_response_passive(&request, discard, &response, sizeof(request), deadline);
            if (error == ESP_OK && cerberus_protocol_frame_is_valid(&response) &&
                response.command == command && response.sequence == token && response.payload_length == output_size)
                memcpy(output, response.payload, output_size);
            else if (error == ESP_OK) error = ESP_ERR_INVALID_RESPONSE;
        }
    }
    give_link_mutex();
    int64_t ended = esp_timer_get_time();
    if (error == ESP_OK && (ended < start || ended - start > 20000LL)) error = ESP_ERR_TIMEOUT;
    if (error != ESP_OK) memset(output, 0, output_size);
    return error;
}
