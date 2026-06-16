#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

/* Constantes de porta e tamanho */
#define DISCOVER_PORT 6000
#define DATA_PORT 6001
#define MAX_MACHINES 10
#define MAX_MSG_LENGTH 1024
#define MAX_QUEUE 10
#define BROADCAST_ADDR "255.255.255.255"

/* Tipos de pacotes */
#define PACKET_DISCOVER 10
#define PACKET_HELLO 20
#define PACKET_TOKEN 1000
#define PACKET_DATA 2000

/* Estados de erro */
#define MACHINE_NOT_EXISTS "maquinainexistente"
#define NAK "NAK"
#define ACK "ACK"

/* Estrutura de uma máquina na rede */
typedef struct {
    char alias;              /* A, B, C, etc */
    char ip[16];            /* IP address */
    struct sockaddr_in addr;
    int active;
} Machine;

/* Estrutura de mensagem na fila */
typedef struct {
    char dest_alias;
    char message[MAX_MSG_LENGTH];
    int attempts;
} QueueMessage;

/* Configuração da máquina */
typedef struct {
    char alias;
    int token_time;        /* tempo do token e dados */
    int error_probability; /* probabilidade de erro (0-100) */
    int token_timeout;
    int min_token_time;
} Config;

/* Estrutura global da aplicação */
typedef struct {
    Config config;
    Machine machines[MAX_MACHINES];
    int num_machines;
    int my_index;          /* meu índice na lista */
    int next_index;        /* índice do próximo */
    int socket_discover;
    int socket_data;
    char my_ip[16];
    int has_token;
    int run;
    pthread_mutex_t lock;
} Network;

/* Funções auxiliares */
void print_log(const char *format, ...);
char *get_local_ip();
long get_timestamp_ms();

#endif
