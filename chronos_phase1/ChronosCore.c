#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ChronosCore.h"

// Implement the exact function signature from the .h file
JNIEXPORT void JNICALL Java_ChronosClient_insertRaw(JNIEnv *env, jobject obj, jlong timestamp, jdouble price) {
    
    // 1. Open file in "append binary" mode. 
    // "ab" means: create if doesn't exist, append if it does, write RAW BYTES (no text formatting)
    FILE *file = fopen("data.chronos", "ab");
    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }

    // 2. Write the timestamp (8 bytes)
    // &timestamp = memory address of the variable
    // sizeof(int64_t) = 8 bytes
    // 1 = write 1 element
    size_t written_ts = fwrite(&timestamp, sizeof(int64_t), 1, file);

    // 3. Write the price (8 bytes)
    size_t written_pr = fwrite(&price, sizeof(double), 1, file);

    // 4. Close the file
    fclose(file);
}