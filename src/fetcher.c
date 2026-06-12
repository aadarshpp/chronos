#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <curl/curl.h>
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

        FILE *f = fopen(DATA_FILE, "wb");
        if (!f) {
            perror("Failed to open data file");
            cJSON_Delete(root);
            return 1;
        }

        int count = cJSON_GetArraySize(ts_arr);

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

            fwrite(&c, sizeof(CandleStick), 1, f);
        }

        fclose(f);

        printf("Success. Data written to %s\n", DATA_FILE);

        // Cleanup
        cJSON_Delete(root);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return 0;
}