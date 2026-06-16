#ifndef NETWORK_OPS_H
#define NETWORK_OPS_H

#include "common.h"
#include "queue.h"
#include "packet.h"

/* Estrutura expandida da rede */
typedef struct {
    Config config;
    Machine machines[MAX_MACHINES];
    int num_machines;
    int my_index;
    int next_index;
    int socket_discover;
    int socket_data;
    char my_ip[16];
    int has_token;
    int run;
    int token_received_time;      /* Para detectar token perdido */
    int token_last_time;          /* Último timestamp quando token passou */
    int token_count;              /* Número de tokens na rede (debug) */
    int total_tokens_generated;
    
    MessageQueue *msg_queue;
    
    pthread_mutex_t net_lock;
    pthread_mutex_t token_lock;
    pthread_cond_t token_cond;
    
} NetManager;

/* Funções de gerenciamento de rede */
NetManager* network_init(const char *config_file);
void network_cleanup(NetManager *nm);

/* Descoberta de máquinas */
int send_discover(NetManager *nm);
int process_discover(NetManager *nm, const char *buffer, struct sockaddr_in *addr);
int send_hello(NetManager *nm, char alias, const char *ip);
int process_hello(NetManager *nm, const char *buffer);

/* Ordenar máquinas em anel */
void build_ring(NetManager *nm);

/* Gerenciar token */
int send_token(NetManager *nm, int to_index);
int receive_token(NetManager *nm);
int token_timeout_occurred(NetManager *nm);
int duplicate_token_detected(NetManager *nm);

/* Enviar/receber dados */
int send_data(NetManager *nm, char dest, const char *message);
int process_data(NetManager *nm, DataPacket *pkt);

/* Carregar config */
int load_config(const char *filename, Config *config);

#endif
