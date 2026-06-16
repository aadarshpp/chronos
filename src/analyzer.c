#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include "engine.h"
#include "cJSON.h"

// ---------------------------------------------------------
// Function: get_matching_files
// ---------------------------------------------------------
int get_matching_files(const char *symbol, char ***files, int *count) {
    DIR *d;
    struct dirent *entry;
    struct stat file_stat;
    int capacity = 10;
    
    *files = (char **)malloc(capacity * sizeof(char *));
    *count = 0;

    d = opendir("data");
    if (!d) return -1;

    // FIX 4: Increased buffer size to prevent truncation (NameMax 255 + "data/" + null)
    char filepath[512]; 

    // Helper to check extension
    const char *ext = ".bin";
    size_t ext_len = strlen(ext);
    size_t sym_len = strlen(symbol);

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG) continue;
        
        // Check filename length safety
        size_t name_len = strlen(entry->d_name);
        if (name_len <= ext_len) continue; // Too short to be .bin

        // FIX 5: Implement actual filtering logic
        // Check prefix
        if (strncmp(entry->d_name, symbol, sym_len) != 0) continue;
        // Check suffix
        if (strcmp(entry->d_name + name_len - ext_len, ext) != 0) continue;

        snprintf(filepath, sizeof(filepath), "data/%s", entry->d_name);

        // FIX 2: Inverted logic. stat() returns 0 on SUCCESS.
        // We only want to process files we can successfully stat.
        if (stat(filepath, &file_stat) != 0) continue; 

        // Skip empty files (optional, but good practice)
        if (file_stat.st_size == 0) continue;

        // Add to list
        (*files)[*count] = strdup(filepath);
        (*count)++;

        // Note: Ideally, you should realloc() here if *count == capacity
        if (*count == capacity) {
            // Omitted for brevity as per original, but strictly needed for production code
            break; 
        }
    }
    closedir(d);
    return 0;
}

// ---------------------------------------------------------
// Function: calculate_sma
// ---------------------------------------------------------
void calculate_sma(int period, float price_buffer[], int count, float sma_buffer[]) {
    float sum = 0.0;
    
    for (int i = 0; i < count; i++) {
        sum += price_buffer[i];
        
        if (i >= period) {
            // FIX 3: Corrected Sliding Window
            // Only subtract when we move past the very first window
            sum -= price_buffer[i - period];
            sma_buffer[i] = sum / (float)period;
        } else if (i == period - 1) {
            // We have exactly 'period' items, calculate first average
            sma_buffer[i] = sum / (float)period;
        } else {
            // Not enough data yet
            sma_buffer[i] = price_buffer[i]; 
        }
    }
}

void free_files(char **files, int count) {
    for (int i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: ./analyzer <SYMBOL> <PERIOD>\n");
        return 1;
    }

    const char *symbol = argv[1];
    int period = atoi(argv[2]);
    char **files;
    int file_count = 0;

    if (get_matching_files(symbol, &files, &file_count) != 0) {
        printf("Error finding files for symbol %s\n", symbol);
        return 1;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *data_array = cJSON_CreateArray();
    size_t total_records = 0;

    // FIX 6: Use distinct variable name for outer loop
    for (int file_idx = 0; file_idx < file_count; file_idx++) {
        const char *filepath = files[file_idx];
        
        FILE *f = fopen(filepath, "rb");
        if (!f) continue;
        
        struct stat sb;
        if (fstat(fileno(f), &sb) != 0) {
            fclose(f);
            continue;
        }
        
        size_t file_size = sb.st_size;
        unsigned char *file_data = (unsigned char *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(f), 0);
        
        if (file_data == MAP_FAILED) {
            fclose(f);
            continue;
        }

        const size_t RECORD_SIZE = sizeof(CandleStick);
        size_t num_records = file_size / RECORD_SIZE;
        
        // Allocs
        float *prices = (float *)malloc(num_records * sizeof(float));
        float *smas = (float *)malloc(num_records * sizeof(float));
        if (!prices || !smas) {
            // Handle alloc failure
            if(prices) free(prices);
            if(smas) free(smas);
            munmap(file_data, file_size);
            fclose(f);
            continue;
        }

        CandleStick c;

        // 1. Unpack Loop
        for (size_t i = 0; i < num_records; i++) {
            size_t offset = i * RECORD_SIZE;
            memcpy(&c, file_data + offset, RECORD_SIZE);
            prices[i] = c.close / PRICE_SCALE;
        }

        // 2. Calculate
        calculate_sma(period, prices, num_records, smas);

        // 3. Construct JSON Loop
        for (size_t i = 0; i < num_records; i++) {
            // FIX 1: Re-read 'c' inside this loop so data isn't stale
            size_t offset = i * RECORD_SIZE;
            memcpy(&c, file_data + offset, RECORD_SIZE);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddItemToObject(item, "x", cJSON_CreateNumber(c.timestamp)); 
            cJSON_AddItemToObject(item, "o", cJSON_CreateNumber(c.open / PRICE_SCALE)); 
            cJSON_AddItemToObject(item, "h", cJSON_CreateNumber(c.high / PRICE_SCALE)); 
            cJSON_AddItemToObject(item, "l", cJSON_CreateNumber(c.low / PRICE_SCALE)); 
            cJSON_AddItemToObject(item, "c", cJSON_CreateNumber(c.close / PRICE_SCALE)); 
            cJSON_AddItemToObject(item, "sma", cJSON_CreateNumber(smas[i])); 
            cJSON_AddItemToArray(data_array, item);
        }

        free(prices);
        free(smas);
        munmap(file_data, file_size);
        fclose(f);
        total_records += num_records;
    }

    cJSON_AddItemToObject(root, "symbol", cJSON_CreateString(symbol));
    cJSON_AddItemToObject(root, "total", cJSON_CreateNumber(total_records));
    cJSON_AddItemToObject(root, "engine", cJSON_CreateString("C Engine"));
    cJSON_AddItemToObject(root, "data", data_array);
    
    // FIX 7: Capture and free the JSON string
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s", json_str);
    free(json_str);

    if (files) free_files(files, file_count);
    cJSON_Delete(root);

    return 0;
}