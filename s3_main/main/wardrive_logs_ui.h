/* Private, single Wardriving branch. All SD/network work runs on the bounded
 * manager worker; the touch loop reads value snapshots only. */
#include "wardrive_log_manager.h"
#include "wardrive_upload_settings.h"
typedef enum { WD_LOG_LIST,WD_LOG_DETAIL,WD_LOG_SETUP,WD_LOG_INPUT,
    WD_LOG_CONFIRM_UPLOAD,WD_LOG_CONFIRM_DELETE,WD_LOG_CONFIRM_FORGET,WD_LOG_CONFIRM_IMPORT } wd_log_page_t;
typedef struct {
    cerberus_upload_config_t draft;
    cerberus_keyboard_t keyboard;
    char input[129],message[48];
    wd_log_page_t page,return_page;
    unsigned list_page,edit;
    int selected,confirmed_index;
    uint64_t confirmed_session;
    uint32_t confirmed_revision;
    unsigned confirmed_count;
    bool closing,auto_pending;
} wd_log_ui_t;
static wd_log_ui_t *wd_log_ui;
static bool wd_log_after_stop;
static void wardrive_logs_arm_after_stop(void){wd_log_after_stop=true;}

static bool wardrive_logs_is_open(void){return wd_log_ui&&current_page==UI_PAGE_WARDRIVE;}
static void wardrive_logs_open(void)
{
    wd_log_after_stop=false;
    cerberus_wardrive_status_t status;cerberus_wardrive_get_status(&status);
    if(wardrive_start_pending||status.state==CERBERUS_WARDRIVE_RUNNING||
        status.state==CERBERUS_WARDRIVE_STARTING||status.state==CERBERUS_WARDRIVE_STOPPING)return;
    wardrive_gps_view_close();wardrive_recent_close();
    if(!wd_log_ui)wd_log_ui=heap_caps_calloc(1,sizeof(*wd_log_ui),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!wd_log_ui){wardrive_start_error=ESP_ERR_NO_MEM;return;}
    wd_log_ui->selected=-1;
    esp_err_t e=cerberus_log_manager_open();
    if(e!=ESP_OK)snprintf(wd_log_ui->message,sizeof(wd_log_ui->message),"%s",esp_err_to_name(e));
    wardrive_last_draw_ms=0;
}
static void wardrive_logs_tick(bool asleep)
{
    if(asleep||current_page!=UI_PAGE_WARDRIVE||wardrive_start_pending)wd_log_after_stop=false;
    if(wd_log_after_stop&&!wd_log_ui){
        cerberus_wardrive_status_t capture;cerberus_wardrive_get_status(&capture);
        if(capture.state==CERBERUS_WARDRIVE_STOPPED){
            wd_log_after_stop=false;cerberus_upload_config_t config={0};
            esp_err_t e=cerberus_upload_settings_load(&config);
            bool automatic=e==ESP_OK&&config.auto_upload_after_stop;
            cerberus_upload_config_clear(&config,sizeof(config));
            if(automatic){wardrive_logs_open();if(wd_log_ui)wd_log_ui->auto_pending=true;}
        }else if(capture.state==CERBERUS_WARDRIVE_ERROR||capture.state==CERBERUS_WARDRIVE_IDLE)wd_log_after_stop=false;
    }
    if(!wd_log_ui)return;
    if(asleep||current_page!=UI_PAGE_WARDRIVE)wd_log_ui->closing=true;
    if(wd_log_ui->auto_pending&&!wd_log_ui->closing){
        cerberus_log_manager_status_t s;cerberus_log_manager_status(&s);
        if(!s.busy){
            wd_log_ui->auto_pending=false;
            if(s.active&&s.error==ESP_OK&&s.count){
                esp_err_t e=cerberus_log_manager_request(s.session,s.catalog_revision,CERBERUS_LOG_AUTO_UPLOAD,-1);
                if(e!=ESP_OK)snprintf(wd_log_ui->message,sizeof(wd_log_ui->message),"%s",esp_err_to_name(e));
            }
        }
    }
    if(!wd_log_ui->closing)return;
    cerberus_log_manager_close();
    cerberus_upload_config_clear(&wd_log_ui->draft,sizeof(wd_log_ui->draft));
    cerberus_upload_config_clear(wd_log_ui->input,sizeof(wd_log_ui->input));
    cerberus_log_manager_status_t s;cerberus_log_manager_status(&s);
    if(!s.active){cerberus_upload_config_clear(wd_log_ui,sizeof(*wd_log_ui));heap_caps_free(wd_log_ui);wd_log_ui=NULL;wardrive_last_draw_ms=0;}
}
static char *wardrive_logs_field(unsigned field,size_t *capacity)
{
    switch(field){
    case 1:*capacity=sizeof(wd_log_ui->draft.ssid);return wd_log_ui->draft.ssid;
    case 2:*capacity=sizeof(wd_log_ui->draft.password);return wd_log_ui->draft.password;
    case 3:*capacity=sizeof(wd_log_ui->draft.wigle_name);return wd_log_ui->draft.wigle_name;
    case 4:*capacity=sizeof(wd_log_ui->draft.wigle_token);return wd_log_ui->draft.wigle_token;
    case 5:*capacity=sizeof(wd_log_ui->draft.wdgwars_token);return wd_log_ui->draft.wdgwars_token;
    case 6:*capacity=sizeof(wd_log_ui->draft.wardrift_token);return wd_log_ui->draft.wardrift_token;
    default:*capacity=0;return NULL;}
}
static void wardrive_logs_confirm(wd_log_page_t page,int index,const cerberus_log_manager_status_t *s)
{
    wd_log_ui->return_page=wd_log_ui->page;wd_log_ui->page=page;
    wd_log_ui->confirmed_session=s->session;wd_log_ui->confirmed_revision=s->catalog_revision;
    wd_log_ui->confirmed_index=index;wd_log_ui->confirmed_count=index<0?s->count:1;
}
static esp_err_t wardrive_logs_draw(void)
{
    cerberus_log_manager_status_t s;cerberus_log_manager_status(&s);
    ESP_RETURN_ON_ERROR(cerberus_display_fill_rect(0,0,320,480,UI_BACKGROUND),TAG,"log clear");
    ESP_RETURN_ON_ERROR(draw_header("Logs & Uploads"),TAG,"log title");
    if(wd_log_ui->page==WD_LOG_INPUT){
        ESP_RETURN_ON_ERROR(cerberus_keyboard_draw(&wd_log_ui->keyboard,
            cerberus_upload_field_name((cerberus_upload_field_t)wd_log_ui->edit)),TAG,"upload keyboard");
        return draw_small_button(8,432,304,36,"CANCEL FIELD");
    }
    char line[64];
    const char *message=wd_log_ui->message[0]?wd_log_ui->message:s.stage;
    if(wd_log_ui->closing)message="CLOSING / RELEASING RESOURCES";
    ESP_RETURN_ON_ERROR(draw_centered(58,message,wd_log_ui->message[0]||s.error!=ESP_OK?UI_BAD:UI_MUTED,1),TAG,"log status");
    if(s.error!=ESP_OK){snprintf(line,sizeof(line),"%s",esp_err_to_name(s.error));
        ESP_RETURN_ON_ERROR(draw_centered(74,line,UI_BAD,1),TAG,"log error");}
    if(s.busy||wd_log_ui->closing){
        ESP_RETURN_ON_ERROR(draw_centered(106,s.name[0]?s.name:"OPENING SAVED LOGS",UI_TEXT,2),TAG,"log active file");
        snprintf(line,sizeof(line),"FILES %u / %u",s.processed,s.total);
        ESP_RETURN_ON_ERROR(draw_centered(145,line,UI_TEXT,1),TAG,"log files progress");
        snprintf(line,sizeof(line),"%lu / %lu KiB",(unsigned long)(s.sent/1024),(unsigned long)(s.bytes/1024));
        ESP_RETURN_ON_ERROR(draw_centered(170,line,UI_TEXT,1),TAG,"log bytes");
        for(unsigned i=0;i<3;++i){snprintf(line,sizeof(line),"%s: %s",cerberus_upload_service_name((cerberus_upload_service_t)i),cerberus_upload_result_name(s.destination[i].result));
            ESP_RETURN_ON_ERROR(draw_centered(220+i*28,line,UI_TEXT,1),TAG,"upload result");}
        ESP_RETURN_ON_ERROR(draw_centered(332,"CANCEL MAY REQUIRE ACCOUNT RECONCILIATION",UI_MUTED,1),TAG,"upload cancel note");
        ESP_RETURN_ON_ERROR(draw_small_button(8,380,304,36,"CANCEL OPERATION"),TAG,"log cancel");
        return draw_small_button(8,432,304,36,"BACK / CANCEL AND CLOSE");
    }
    if(wd_log_ui->page==WD_LOG_SETUP){
        ESP_RETURN_ON_ERROR(draw_centered(90,"HOME AP: 2.4 GHz / UPLOAD AFTER CAPTURE",UI_GOOD,1),TAG,"upload home role");
        for(unsigned i=1;i<=6;++i){size_t cap;const char *value=wardrive_logs_field(i,&cap);(void)cap;
            snprintf(line,sizeof(line),"%s: %s",cerberus_upload_field_name((cerberus_upload_field_t)i),value&&value[0]?"SET":"EMPTY");
            ESP_RETURN_ON_ERROR(draw_small_button(8,108+(i-1)*29,304,25,line),TAG,"upload field");}
        for(unsigned i=0;i<3;++i){snprintf(line,sizeof(line),"%s %s",i==0?"WIGLE":i==1?"WDGW":"DRIFT",(wd_log_ui->draft.destinations&(1U<<i))?"ON":"OFF");
            ESP_RETURN_ON_ERROR(draw_small_button(8+i*104,288,96,26,line),TAG,"upload destination");}
        ESP_RETURN_ON_ERROR(draw_small_button(8,322,304,28,wd_log_ui->draft.auto_upload_after_stop?
            "AUTO AFTER STOP: ON":"AUTO AFTER STOP: OFF"),TAG,"automatic setup");
        ESP_RETURN_ON_ERROR(draw_small_button(8,358,148,28,"IMPORT SD JSON"),TAG,"import setup");
        ESP_RETURN_ON_ERROR(draw_small_button(164,358,148,28,"SAVE SETUP"),TAG,"save setup");
        ESP_RETURN_ON_ERROR(draw_small_button(8,394,304,28,"FORGET HOME AP AND ALL KEYS"),TAG,"forget setup");
        return draw_small_button(8,432,304,36,"CANCEL SETUP / LOGS");
    }
    if(wd_log_ui->page==WD_LOG_CONFIRM_IMPORT){
        ESP_RETURN_ON_ERROR(draw_centered(120,"IMPORT UPLOAD PROFILE?",UI_TEXT,1),TAG,"profile confirm");
        ESP_RETURN_ON_ERROR(draw_centered(155,CERBERUS_WARD_UPLOAD_PROFILE_PATH,UI_TEXT,1),TAG,"profile path");
        ESP_RETURN_ON_ERROR(draw_centered(205,"REPLACES HOME AP AND ALL SERVICE KEYS",UI_BAD,1),TAG,"profile replacement");
        ESP_RETURN_ON_ERROR(draw_centered(245,"SD COPY REMOVED AFTER VERIFIED SAVE",UI_MUTED,1),TAG,"profile consume");
        ESP_RETURN_ON_ERROR(draw_centered(285,"SAVING DOES NOT CONNECT OR UPLOAD",UI_MUTED,1),TAG,"profile no network");
        ESP_RETURN_ON_ERROR(draw_small_button(8,380,304,36,"CONFIRM IMPORT"),TAG,"import confirm button");
        return draw_small_button(8,432,304,36,"CANCEL");
    }
    if(wd_log_ui->page==WD_LOG_CONFIRM_UPLOAD||wd_log_ui->page==WD_LOG_CONFIRM_DELETE||wd_log_ui->page==WD_LOG_CONFIRM_FORGET){
        bool upload=wd_log_ui->page==WD_LOG_CONFIRM_UPLOAD,forget=wd_log_ui->page==WD_LOG_CONFIRM_FORGET;
        ESP_RETURN_ON_ERROR(draw_centered(120,forget?"FORGET SAVED CREDENTIALS?":upload?"UPLOAD SELECTED CAPTURES?":"DELETE SELECTED CAPTURES?",UI_TEXT,1),TAG,"log confirm title");
        snprintf(line,sizeof(line),"%u FINALIZED LOG%s IN /CERBERUS/WARD",wd_log_ui->confirmed_count,wd_log_ui->confirmed_count==1?"":"S");
        ESP_RETURN_ON_ERROR(draw_centered(155,forget?"HOME WI-FI + ALL THREE SERVICE KEYS":line,UI_TEXT,1),TAG,"log confirm scope");
        ESP_RETURN_ON_ERROR(draw_centered(195,upload?"INCLUDES OBSERVED IDENTIFIERS AND GPS":forget?"LOGS AND RECEIPTS STAY ON SD":"THIS CANNOT BE UNDONE",UI_BAD,1),TAG,"log confirm effect");
        if(upload){
            cerberus_upload_config_t config={0};esp_err_t e=cerberus_upload_settings_load(&config);
            for(unsigned i=0;i<3;++i){snprintf(line,sizeof(line),"%s: %s",cerberus_upload_service_name((cerberus_upload_service_t)i),
                e!=ESP_OK?"SETUP ERROR":(config.destinations&(1U<<i))?"SELECTED":"OFF");
                ESP_RETURN_ON_ERROR(draw_centered(234+i*22,line,UI_TEXT,1),TAG,"confirm destination");}
            cerberus_upload_config_clear(&config,sizeof(config));
            ESP_RETURN_ON_ERROR(draw_centered(318,"UNLOCATED LOGS STAY LOCAL",UI_MUTED,1),TAG,"local log note");
        }
        ESP_RETURN_ON_ERROR(draw_small_button(8,380,304,36,upload?"CONFIRM UPLOAD":forget?"CONFIRM FORGET":"CONFIRM DELETE"),TAG,"log confirm");
        return draw_small_button(8,432,304,36,"CANCEL");
    }
    if(wd_log_ui->page==WD_LOG_DETAIL){
        ESP_RETURN_ON_ERROR(draw_centered(100,s.name,UI_TEXT,2),TAG,"selected log");
        snprintf(line,sizeof(line),"%lu ROWS / %llu KiB",(unsigned long)s.summary.rows,(unsigned long long)(s.summary.bytes/1024));
        ESP_RETURN_ON_ERROR(draw_centered(135,line,UI_TEXT,1),TAG,"log rows");
        if(s.summary.kind==CERBERUS_WARD_LOG_UNLOCATED)snprintf(line,sizeof(line),"RADIO SUBTOTALS NOT AVAILABLE");
        else snprintf(line,sizeof(line),"2.4G %lu / 5G %lu / BLE %lu",(unsigned long)s.summary.wifi_24,(unsigned long)s.summary.wifi_5,(unsigned long)s.summary.ble);
        ESP_RETURN_ON_ERROR(draw_centered(157,line,UI_TEXT,1),TAG,"log radios");
        ESP_RETURN_ON_ERROR(draw_centered(180,s.summary.first_utc[0]?s.summary.first_utc:"UTC NOT AVAILABLE",UI_MUTED,1),TAG,"log date");
        if(s.summary.kind==CERBERUS_WARD_LOG_UNLOCATED)snprintf(line,sizeof(line),"UNLOCATED / LOCAL EVIDENCE ONLY");
        else snprintf(line,sizeof(line),"NO ACCURACY %lu / NO ALTITUDE %lu",(unsigned long)s.summary.missing_accuracy,(unsigned long)s.summary.missing_altitude);
        ESP_RETURN_ON_ERROR(draw_centered(202,line,UI_MUTED,1),TAG,"log quality");
        for(unsigned i=0;i<3;++i){snprintf(line,sizeof(line),"%s: %s",cerberus_upload_service_name((cerberus_upload_service_t)i),cerberus_upload_result_name(s.destination[i].result));
            ESP_RETURN_ON_ERROR(draw_centered(234+i*23,line,UI_TEXT,1),TAG,"log receipt");}
        if(s.transfer.attempted){
            snprintf(line,sizeof(line),"HTTP %u / %s",s.transfer.http_status,
                s.transfer.response_complete?"REPLY RECEIVED":"NO COMPLETE REPLY");
            ESP_RETURN_ON_ERROR(draw_centered(298,line,UI_MUTED,1),TAG,"upload transfer result");}
        if(s.summary.error)ESP_RETURN_ON_ERROR(draw_centered(312,cerberus_ward_log_error_name(s.summary.error),UI_BAD,1),TAG,"log validation error");
        else if(s.destination[CERBERUS_UPLOAD_WDGWARS].counts_valid){
            snprintf(line,sizeof(line),"WDGW IMPORTED %lu / REJECTED ROWS %lu",(unsigned long)s.destination[1].imported,(unsigned long)s.destination[1].rejected_rows);
            ESP_RETURN_ON_ERROR(draw_centered(312,line,UI_TEXT,1),TAG,"server row counts");}
        bool eligible=s.summary.complete&&!s.summary.error&&!s.summary.legacy_metadata&&s.summary.kind==CERBERUS_WARD_LOG_WIGLE;
        ESP_RETURN_ON_ERROR(draw_small_button(8,346,304,32,eligible?"UPLOAD THIS LOG":"LOCAL ONLY / EXPORT USING SD"),TAG,"upload one");
        ESP_RETURN_ON_ERROR(draw_small_button(8,388,304,30,"DELETE THIS LOG"),TAG,"delete one");
        return draw_small_button(8,432,304,36,"BACK TO LOGS");
    }
    unsigned pages=s.count?(s.count+4)/5:1;if(wd_log_ui->list_page>=pages)wd_log_ui->list_page=pages-1;
    ESP_RETURN_ON_ERROR(draw_small_button(8,90,304,30,"HOME WI-FI / API SETUP"),TAG,"setup route");
    for(unsigned row=0;row<5;++row){cerberus_ward_log_entry_t entry;
        unsigned index=wd_log_ui->list_page*5+row;if(!cerberus_log_manager_row(index,&entry))break;
        snprintf(line,sizeof(line),"%s  %llu KiB",entry.name,(unsigned long long)(entry.bytes/1024));
        ESP_RETURN_ON_ERROR(draw_small_button(8,130+row*34,304,30,line),TAG,"saved log row");}
    if(!s.count)ESP_RETURN_ON_ERROR(draw_centered(190,"NO FINALIZED CAPTURES ON THIS CARD",UI_MUTED,1),TAG,"empty logs");
    snprintf(line,sizeof(line),"%u / %u",wd_log_ui->list_page+1,pages);
    ESP_RETURN_ON_ERROR(draw_small_button(8,310,84,28,"PREV"),TAG,"previous logs");
    ESP_RETURN_ON_ERROR(draw_centered(320,line,UI_MUTED,1),TAG,"log pagination");
    ESP_RETURN_ON_ERROR(draw_small_button(228,310,84,28,"NEXT"),TAG,"next logs");
    ESP_RETURN_ON_ERROR(draw_small_button(8,350,304,30,"UPLOAD ALL LOCATED LOGS"),TAG,"upload all");
    ESP_RETURN_ON_ERROR(draw_small_button(8,388,304,30,"DELETE ALL FINALIZED LOGS"),TAG,"delete all");
    return draw_small_button(8,432,304,36,"BACK TO WARDRIVING");
}
static void wardrive_logs_tap(uint16_t x,uint16_t y)
{
    if(!wd_log_ui||x<8||x>=312)return;
    /* A touch while the catalog opens cancels an undispatched automatic pass.
     * Otherwise Cancel could be cleared by the later request admission. */
    wd_log_ui->auto_pending=false;
    cerberus_log_manager_status_t s;cerberus_log_manager_status(&s);
    wd_log_ui->message[0]=0;
    if(wd_log_ui->closing)return;
    if(s.busy){if(y>=432&&y<468)wd_log_ui->closing=true;
        if(y>=380)cerberus_log_manager_cancel();
        wardrive_last_draw_ms=0;return;}
    if(y>=432&&y<468){
        if(wd_log_ui->page==WD_LOG_INPUT){cerberus_upload_config_clear(wd_log_ui->input,sizeof(wd_log_ui->input));wd_log_ui->page=WD_LOG_SETUP;}
        else if(wd_log_ui->page>=WD_LOG_CONFIRM_UPLOAD)wd_log_ui->page=wd_log_ui->return_page;
        else if(wd_log_ui->page==WD_LOG_LIST)wd_log_ui->closing=true;
        else{cerberus_upload_config_clear(&wd_log_ui->draft,sizeof(wd_log_ui->draft));wd_log_ui->page=WD_LOG_LIST;}
        wardrive_last_draw_ms=0;return;
    }
    esp_err_t e=ESP_OK;
    if(wd_log_ui->page==WD_LOG_INPUT){
        cerberus_keyboard_action_t action;e=cerberus_keyboard_handle_tap(&wd_log_ui->keyboard,x,y,&action);
        if(e==ESP_OK&&action==CERBERUS_KEYBOARD_ACTION_SUBMIT){
            size_t cap;char *field=wardrive_logs_field(wd_log_ui->edit,&cap);
            cerberus_upload_config_t candidate=wd_log_ui->draft;
            if(field&&strlen(wd_log_ui->input)<cap){size_t offset=(size_t)(field-(char *)&wd_log_ui->draft);
                memset((char *)&candidate+offset,0,cap);memcpy((char *)&candidate+offset,wd_log_ui->input,strlen(wd_log_ui->input));
                cerberus_upload_field_t bad=cerberus_upload_config_invalid(&candidate);
                if(bad==CERBERUS_UPLOAD_FIELD_NONE){wd_log_ui->draft=candidate;wd_log_ui->page=WD_LOG_SETUP;
                    cerberus_upload_config_clear(wd_log_ui->input,sizeof(wd_log_ui->input));}
                else{snprintf(wd_log_ui->message,sizeof(wd_log_ui->message),"INVALID %s",cerberus_upload_field_name(bad));wd_log_ui->page=WD_LOG_SETUP;}
            }cerberus_upload_config_clear(&candidate,sizeof(candidate));
            cerberus_upload_config_clear(wd_log_ui->input,sizeof(wd_log_ui->input));
            memset(&wd_log_ui->keyboard,0,sizeof(wd_log_ui->keyboard));
        }
    }else if(wd_log_ui->page==WD_LOG_SETUP){
        if(y>=108&&y<282){unsigned field=(y-108)/29+1;size_t cap;char *value=wardrive_logs_field(field,&cap);
            if(value){wd_log_ui->edit=field;e=cerberus_keyboard_begin(&wd_log_ui->keyboard,wd_log_ui->input,cap,value);
                cerberus_keyboard_set_masked(&wd_log_ui->keyboard,field!=1);wd_log_ui->page=WD_LOG_INPUT;}}
        else if(y>=288&&y<314){unsigned i=(x-8)/104;if(i<3)wd_log_ui->draft.destinations^=(uint8_t)(1U<<i);}
        else if(y>=322&&y<350)wd_log_ui->draft.auto_upload_after_stop^=1U;
        else if(y>=358&&y<386&&x<160)wardrive_logs_confirm(WD_LOG_CONFIRM_IMPORT,-1,&s);
        else if(y>=358&&y<386){e=cerberus_upload_settings_save(&wd_log_ui->draft);
            if(e==ESP_OK){cerberus_upload_config_clear(&wd_log_ui->draft,sizeof(wd_log_ui->draft));wd_log_ui->page=WD_LOG_LIST;
                snprintf(wd_log_ui->message,sizeof(wd_log_ui->message),"UPLOAD SETUP SAVED");}}
        else if(y>=394&&y<422)wardrive_logs_confirm(WD_LOG_CONFIRM_FORGET,-1,&s);
    }else if(wd_log_ui->page>=WD_LOG_CONFIRM_UPLOAD){
        if(y>=380&&y<416){
            if(wd_log_ui->page==WD_LOG_CONFIRM_IMPORT){
                e=cerberus_log_manager_request(wd_log_ui->confirmed_session,wd_log_ui->confirmed_revision,CERBERUS_LOG_IMPORT_PROFILE,-1);
                cerberus_upload_config_clear(&wd_log_ui->draft,sizeof(wd_log_ui->draft));wd_log_ui->page=WD_LOG_LIST;
            }else if(wd_log_ui->page==WD_LOG_CONFIRM_FORGET){e=cerberus_upload_settings_forget();
                cerberus_upload_config_clear(&wd_log_ui->draft,sizeof(wd_log_ui->draft));wd_log_ui->page=WD_LOG_SETUP;}
            else{bool upload=wd_log_ui->page==WD_LOG_CONFIRM_UPLOAD;
                e=cerberus_log_manager_request(wd_log_ui->confirmed_session,wd_log_ui->confirmed_revision,
                    upload?CERBERUS_LOG_UPLOAD:CERBERUS_LOG_DELETE,wd_log_ui->confirmed_index);
                wd_log_ui->page=upload&&wd_log_ui->confirmed_index>=0?WD_LOG_DETAIL:WD_LOG_LIST;}
        }
    }else if(wd_log_ui->page==WD_LOG_DETAIL){
        if(y>=346&&y<378&&s.summary.complete&&!s.summary.error&&!s.summary.legacy_metadata&&
            s.summary.kind==CERBERUS_WARD_LOG_WIGLE)wardrive_logs_confirm(WD_LOG_CONFIRM_UPLOAD,wd_log_ui->selected,&s);
        else if(y>=388&&y<418)wardrive_logs_confirm(WD_LOG_CONFIRM_DELETE,wd_log_ui->selected,&s);
    }else{
        if(y>=90&&y<120){e=cerberus_upload_settings_load(&wd_log_ui->draft);wd_log_ui->page=WD_LOG_SETUP;}
        else if(y>=130&&y<300){unsigned index=wd_log_ui->list_page*5+(y-130)/34;
            if(index<s.count){wd_log_ui->selected=(int)index;wd_log_ui->page=WD_LOG_DETAIL;
                e=cerberus_log_manager_request(s.session,s.catalog_revision,CERBERUS_LOG_INSPECT,(int)index);}}
        else if(y>=310&&y<338){if(x<92&&wd_log_ui->list_page)--wd_log_ui->list_page;
            else if(x>=228&&(wd_log_ui->list_page+1)*5<s.count)++wd_log_ui->list_page;}
        else if(y>=350&&y<380&&s.count)wardrive_logs_confirm(WD_LOG_CONFIRM_UPLOAD,-1,&s);
        else if(y>=388&&y<418&&s.count)wardrive_logs_confirm(WD_LOG_CONFIRM_DELETE,-1,&s);
    }
    if(e!=ESP_OK)snprintf(wd_log_ui->message,sizeof(wd_log_ui->message),"%s",esp_err_to_name(e));
    wardrive_last_draw_ms=0;
}
