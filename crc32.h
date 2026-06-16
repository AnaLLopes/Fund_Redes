#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

/* Calcula CRC32 de um buffer */
uint32_t calculate_crc32(const unsigned char *data, size_t length);

/* Verifica se o CRC32 está correto */
int verify_crc32(const unsigned char *data, size_t data_length, uint32_t expected_crc);

#endif
