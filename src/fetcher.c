#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>
#include <time.h>
#include "cJSON.h"
#include "engine.h"

// --- GLOBAL BUFFER FOR HTTP RESPONSE ---
// We allocate 5MB. Enough for a full day of minute-data.
char response_buffer[5000000] = {0}; 
size_t buffer_index = 0;

// --- CALLBACK FUNCTION ---
// libcurl calls this function when data arrives.
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;

    // Safety check to prevent buffer overflow
    if (buffer_index + total_size >= sizeof(response_buffer)) {
        printf("Error: Response too large for buffer.\n");
        return 0;
    }

    // Append data to our global buffer
    memcpy(&response_buffer[buffer_index], contents, total_size);
    buffer_index += total_size;

    return total_size;
}

char* get_partition_filename(uint64_t timestamp) {
    static char filename[256];

    time_t raw_time = timestamp;
    struct tm *time_info = gmtime(&raw_time);

    snprintf(
        filename,
        sizeof(filename),
        "%sAAPL_%04d_%02d.bin",
        DATA_DIR,
        time_info->tm_year + 1900,
        time_info->tm_mon + 1
    );

    return filename;
}

void update_catalog(char *filename, uint64_t start, uint64_t end) {

    cJSON *new_entry = cJSON_CreateObject();

    cJSON_AddStringToObject(
        new_entry,
        "file",
        filename
    );

    cJSON_AddNumberToObject(
        new_entry,
        "start",
        start
    );

    cJSON_AddNumberToObject(
        new_entry,
        "end",
        end
    );

    cJSON *catalog_array = NULL;

    FILE *catalog_file = fopen(CATALOG_FILE, "r");

    if (catalog_file) {

        fseek(catalog_file, 0, SEEK_END);
        long size = ftell(catalog_file);
        rewind(catalog_file);

        char *buffer = malloc(size + 1);

        fread(buffer, 1, size, catalog_file);
        buffer[size] = '\0';

        fclose(catalog_file);

        catalog_array = cJSON_Parse(buffer);

        free(buffer);
    }

    if (!catalog_array) {
        catalog_array = cJSON_CreateArray();
        printf("Catalog entries before append: %d\n",
            cJSON_GetArraySize(catalog_array));
    }

    // cJSON_AddItemToArray(catalog_array, new_entry);

    int updated = 0;

    for (int i = 0; i < cJSON_GetArraySize(catalog_array); i++) {

        cJSON *entry = cJSON_GetArrayItem(catalog_array, i);

        cJSON *file_item = cJSON_GetObjectItem(entry, "file");

        if (file_item && strcmp(file_item->valuestring, filename) == 0) {

            cJSON_ReplaceItemInObject(
                entry,
                "start",
                cJSON_CreateNumber(start)
            );

            cJSON_ReplaceItemInObject(
                entry,
                "end",
                cJSON_CreateNumber(end)
            );

            updated = 1;
            break;
        }
    }

    if (!updated) {
        cJSON_AddItemToArray(catalog_array, new_entry);
    }

    printf("Catalog entries after append: %d\n",
        cJSON_GetArraySize(catalog_array));

    char *json_string = cJSON_Print(catalog_array);

    FILE *catalog = fopen(CATALOG_FILE, "w");

    if (!catalog) {
        perror("Failed to open catalog");
        cJSON_free(json_string);
        cJSON_Delete(catalog_array);
        return;
    }

    fprintf(catalog, "%s", json_string);

    fclose(catalog);

    cJSON_free(json_string);
    cJSON_Delete(catalog_array);
}

// --- MAIN ENGINE ---
int main() {
    CURL *curl;
    CURLcode res;

    // 1. Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        // 2. Set the URL (AAPL, 1 minute interval, 1 day range)
        // You can change "AAPL" to "TSLA", "BTC-USD", etc.
        const char *url = "https://query1.finance.yahoo.com/v8/finance/chart/AAPL?interval=1m&range=1d";
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // 3. Setup the callback to capture the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        // 4. Add User-Agent (Yahoo Finance sometimes blocks requests without it)
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        printf("Fetching data from: %s ...\n", url);

        // 5. Perform the Request (Blocks here until done)
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 1;
        }

        // Null-terminate the string so cJSON can read it
        response_buffer[buffer_index] = '\0';

        // --- PHASE 2: PARSING JSON ---
        printf("Parsing JSON...\n");

        cJSON *root = cJSON_Parse(response_buffer);
        if (!root) {
            printf("JSON Parse Error before: [%s]\n", cJSON_GetErrorPtr());
            curl_easy_cleanup(curl);
            return 1;
        }

        // root -> chart
        cJSON *chart = cJSON_GetObjectItem(root, "chart");
        if (!chart) {
            printf("Error: Missing 'chart'\n");
            cJSON_Delete(root);
            return 1;
        }

        // root -> chart -> result
        cJSON *result = cJSON_GetObjectItem(chart, "result");

        if (!result || !cJSON_IsArray(result)) {
            printf("Error: Yahoo returned no result array\n");

            cJSON *error = cJSON_GetObjectItem(chart, "error");
            if (error) {
                char *err = cJSON_Print(error);
                printf("Yahoo Error:\n%s\n", err);
                free(err);
            }

            cJSON_Delete(root);
            return 1;
        }

        // result[0]
        cJSON *result_0 = cJSON_GetArrayItem(result, 0);
        if (!result_0) {
            printf("Error: result[0] missing\n");
            cJSON_Delete(root);
            return 1;
        }

        // timestamp is an array of UNIX timestamps (in seconds)
        cJSON *ts_arr = cJSON_GetObjectItem(result_0, "timestamp");

        cJSON *indicators = cJSON_GetObjectItem(result_0, "indicators");
        cJSON *quote = cJSON_GetObjectItem(indicators, "quote");
        cJSON *quote_0 = cJSON_GetArrayItem(quote, 0);

        if (!quote_0) {
            printf("Error: quote[0] missing\n");
            cJSON_Delete(root);
            return 1;
        }

        // OHLCV arrays
        cJSON *open_arr = cJSON_GetObjectItem(quote_0, "open");
        cJSON *high_arr = cJSON_GetObjectItem(quote_0, "high");
        cJSON *low_arr = cJSON_GetObjectItem(quote_0, "low");
        cJSON *close_arr = cJSON_GetObjectItem(quote_0, "close");
        cJSON *vol_arr = cJSON_GetObjectItem(quote_0, "volume");

        // Validation
        if (!ts_arr || !open_arr || !high_arr || !low_arr || !close_arr || !vol_arr) {
            printf("Error: Missing one or more data arrays\n");
            cJSON_Delete(root);
            return 1;
        }

        printf("timestamp records: %d\n", cJSON_GetArraySize(ts_arr));
        printf("open records: %d\n", cJSON_GetArraySize(open_arr));
        printf("close records: %d\n", cJSON_GetArraySize(close_arr));

        // --- PHASE 3: WRITING BINARY ---
        printf("Writing to binary file...\n");

        // FILE *f = fopen(DATA_FILE, "wb");
        // if (!f) {
        //     perror("Failed to open data file");
        //     cJSON_Delete(root);
        //     return 1;
        // }

        int count = cJSON_GetArraySize(ts_arr);
        uint64_t partition_start = 0;
        uint64_t partition_end = 0;
        char current_partition[256] = "";
        for (int i = 0; i < count; i++) {

            cJSON *ts_item = cJSON_GetArrayItem(ts_arr, i);
            cJSON *open_item = cJSON_GetArrayItem(open_arr, i);
            cJSON *high_item = cJSON_GetArrayItem(high_arr, i);
            cJSON *low_item = cJSON_GetArrayItem(low_arr, i);
            cJSON *close_item = cJSON_GetArrayItem(close_arr, i);
            cJSON *vol_item = cJSON_GetArrayItem(vol_arr, i);

            // Skip incomplete candles
            if (!ts_item || !open_item || !high_item ||
                !low_item || !close_item || !vol_item ||
                cJSON_IsNull(open_item) ||
                cJSON_IsNull(high_item) ||
                cJSON_IsNull(low_item) ||
                cJSON_IsNull(close_item) ||
                cJSON_IsNull(vol_item)) {
                continue;
            }

            CandleStick c;

            c.timestamp = (uint64_t)ts_item->valuedouble;
            c.open = (int32_t)(open_item->valuedouble * PRICE_SCALE);
            c.high = (int32_t)(high_item->valuedouble * PRICE_SCALE);
            c.low = (int32_t)(low_item->valuedouble * PRICE_SCALE);
            c.close = (int32_t)(close_item->valuedouble * PRICE_SCALE);
            c.volume = (uint32_t)vol_item->valuedouble;

            c.crc = (uint32_t)(
                c.timestamp ^
                c.open ^
                c.high ^
                c.low ^
                c.close ^
                c.volume
            );

            // fwrite(&c, sizeof(CandleStick), 1, f);
            char *filename = get_partition_filename(c.timestamp);
            if (partition_start == 0) {
                partition_start = c.timestamp;
            }

            partition_end = c.timestamp;

            strcpy(current_partition, filename);
            FILE *f = fopen(filename, "ab");

            if (!f) {
                perror("Failed to open partition file");
                continue;
            }

            fwrite(&c, sizeof(CandleStick), 1, f);

            fclose(f);
        }

        // printf("Success. Data written to %s\n", DATA_FILE);
        update_catalog(
            current_partition,
            partition_start,
            partition_end
        );

        printf("Success. Data written to partitions\n");

        // Cleanup
        cJSON_Delete(root);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return 0;
}