#include "crc32.h"
#include <string.h>

/* Tabela pré-calculada para CRC32 */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

/* Inicializar tabela CRC32 */
static void init_crc32_table() {
    if (crc32_table_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = 1;
}

/* Calcula CRC32 de um buffer */
uint32_t calculate_crc32(const unsigned char *data, size_t length) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFFUL;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFFUL;
}

/* Verifica se o CRC32 está correto */
int verify_crc32(const unsigned char *data, size_t data_length, uint32_t expected_crc) {
    uint32_t calculated = calculate_crc32(data, data_length);
    return calculated == expected_crc;
}
