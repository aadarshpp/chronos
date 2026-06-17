#include <stdio.h>
#include <stdint.h>

// The ZigZag encoder for 32-bit integers
uint32_t zigzag_encode(int32_t n) {
    return (uint32_t)((n << 1) ^ (n >> 31));
}

int main() {
    // Test cases mapping to our known truth table
    int32_t test_values[] = {0, -1, 1, -2, 2, -150, 150};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    printf("--- ZigZag Encoding Test ---\n");
    for (int i = 0; i < num_tests; i++) {
        uint32_t encoded = zigzag_encode(test_values[i]);
        printf("Input: %6d  |  ZigZag Encoded: %6u\n", test_values[i], encoded);
    }

    return 0;
}