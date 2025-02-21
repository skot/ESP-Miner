/******************************************************************************
 *  *
 * References:
 *  1. Stratum Protocol - [link](https://reference.cash/mining/stratum-protocol)
 *****************************************************************************/

#include "stratum_api.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "lwip/sockets.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 1024
static const char * TAG = "stratum_api";

static void debug_stratum_tx(const char * POOL_TAG, const char *);


StratumApiV1Buffer *STRATUM_V1_buffer_create() {
    StratumApiV1Buffer *buf = malloc(sizeof(StratumApiV1Buffer));
    if (!buf) {
        fprintf(stderr, "Error: Failed to allocate memory for StratumApiV1Buffer\n");
        exit(1);
    }

    STRATUM_V1_buffer_init(buf);

    return buf;
}

void STRATUM_V1_buffer_init(StratumApiV1Buffer *buf) {
    buf->data = malloc(BUFFER_SIZE);
    buf->size = BUFFER_SIZE;

    if (!buf->data) {
        fprintf(stderr, "Error: Failed to allocate memory for buffer\n");
        exit(1);
    }
    memset(buf->data, 0, buf->size);
}

void STRATUM_V1_buffer_clear(StratumApiV1Buffer *buf) {
    memset(buf->data, 0, BUFFER_SIZE);
    buf->size = 0;
}

void STRATUM_V1_buffer_realloc(StratumApiV1Buffer *buf, size_t additional_len) {
    size_t old_size = strlen(buf->data);
    size_t new_size = old_size + additional_len + 1;

    if (new_size <= buf->size) {
        return;
    }

    new_size += BUFFER_SIZE - (new_size % BUFFER_SIZE);
    char *new_data = realloc(buf->data, new_size);

    if (!new_data) {
        fprintf(stderr, "Error: realloc failed in recalloc_sock()\n");
        ESP_LOGI(TAG, "Restarting System because of ERROR: realloc failed in recalloc_sock");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    buf->data = new_data;
    memset(buf->data + old_size, 0, new_size - old_size);
    buf->size = new_size;
}

char *STRATUM_V1_receive_jsonrpc_line(const char * POOL_TAG, int sockfd, StratumApiV1Buffer *json_buf)
{
    char *line, *tok = NULL;
    char recv_buffer[BUFFER_SIZE];
    int nbytes;
    size_t buflen = 0;

    if (!strstr(json_buf->data, "\n")) {
        do {
            memset(recv_buffer, 0, BUFFER_SIZE);
            nbytes = recv(sockfd, recv_buffer, BUFFER_SIZE - 1, 0);
            if (nbytes <= 0)
            {
                ESP_LOGI(POOL_TAG, "Error: recv (errno %d: %s)", errno, strerror(errno));
                STRATUM_V1_buffer_clear(json_buf);
                return NULL;
            }

            STRATUM_V1_buffer_realloc(json_buf, nbytes);
            strncat(json_buf->data, recv_buffer, nbytes);
        } while (!strstr(json_buf->data, "\n"));
    }
    buflen = strlen(json_buf->data);
    tok = strtok(json_buf->data, "\n");
    line = strdup(tok);
    int len = strlen(line);
    if (buflen > len + 1)
        memmove(json_buf->data, json_buf->data + len + 1, buflen - len + 1);
    else
        strcpy(json_buf->data, "");
    return line;
}

void STRATUM_V1_parse(const char * POOL_TAG, StratumApiV1Message * message, const char * stratum_json)
{
    cJSON * json = cJSON_Parse(stratum_json);

    cJSON * id_json = cJSON_GetObjectItem(json, "id");
    int64_t parsed_id = -1;
    if (id_json != NULL && cJSON_IsNumber(id_json)) {
        parsed_id = id_json->valueint;
    }
    message->message_id = parsed_id;

    cJSON * method_json = cJSON_GetObjectItem(json, "method");
    stratum_method result = STRATUM_UNKNOWN;

    //if there is a method, then use that to decide what to do
    if (method_json != NULL && cJSON_IsString(method_json)) {
        if (strcmp("mining.notify", method_json->valuestring) == 0) {
            result = MINING_NOTIFY;
        } else if (strcmp("mining.set_difficulty", method_json->valuestring) == 0) {
            result = MINING_SET_DIFFICULTY;
        } else if (strcmp("mining.set_version_mask", method_json->valuestring) == 0) {
            result = MINING_SET_VERSION_MASK;
        } else if (strcmp("client.reconnect", method_json->valuestring) == 0) {
            result = CLIENT_RECONNECT;
        } else {
            ESP_LOGI(POOL_TAG, "unhandled method in stratum message: %s", stratum_json);
        }

    //if there is no method, then it is a result
    } else {
        // parse results
        cJSON * result_json = cJSON_GetObjectItem(json, "result");
        cJSON * error_json = cJSON_GetObjectItem(json, "error");
        cJSON * reject_reason_json = cJSON_GetObjectItem(json, "reject-reason");

        // if the result is null, then it's a fail
        if (result_json == NULL) {
            message->response_success = false;
            message->error_str = strdup("unknown");
            
        // if it's an error, then it's a fail
        } else if (!cJSON_IsNull(error_json)) {
            message->response_success = false;
            message->error_str = strdup("unknown");
            if (parsed_id < 5) {
                result = STRATUM_RESULT_SETUP;
            } else {
                result = STRATUM_RESULT;
            }
            if (cJSON_IsArray(error_json)) {
                int len = cJSON_GetArraySize(error_json);
                if (len >= 2) {
                    cJSON * error_msg = cJSON_GetArrayItem(error_json, 1);
                    if (cJSON_IsString(error_msg)) {
                        message->error_str = strdup(cJSON_GetStringValue(error_msg));
                    }
                }
            }

        // if the result is a boolean, then parse it
        } else if (cJSON_IsBool(result_json)) {
            if (parsed_id < 5) {
                result = STRATUM_RESULT_SETUP;
            } else {
                result = STRATUM_RESULT;
            }
            if (cJSON_IsTrue(result_json)) {
                message->response_success = true;
            } else {
                message->response_success = false;
                message->error_str = strdup("unknown");
                if (cJSON_IsString(reject_reason_json)) {
                    message->error_str = strdup(cJSON_GetStringValue(reject_reason_json));
                }                
            }
        
        //if the id is STRATUM_ID_SUBSCRIBE parse it
        } else if (parsed_id == STRATUM_ID_SUBSCRIBE) {
            result = STRATUM_RESULT_SUBSCRIBE;

            cJSON * extranonce2_len_json = cJSON_GetArrayItem(result_json, 2);
            if (extranonce2_len_json == NULL) {
                ESP_LOGE(POOL_TAG, "Unable to parse extranonce2_len: %s", result_json->valuestring);
                message->response_success = false;
                goto done;
            }
            message->extranonce_2_len = extranonce2_len_json->valueint;

            cJSON * extranonce_json = cJSON_GetArrayItem(result_json, 1);
            if (extranonce_json == NULL) {
                ESP_LOGE(POOL_TAG, "Unable parse extranonce: %s", result_json->valuestring);
                message->response_success = false;
                goto done;
            }
            message->extranonce_str = malloc(strlen(extranonce_json->valuestring) + 1);
            strcpy(message->extranonce_str, extranonce_json->valuestring);
            message->response_success = true;

            //print the extranonce_str
            ESP_LOGI(POOL_TAG, "extranonce_str: %s", message->extranonce_str);
            ESP_LOGI(POOL_TAG, "extranonce_2_len: %d", message->extranonce_2_len);

        //if the id is STRATUM_ID_CONFIGURE parse it
        } else if (parsed_id == STRATUM_ID_CONFIGURE) {
            cJSON * mask = cJSON_GetObjectItem(result_json, "version-rolling.mask");
            if (mask != NULL) {
                result = STRATUM_RESULT_VERSION_MASK;
                message->version_mask = strtoul(mask->valuestring, NULL, 16);
                ESP_LOGI(POOL_TAG, "Set version mask: %08lx", message->version_mask);
            } else {
                ESP_LOGI(POOL_TAG, "error setting version mask: %s", stratum_json);
            }

        } else {
            ESP_LOGI(POOL_TAG, "unhandled result in stratum message: %s", stratum_json);
        }
    }

    message->method = result;

    if (message->method == MINING_NOTIFY) {

        mining_notify * new_work = malloc(sizeof(mining_notify));
        // new_work->difficulty = difficulty;
        cJSON * params = cJSON_GetObjectItem(json, "params");
        new_work->job_id = strdup(cJSON_GetArrayItem(params, 0)->valuestring);
        new_work->prev_block_hash = strdup(cJSON_GetArrayItem(params, 1)->valuestring);
        new_work->coinbase_1 = strdup(cJSON_GetArrayItem(params, 2)->valuestring);
        new_work->coinbase_2 = strdup(cJSON_GetArrayItem(params, 3)->valuestring);

        cJSON * merkle_branch = cJSON_GetArrayItem(params, 4);
        new_work->n_merkle_branches = cJSON_GetArraySize(merkle_branch);
        if (new_work->n_merkle_branches > MAX_MERKLE_BRANCHES) {
            printf("Too many Merkle branches.\n");
            abort();
        }
        new_work->merkle_branches = malloc(HASH_SIZE * new_work->n_merkle_branches);
        for (size_t i = 0; i < new_work->n_merkle_branches; i++) {
            hex2bin(cJSON_GetArrayItem(merkle_branch, i)->valuestring, new_work->merkle_branches + HASH_SIZE * i, HASH_SIZE);
        }

        new_work->version = strtoul(cJSON_GetArrayItem(params, 5)->valuestring, NULL, 16);
        new_work->target = strtoul(cJSON_GetArrayItem(params, 6)->valuestring, NULL, 16);
        new_work->ntime = strtoul(cJSON_GetArrayItem(params, 7)->valuestring, NULL, 16);

        message->mining_notification = new_work;

        // params can be varible length
        int paramsLength = cJSON_GetArraySize(params);
        int value = cJSON_IsTrue(cJSON_GetArrayItem(params, paramsLength - 1));
        message->should_abandon_work = value;
    } else if (message->method == MINING_SET_DIFFICULTY) {
        cJSON * params = cJSON_GetObjectItem(json, "params");
        uint32_t difficulty = cJSON_GetArrayItem(params, 0)->valueint;

        message->new_difficulty = difficulty;
    } else if (message->method == MINING_SET_VERSION_MASK) {

        cJSON * params = cJSON_GetObjectItem(json, "params");
        uint32_t version_mask = strtoul(cJSON_GetArrayItem(params, 0)->valuestring, NULL, 16);
        message->version_mask = version_mask;
    }
    done:
    cJSON_Delete(json);
}

void STRATUM_V1_free_mining_notify(mining_notify * params)
{
    free(params->job_id);
    free(params->prev_block_hash);
    free(params->coinbase_1);
    free(params->coinbase_2);
    free(params->merkle_branches);
    free(params);
}

int STRATUM_V1_subscribe(const char * POOL_TAG, int socket, int send_uid, char * model)
{
    // Subscribe
    char subscribe_msg[BUFFER_SIZE];
    const esp_app_desc_t *app_desc = esp_app_get_description();
    const char *version = app_desc->version;	
    sprintf(subscribe_msg, "{\"id\": %d, \"method\": \"mining.subscribe\", \"params\": [\"bitaxe/%s/%s\"]}\n", send_uid, model, version);
    debug_stratum_tx(POOL_TAG, subscribe_msg);

    return write(socket, subscribe_msg, strlen(subscribe_msg));
}

int STRATUM_V1_suggest_difficulty(const char * POOL_TAG, int socket, int send_uid, uint32_t difficulty)
{
    char difficulty_msg[BUFFER_SIZE];
    sprintf(difficulty_msg, "{\"id\": %d, \"method\": \"mining.suggest_difficulty\", \"params\": [%ld]}\n", send_uid, difficulty);
    debug_stratum_tx(POOL_TAG, difficulty_msg);

    return write(socket, difficulty_msg, strlen(difficulty_msg));
}

int STRATUM_V1_authenticate(const char * POOL_TAG, int socket, int send_uid, const char * username, const char * pass)
{
    char authorize_msg[BUFFER_SIZE];
    sprintf(authorize_msg, "{\"id\": %d, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"%s\"]}\n", send_uid, username,
            pass);
    debug_stratum_tx(POOL_TAG, authorize_msg);

    return write(socket, authorize_msg, strlen(authorize_msg));
}

/// @param socket Socket to write to
/// @param username The client’s user name.
/// @param jobid The job ID for the work being submitted.
/// @param ntime The hex-encoded time value use in the block header.
/// @param extranonce_2 The hex-encoded value of extra nonce 2.
/// @param nonce The hex-encoded nonce value to use in the block header.
int STRATUM_V1_submit_share(int socket, int send_uid, const char * username, const char * jobid,
                            const char * extranonce_2, const uint32_t ntime,
                            const uint32_t nonce, const uint32_t version)
{
    char submit_msg[BUFFER_SIZE];
    sprintf(submit_msg,
            "{\"id\": %d, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%08lx\", \"%08lx\", \"%08lx\"]}\n",
            send_uid, username, jobid, extranonce_2, ntime, nonce, version);
    debug_stratum_tx(TAG, submit_msg);

    return write(socket, submit_msg, strlen(submit_msg));
}

int STRATUM_V1_configure_version_rolling(const char * POOL_TAG, int socket, int send_uid)
{
    char configure_msg[BUFFER_SIZE * 2];
    sprintf(configure_msg,
            "{\"id\": %d, \"method\": \"mining.configure\", \"params\": [[\"version-rolling\"], {\"version-rolling.mask\": "
            "\"ffffffff\"}]}\n",
            send_uid);
    debug_stratum_tx(POOL_TAG, configure_msg);

    return write(socket, configure_msg, strlen(configure_msg));
}

static void debug_stratum_tx(const char * POOL_TAG, const char * msg)
{
    //remove the trailing newline
    char * newline = strchr(msg, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }
    ESP_LOGI(POOL_TAG, "tx: %s", msg);

    //put it back!
    if (newline != NULL) {
        *newline = '\n';
    }
}
