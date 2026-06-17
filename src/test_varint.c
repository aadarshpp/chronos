#include <stdio.h>
#include <stdint.h>

// Writes a Varint into a buffer. 
// Returns the number of bytes written.
int write_varint(uint32_t value, uint8_t *buffer) {
    int bytes_written = 0;
    
    // do-while ensures we write at least 1 byte (even if value is 0)
    do {
        // Grab the lowest 7 bits
        uint8_t byte = value & 0x7F; 
        
        // Shift value right by 7 to prepare for the next byte
        value >>= 7; 
        
        // If there is still data left to encode, set the MSB continue flag
        if (value > 0) {
            byte |= 0x80; 
        }
        
        // Store the byte and increment our counter
        buffer[bytes_written] = byte;
        bytes_written++;
        
    } while (value > 0);
    
    return bytes_written;
}

int main() {
    // We will test the raw numbers, AND the ZigZag encoded numbers from 3A
    uint32_t test_values[] = {0, 1, 2, 127, 128, 299, 300, 16383, 16384};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);
    
    // Buffer large enough to hold the maximum varint size for a 32-bit int (5 bytes)
    uint8_t buffer[5]; 

    printf("--- Varint Encoding Test ---\n");
    for (int i = 0; i < num_tests; i++) {
        int size = write_varint(test_values[i], buffer);
        
        printf("Value: %5u  |  Size: %d byte(s)  |  Hex: ", test_values[i], size);
        for (int j = 0; j < size; j++) {
            // Print in uppercase hex, padded with zero if needed
            printf("%02X ", buffer[j]); 
        }
        printf("\n");
    }

    return 0;
}