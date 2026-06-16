#ifndef PACKET_H
#define PACKET_H

#include "common.h"
#include <stdint.h>

/* Estrutura para pacote DISCOVER/HELLO */
typedef struct {
    int type;     /* 10 = DISCOVER, 20 = HELLO */
    char alias;
    char ip[16];
} DiscoverPacket;

/* Estrutura para pacote TOKEN */
typedef struct {
    int type;     /* 1000 */
} TokenPacket;

/* Estrutura para pacote DATA */
typedef struct {
    int type;     /* 2000 */
    char origin;
    char dest;
    char status[20];      /* maquinainexistente, NAK, ACK */
    uint32_t crc;
    char message[MAX_MSG_LENGTH];
} DataPacket;

/* Funções de serialização */
int serialize_discover(char *buffer, int max_len, char alias, const char *ip);
int deserialize_discover(const char *buffer, DiscoverPacket *pkt);

int serialize_hello(char *buffer, int max_len, char alias, const char *ip);
int deserialize_hello(const char *buffer, DiscoverPacket *pkt);

int serialize_token(char *buffer, int max_len);
int deserialize_token(const char *buffer);

int serialize_data(char *buffer, int max_len, char origin, char dest, 
                   const char *status, uint32_t crc, const char *message);
int deserialize_data(const char *buffer, DataPacket *pkt);

/* Funções utilitárias */
int get_packet_type(const char *buffer);

#endif
