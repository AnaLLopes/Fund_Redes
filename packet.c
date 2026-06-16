#include "packet.h"
#include "crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Retornar o tipo do pacote */
int get_packet_type(const char *buffer) {
    int type = 0;
    sscanf(buffer, "%d", &type);
    return type;
}

/* ============= DISCOVER ============= */

/* Serializar pacote DISCOVER: 10:alias:ip */
int serialize_discover(char *buffer, int max_len, char alias, const char *ip) {
    int len = snprintf(buffer, max_len, "10:%c:%s", alias, ip);
    return (len < 0 || len >= max_len) ? -1 : len;
}

/* Desserializar pacote DISCOVER */
int deserialize_discover(const char *buffer, DiscoverPacket *pkt) {
    if (pkt == NULL) return -1;
    pkt->type = PACKET_DISCOVER;
    
    int n = sscanf(buffer, "%d:%c:%15s", &pkt->type, &pkt->alias, pkt->ip);
    return (n == 3) ? 0 : -1;
}

/* ============= HELLO ============= */

/* Serializar pacote HELLO: 20:alias:ip */
int serialize_hello(char *buffer, int max_len, char alias, const char *ip) {
    int len = snprintf(buffer, max_len, "20:%c:%s", alias, ip);
    return (len < 0 || len >= max_len) ? -1 : len;
}

/* Desserializar pacote HELLO */
int deserialize_hello(const char *buffer, DiscoverPacket *pkt) {
    if (pkt == NULL) return -1;
    pkt->type = PACKET_HELLO;
    
    int n = sscanf(buffer, "%d:%c:%15s", &pkt->type, &pkt->alias, pkt->ip);
    return (n == 3) ? 0 : -1;
}

/* ============= TOKEN ============= */

/* Serializar pacote TOKEN: 1000 */
int serialize_token(char *buffer, int max_len) {
    int len = snprintf(buffer, max_len, "1000");
    return (len < 0 || len >= max_len) ? -1 : len;
}

/* Desserializar pacote TOKEN */
int deserialize_token(const char *buffer) {
    int type = 0;
    int n = sscanf(buffer, "%d", &type);
    return (n == 1 && type == PACKET_TOKEN) ? 0 : -1;
}

/* ============= DATA ============= */

/* Serializar pacote DATA: 2000:origin:dest:status:crc:message */
int serialize_data(char *buffer, int max_len, char origin, char dest,
                   const char *status, uint32_t crc, const char *message) {
    int len = snprintf(buffer, max_len, "2000:%c:%c:%s:%u:%s",
                       origin, dest, status, crc, message);
    return (len < 0 || len >= max_len) ? -1 : len;
}

/* Desserializar pacote DATA */
int deserialize_data(const char *buffer, DataPacket *pkt) {
    if (pkt == NULL) return -1;
    
    pkt->type = PACKET_DATA;
    
    /* Encontrar os 5 primeiros ':' para dividir os campos */
    char temp[2048];
    strncpy(temp, buffer, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    char *p = temp;
    int field = 0;
    char *start = p;
    
    while (*p && field < 5) {
        if (*p == ':') {
            *p = '\0';
            
            switch(field) {
                case 0: /* type */
                    pkt->type = atoi(start);
                    break;
                case 1: /* origin */
                    pkt->origin = start[0];
                    break;
                case 2: /* dest */
                    pkt->dest = start[0];
                    break;
                case 3: /* status */
                    strncpy(pkt->status, start, sizeof(pkt->status) - 1);
                    break;
                case 4: /* crc */
                    pkt->crc = (uint32_t)strtoul(start, NULL, 10);
                    break;
            }
            
            field++;
            start = p + 1;
        }
        p++;
    }
    
    /* Último campo: mensagem */
    if (field == 5) {
        strncpy(pkt->message, start, sizeof(pkt->message) - 1);
        pkt->message[sizeof(pkt->message) - 1] = '\0';
        return 0;
    }
    
    return -1;
}
