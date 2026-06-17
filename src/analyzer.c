#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>
#include "cJSON.h"
#include "engine.h"

// On-disk size is 32 bytes (struct without the in-memory sma field)
#define DISK_RECORD_SIZE 32

int compare_filenames(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

char **discover_partitions(const char *symbol, int *count) {
    DIR *d = opendir("data");
    if (!d) return NULL;

    struct dirent *entry;
    char **files = NULL;
    int n = 0;
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s_", symbol);

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0 &&
                strstr(entry->d_name, ".bin")) {
                files = realloc(files, (n + 1) * sizeof(char *));
                files[n] = malloc(strlen("data/") + strlen(entry->d_name) + 1);
                sprintf(files[n], "data/%s", entry->d_name);
                n++;
            }
        }
    }
    closedir(d);

    // Alphanumeric sort == chronological for SYMBOL_YYYY_MM.bin
    qsort(files, n, sizeof(char *), compare_filenames);
    *count = n;
    return files;
}

void free_partitions(char **files, int count) {
    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <symbol> <period>\n", argv[0]);
        return 1;
    }

    const char *symbol = argv[1];
    int period = atoi(argv[2]);
    if (period <= 0) period = 20;

    int file_count = 0;
    char **files = discover_partitions(symbol, &file_count);
    if (!files || file_count == 0) {
        printf("[]\n");
        return 0;
    }

    // Count total records
    size_t total_records = 0;
    for (int i = 0; i < file_count; i++) {
        struct stat sb;
        if (stat(files[i], &sb) == 0) total_records += sb.st_size / DISK_RECORD_SIZE;
    }

    if (total_records == 0) {
        free_partitions(files, file_count);
        printf("[]\n");
        return 0;
    }

    float *closes = malloc(total_records * sizeof(float));
    CandleStick *records = malloc(total_records * sizeof(CandleStick));

    // Read all partitions via mmap
    size_t idx = 0;
    for (int i = 0; i < file_count; i++) {
        FILE *f = fopen(files[i], "rb");
        if (!f) continue;

        struct stat sb;
        fstat(fileno(f), &sb);
        size_t file_size = sb.st_size;
        if (file_size == 0) { fclose(f); continue; }

        unsigned char *data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(f), 0);
        if (data == MAP_FAILED) { fclose(f); continue; }

        size_t num_records = file_size / DISK_RECORD_SIZE;
        for (size_t j = 0; j < num_records; j++) {
            unsigned char *p = data + (j * DISK_RECORD_SIZE);
            records[idx].timestamp = *(uint64_t *)(p);
            records[idx].open      = *(int32_t  *)(p + 8);
            records[idx].high      = *(int32_t  *)(p + 12);
            records[idx].low       = *(int32_t  *)(p + 16);
            records[idx].close     = *(int32_t  *)(p + 20);
            records[idx].volume    = *(uint32_t *)(p + 24);
            records[idx].crc       = *(uint32_t *)(p + 28);
            records[idx].sma       = 0.0f;

            closes[idx] = records[idx].close / (float)PRICE_SCALE;
            idx++;
        }
        munmap(data, file_size);
        fclose(f);
    }
    free_partitions(files, file_count);

    // Rolling SMA calculation
    float window_sum = 0.0f;
    for (size_t i = 0; i < idx; i++) {
        window_sum += closes[i];
        if (i >= (size_t)period) window_sum -= closes[i - period];
        if (i >= (size_t)(period - 1)) {
            records[i].sma = window_sum / period;
        } else {
            records[i].sma = closes[i]; // Warmup: track close until window fills
        }
    }

    // Serialize to JSON array (pure array, no metadata wrapper)
    cJSON *root = cJSON_CreateArray();
    for (size_t i = 0; i < idx; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddItemToObject(item, "x",   cJSON_CreateNumber((double)(records[i].timestamp * 1000)));
        cJSON_AddItemToObject(item, "o",   cJSON_CreateNumber(records[i].open  / (double)PRICE_SCALE));
        cJSON_AddItemToObject(item, "h",   cJSON_CreateNumber(records[i].high  / (double)PRICE_SCALE));
        cJSON_AddItemToObject(item, "l",   cJSON_CreateNumber(records[i].low   / (double)PRICE_SCALE));
        cJSON_AddItemToObject(item, "c",   cJSON_CreateNumber(records[i].close / (double)PRICE_SCALE));
        cJSON_AddItemToObject(item, "sma", cJSON_CreateNumber((double)records[i].sma));
        cJSON_AddItemToArray(root, item);
    }

    char *out = cJSON_PrintUnformatted(root);
    printf("%s\n", out);

    free(out);
    cJSON_Delete(root);
    free(closes);
    free(records);
    return 0;
}