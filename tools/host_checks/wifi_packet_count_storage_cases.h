/* Executes the production optional two-stream storage lease in this existing
 * real-file fault harness. No SD card, radio or timing claim is made. */
static void packet_count_storage_cases(void)
{
    packet_count_testing = true;
    uint8_t header[68]={0}, packet[80]={0};
    char directory[8];
    cerberus_storage_packet_count_result_t result;
    setup();
    CHECK(cerberus_storage_packet_count_begin(0,directory)==ESP_ERR_INVALID_ARG);
    count_fail_allocation=true;
    CHECK(cerberus_storage_packet_count_begin(1,directory)==ESP_ERR_NO_MEM);
    count_fail_allocation=false;
    CHECK(!sd_packet_count&&!count_allocations);
    CHECK(cerberus_storage_packet_count_begin(1,directory)==ESP_OK);
    CHECK(!strcmp(directory,"P00001")&&count_allocations==1);
    CHECK(cerberus_storage_packet_count_begin(2,directory)==ESP_ERR_INVALID_STATE);
    CHECK(cerberus_storage_unmount()==ESP_ERR_INVALID_STATE);
    CHECK(cerberus_storage_reserve_for_usb()==ESP_ERR_INVALID_STATE);
    CHECK(begin()==ESP_ERR_INVALID_STATE);
    CHECK(cerberus_storage_wardrive_session_begin()==ESP_ERR_INVALID_STATE);
    CHECK(wardrive_stream_begin(&sd_wardrive_stream,"UNLOC","h",1)==ESP_ERR_INVALID_STATE);
    CHECK(cerberus_storage_wifi_capture_begin(header,sizeof(header))==ESP_ERR_INVALID_STATE);
    CHECK(!ble_archive_storage_available());
    CHECK(recon_path_protected("sdcard/cerberus/PCOUNT./P00001 /S3.TMP"));
    CHECK(recon_path_protected("sdcard/CERBERUS"));
    CHECK(!recon_path_protected("sdcard/CERBERUS/PCOUNT/P00002"));
    CHECK(cerberus_storage_delete_entry("/CERBERUS/PCOUNT","P00001")==ESP_ERR_INVALID_STATE);
    CHECK(cerberus_storage_packet_count_open(2,0,header,sizeof(header))==ESP_ERR_INVALID_STATE);
    CHECK(cerberus_storage_packet_count_finish(2,&result)==ESP_ERR_INVALID_STATE&&sd_packet_count);
    for(unsigned i=0;i<2;++i){
        CHECK(cerberus_storage_packet_count_open(1,i,header,sizeof(header))==ESP_OK);
        unsigned writes=calls[WRITE];
        CHECK(cerberus_storage_packet_count_open(1,i,header,sizeof(header))==ESP_OK);
        CHECK(writes==calls[WRITE]);
        CHECK(cerberus_storage_packet_count_append(1,i,packet,sizeof(packet))==ESP_OK);
    }
    CHECK(cerberus_storage_packet_count_finish(1,&result)==ESP_OK);
    CHECK(!sd_packet_count&&!count_allocations&&!stream_count&&!descriptor_count&&!locked);
    CHECK(result.bytes[0]==148&&result.bytes[1]==148);
    CHECK(!strcmp(result.file_name[0],"S3.PCAPNG")&&!strcmp(result.file_name[1],"C5.PCAPNG"));
    CHECK(cerberus_storage_packet_count_begin(2,directory)==ESP_OK&&!strcmp(directory,"P00002"));
    CHECK(cerberus_storage_packet_count_finish(2,&result)==ESP_OK&&!result.file_name[0][0]);
    CHECK(locks==releases);
    const enum event failures[]={OPEN,FDOPEN,WRITE,FLUSH,SYNC,CLOSE,RENAME,STAT};
    for(unsigned f=0;f<sizeof(failures)/sizeof(failures[0]);++f){
        setup();CHECK(cerberus_storage_packet_count_begin(9,directory)==ESP_OK);
        if(failures[f]==OPEN || failures[f]==FDOPEN || failures[f]==WRITE){
            fail_next(failures[f]);
            CHECK(cerberus_storage_packet_count_open(9,0,header,sizeof(header))!=ESP_OK);
        }else{
            CHECK(cerberus_storage_packet_count_open(9,0,header,sizeof(header))==ESP_OK);
            fail_next(failures[f]);
        }
        CHECK(cerberus_storage_packet_count_finish(9,&result)!=ESP_OK);
        CHECK(!sd_packet_count&&!count_allocations&&!stream_count&&!descriptor_count&&!locked);
        CHECK(strcmp(result.file_name[0],"S3.PCAPNG")!=0);
        CHECK(locks==releases);
    }
    setup();CHECK(cerberus_storage_packet_count_begin(12,directory)==ESP_OK);
    CHECK(cerberus_storage_packet_count_open(12,1,header,sizeof(header))==ESP_OK);
    sd_packet_count->source[1].bytes=CERBERUS_STORAGE_WIFI_CAPTURE_MAX_BYTES-1;
    CHECK(cerberus_storage_packet_count_append(12,1,packet,2)==ESP_ERR_INVALID_SIZE);
    CHECK(cerberus_storage_packet_count_finish(12,&result)==ESP_ERR_INVALID_SIZE);
    CHECK(!strcmp(result.file_name[1],"C5.TMP")&&!sd_packet_count&&!count_allocations);
}
