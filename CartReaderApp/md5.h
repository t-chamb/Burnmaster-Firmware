#ifndef MD5_H
#define MD5_H

#include <stdint.h>

// MD5 context structure
typedef struct {
    uint32_t state[4];    // state (ABCD)
    uint32_t count[2];    // number of bits, modulo 2^64 (lsb first)
    uint8_t buffer[64];   // input buffer
} MD5_CTX;

// Function prototypes
void MD5_Init(MD5_CTX *context);
void MD5_Update(MD5_CTX *context, const uint8_t *input, uint32_t inputLen);
void MD5_Final(uint8_t digest[16], MD5_CTX *context);

// Helper function to calculate MD5 of a file
uint8_t calculateMD5_file(const char* filename, const char* folder, uint8_t digest[16]);

#endif // MD5_H