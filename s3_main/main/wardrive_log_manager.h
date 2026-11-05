#pragma once
#include "wardrive_log_storage.h"
#include "wardrive_upload_reply.h"
typedef enum { CERBERUS_LOG_INSPECT, CERBERUS_LOG_DELETE, CERBERUS_LOG_UPLOAD } cerberus_log_action_t;
typedef struct {
    uint64_t session;
    uint32_t catalog_revision;
    unsigned count, processed, total, accepted[3], queued[3], skipped[3];
    uint32_t sent, bytes;
    bool active, busy, closing;
    esp_err_t error;
    char stage[40], name[13];
    cerberus_ward_log_summary_t summary;
    cerberus_upload_reply_t destination[3];
} cerberus_log_manager_status_t;
esp_err_t cerberus_log_manager_open(void);
void cerberus_log_manager_close(void);
void cerberus_log_manager_cancel(void);
void cerberus_log_manager_status(cerberus_log_manager_status_t *out);
bool cerberus_log_manager_row(unsigned index,cerberus_ward_log_entry_t *out);
/* index=-1 means all finalized files from the reviewed catalog. The caller
 * must obtain explicit confirmation before DELETE/UPLOAD and pass the exact
 * session and catalog revision shown by that confirmation. */
esp_err_t cerberus_log_manager_request(uint64_t session,uint32_t revision,
    cerberus_log_action_t action,int index);
