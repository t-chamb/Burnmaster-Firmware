#ifndef DATABASE_H
#define DATABASE_H

#include "Common.h"

// Function prototypes
uint32_t calculateCRC32(const char* filename, const char* folder, uint32_t offset);
boolean compareCRC_GB(const char* database, uint32_t crc32sum, boolean renamerom);
boolean searchGBADatabase(const char* database, const char* gameID, uint8_t* romSize, uint8_t* saveType);

// MD5 database functions
boolean compareMD5_GB(const char* database, const uint8_t* md5sum, boolean renamerom);

#endif // DATABASE_H