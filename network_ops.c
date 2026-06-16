#include "network_ops.h"
#include "packet.h"
#include "crc32.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Comparar aliases para ordenação */
static int compare_machines(const void *a, const void *b) {
    const Machine *m1 = (const Machine *)a;
    const Machine *m2 = (const Machine *)b;
    
    if (m1->active && !m2->active) return -1;
    if (!m1->active && m2->active) return 1;
    if (!m1->active && !m2->active) return 0;
    
    return m1->alias - m2->alias;
}

/* Carregar arquivo de configuração */
int load_config(const char *filename, Config *config) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        perror("Erro ao abrir arquivo de configuração");
        return -1;
    }
    
    int n = fscanf(f, "%c\n%d\n%d\n%d\n%d",
                   &config->alias,
                   &config->token_time,
                   &config->error_probability,
                   &config->token_timeout,
                   &config->min_token_time);
    
    fclose(f);
    
    if (n != 5) {
        fprintf(stderr, "Erro ao ler configuração\n");
        return -1;
    }
    
    print_log("Configuração carregada: alias=%c, token_time=%d, erro=%d%%, timeout=%d, min_time=%d\n",
              config->alias, config->token_time, config->error_probability,
              config->token_timeout, config->min_token_time);
    
    return 0;
}

/* Inicializar rede */
NetManager* network_init(const char *config_file) {
    NetManager *nm = (NetManager*)malloc(sizeof(NetManager));
    if (nm == NULL) return NULL;
    
    memset(nm, 0, sizeof(NetManager));
    
    /* Carregar configuração */
    if (load_config(config_file, &nm->config) < 0) {
        free(nm);
        return NULL;
    }
    
    /* Inicializar locks e condições */
    pthread_mutex_init(&nm->net_lock, NULL);
    pthread_mutex_init(&nm->token_lock, NULL);
    pthread_cond_init(&nm->token_cond, NULL);
    
    /* Obter IP local */
    strcpy(nm->my_ip, get_local_ip());
    
    /* Adicionar a si mesmo à lista de máquinas */
    nm->machines[0].alias = nm->config.alias;
    strcpy(nm->machines[0].ip, nm->my_ip);
    nm->machines[0].active = 1;
    nm->num_machines = 1;
    nm->my_index = 0;
    
    /* Criar sockets UDP */
    nm->socket_discover = socket(AF_INET, SOCK_DGRAM, 0);
    nm->socket_data = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (nm->socket_discover < 0 || nm->socket_data < 0) {
        perror("Erro ao criar sockets");
        free(nm);
        return NULL;
    }
    
    /* Permitir reuso de endereço */
    int reuse = 1;
    setsockopt(nm->socket_discover, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(nm->socket_data, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    /* Permitir broadcast */
    setsockopt(nm->socket_discover, SOL_SOCKET, SO_BROADCAST, &reuse, sizeof(reuse));
    
    /* Bind nos sockets */
    struct sockaddr_in addr_discover, addr_data;
    
    memset(&addr_discover, 0, sizeof(addr_discover));
    addr_discover.sin_family = AF_INET;
    addr_discover.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_discover.sin_port = htons(DISCOVER_PORT);
    
    if (bind(nm->socket_discover, (struct sockaddr *)&addr_discover, 
             sizeof(addr_discover)) < 0) {
        perror("Erro ao fazer bind na porta DISCOVER");
        close(nm->socket_discover);
        close(nm->socket_data);
        free(nm);
        return NULL;
    }
    
    memset(&addr_data, 0, sizeof(addr_data));
    addr_data.sin_family = AF_INET;
    addr_data.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_data.sin_port = htons(DATA_PORT);
    
    if (bind(nm->socket_data, (struct sockaddr *)&addr_data,
             sizeof(addr_data)) < 0) {
        perror("Erro ao fazer bind na porta DATA");
        close(nm->socket_discover);
        close(nm->socket_data);
        free(nm);
        return NULL;
    }
    
    /* Criar fila de mensagens */
    nm->msg_queue = queue_create();
    if (nm->msg_queue == NULL) {
        close(nm->socket_discover);
        close(nm->socket_data);
        free(nm);
        return NULL;
    }
    
    nm->run = 1;
    nm->has_token = 0;
    nm->token_last_time = get_timestamp_ms();
    nm->token_count = 0;
    nm->total_tokens_generated = 0;
    
    print_log("Rede inicializada: %c em %s\n", nm->config.alias, nm->my_ip);
    
    return nm;
}

/* Limpar rede */
void network_cleanup(NetManager *nm) {
    if (nm == NULL) return;
    
    nm->run = 0;
    
    if (nm->socket_discover >= 0) close(nm->socket_discover);
    if (nm->socket_data >= 0) close(nm->socket_data);
    
    if (nm->msg_queue) queue_destroy(nm->msg_queue);
    
    pthread_mutex_destroy(&nm->net_lock);
    pthread_mutex_destroy(&nm->token_lock);
    pthread_cond_destroy(&nm->token_cond);
    
    free(nm);
}

/* Enviar DISCOVER em broadcast */
int send_discover(NetManager *nm) {
    if (nm == NULL) return -1;
    
    char buffer[256];
    serialize_discover(buffer, sizeof(buffer), nm->config.alias, nm->my_ip);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    addr.sin_port = htons(DISCOVER_PORT);
    
    int n = sendto(nm->socket_discover, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&addr, sizeof(addr));
    
    if (n < 0) {
        perror("Erro ao enviar DISCOVER");
        return -1;
    }
    
    print_log("DISCOVER enviado: %s (%s)\n", buffer, nm->my_ip);
    return 0;
}

/* Processar DISCOVER recebido */
int process_discover(NetManager *nm, const char *buffer, struct sockaddr_in *addr) {
    if (nm == NULL) return -1;
    
    DiscoverPacket pkt;
    if (deserialize_discover(buffer, &pkt) < 0) {
        return -1;
    }
    
    print_log("DISCOVER recebido de %c (%s)\n", pkt.alias, pkt.ip);
    
    /* Verificar se já está na lista */
    pthread_mutex_lock(&nm->net_lock);
    
    for (int i = 0; i < nm->num_machines; i++) {
        if (nm->machines[i].alias == pkt.alias) {
            pthread_mutex_unlock(&nm->net_lock);
            return 0; /* Já conhecemos esta máquina */
        }
    }
    
    /* Adicionar nova máquina */
    if (nm->num_machines < MAX_MACHINES) {
        nm->machines[nm->num_machines].alias = pkt.alias;
        strcpy(nm->machines[nm->num_machines].ip, pkt.ip);
        nm->machines[nm->num_machines].active = 1;
        nm->num_machines++;
        
        print_log("Nova máquina adicionada: %c\n", pkt.alias);
        
        /* Reconstruir anel */
        build_ring(nm);
    }
    
    pthread_mutex_unlock(&nm->net_lock);
    
    /* Responder com HELLO */
    send_hello(nm, nm->config.alias, nm->my_ip);
    
    return 0;
}

/* Enviar HELLO */
int send_hello(NetManager *nm, char alias, const char *ip) {
    if (nm == NULL) return -1;
    
    char buffer[256];
    serialize_hello(buffer, sizeof(buffer), alias, ip);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    addr.sin_port = htons(DISCOVER_PORT);
    
    int n = sendto(nm->socket_discover, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&addr, sizeof(addr));
    
    if (n < 0) {
        perror("Erro ao enviar HELLO");
        return -1;
    }
    
    return 0;
}

/* Processar HELLO recebido */
int process_hello(NetManager *nm, const char *buffer) {
    if (nm == NULL) return -1;
    
    DiscoverPacket pkt;
    if (deserialize_hello(buffer, &pkt) < 0) {
        return -1;
    }
    
    print_log("HELLO recebido de %c (%s)\n", pkt.alias, pkt.ip);
    
    pthread_mutex_lock(&nm->net_lock);
    
    /* Atualizar se já existe, adicionar se novo */
    int found = 0;
    for (int i = 0; i < nm->num_machines; i++) {
        if (nm->machines[i].alias == pkt.alias) {
            found = 1;
            break;
        }
    }
    
    if (!found && nm->num_machines < MAX_MACHINES) {
        nm->machines[nm->num_machines].alias = pkt.alias;
        strcpy(nm->machines[nm->num_machines].ip, pkt.ip);
        nm->machines[nm->num_machines].active = 1;
        nm->num_machines++;
        
        print_log("Máquina %c adicionada via HELLO\n", pkt.alias);
        build_ring(nm);
    }
    
    pthread_mutex_unlock(&nm->net_lock);
    
    return 0;
}

/* Construir anel ordenado alfabeticamente */
void build_ring(NetManager *nm) {
    if (nm == NULL) return;
    
    /* Ordenar máquinas por alias */
    qsort(nm->machines, nm->num_machines, sizeof(Machine), compare_machines);
    
    /* Encontrar índice da máquina atual */
    nm->my_index = -1;
    for (int i = 0; i < nm->num_machines; i++) {
        if (nm->machines[i].alias == nm->config.alias) {
            nm->my_index = i;
            break;
        }
    }
    
    if (nm->my_index == -1) return;
    
    /* Próximo índice (circular) */
    nm->next_index = (nm->my_index + 1) % nm->num_machines;
    
    print_log("Anel reconstruído: ");
    for (int i = 0; i < nm->num_machines; i++) {
        printf("%c ", nm->machines[i].alias);
    }
    printf("(próximo: %c)\n", nm->machines[nm->next_index].alias);
}

/* Enviar token para próxima máquina */
int send_token(NetManager *nm, int to_index) {
    if (nm == NULL || to_index < 0 || to_index >= nm->num_machines) return -1;
    
    char buffer[256];
    serialize_token(buffer, sizeof(buffer));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(nm->machines[to_index].ip);
    addr.sin_port = htons(DATA_PORT);
    
    int n = sendto(nm->socket_data, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&addr, sizeof(addr));
    
    if (n < 0) {
        perror("Erro ao enviar TOKEN");
        return -1;
    }
    
    print_log("TOKEN enviado para %c\n", nm->machines[to_index].alias);
    
    pthread_mutex_lock(&nm->token_lock);
    nm->has_token = 0;
    nm->token_last_time = get_timestamp_ms();
    pthread_mutex_unlock(&nm->token_lock);
    
    return 0;
}

/* Verificar se token perdido */
int token_timeout_occurred(NetManager *nm) {
    if (nm == NULL) return 0;
    
    pthread_mutex_lock(&nm->token_lock);
    
    /* Apenas a máquina que criou o token monitora */
    if (nm->config.alias != 'A') {
        pthread_mutex_unlock(&nm->token_lock);
        return 0;
    }
    
    long now = get_timestamp_ms();
    long elapsed = now - nm->token_last_time;
    
    int timeout = (elapsed > nm->config.token_timeout * 1000);
    
    pthread_mutex_unlock(&nm->token_lock);
    
    return timeout;
}

/* Verificar token duplicado */
int duplicate_token_detected(NetManager *nm) {
    if (nm == NULL) return 0;
    
    pthread_mutex_lock(&nm->token_lock);
    
    if (nm->config.alias != 'A') {
        pthread_mutex_unlock(&nm->token_lock);
        return 0;
    }
    
    long now = get_timestamp_ms();
    long elapsed = now - nm->token_last_time;
    
    int duplicate = (elapsed < nm->config.min_token_time * 1000 &&
                     nm->has_token == 0 && nm->token_count > 0);
    
    pthread_mutex_unlock(&nm->token_lock);
    
    return duplicate;
}

/* Enviar dados */
int send_data(NetManager *nm, char dest, const char *message) {
    if (nm == NULL) return -1;
    
    /* Enfileirar mensagem */
    if (queue_enqueue(nm->msg_queue, dest, message) < 0) {
        print_log("ERRO: Fila de mensagens cheia!\n");
        return -1;
    }
    
    print_log("Mensagem enfileirada para %c: %s\n", dest, message);
    return 0;
}

/* Processar dados recebidos */
int process_data(NetManager *nm, DataPacket *pkt) {
    if (nm == NULL || pkt == NULL) return -1;
    
    /* Se não é para mim, encaminhar */
    if (pkt->dest != nm->config.alias && pkt->dest != 'BROADCAST') {
        print_log("DATA recebida de %c para %c, encaminhando\n",
                  pkt->origin, pkt->dest);
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(nm->machines[nm->next_index].ip);
        addr.sin_port = htons(DATA_PORT);
        
        char buffer[2048];
        serialize_data(buffer, sizeof(buffer), pkt->origin, pkt->dest,
                      pkt->status, pkt->crc, pkt->message);
        
        sendto(nm->socket_data, buffer, strlen(buffer), 0,
               (struct sockaddr *)&addr, sizeof(addr));
        
        return 0;
    }
    
    /* Para mim: verificar CRC */
    uint32_t calculated_crc = calculate_crc32((unsigned char *)pkt->message,
                                              strlen(pkt->message));
    
    print_log("DATA recebida de %c: %s\n", pkt->origin, pkt->message);
    print_log("CRC: esperado=%u, calculado=%u\n", pkt->crc, calculated_crc);
    
    /* Se for da origem, verificar resultado */
    if (pkt->origin == nm->config.alias) {
        if (strcmp(pkt->status, ACK) == 0 || strcmp(pkt->status, MACHINE_NOT_EXISTS) == 0) {
            print_log("Mensagem entregue (status: %s)\n", pkt->status);
        } else if (strcmp(pkt->status, NAK) == 0) {
            print_log("Mensagem com erro, será retransmitida\n");
        }
        
        return 0;
    }
    
    /* Responder ao originador */
    strcpy(pkt->status, (calculated_crc == pkt->crc) ? ACK : NAK);
    pkt->dest = pkt->origin;
    pkt->origin = nm->config.alias;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(nm->machines[nm->next_index].ip);
    addr.sin_port = htons(DATA_PORT);
    
    char buffer[2048];
    serialize_data(buffer, sizeof(buffer), pkt->origin, pkt->dest,
                  pkt->status, pkt->crc, pkt->message);
    
    sendto(nm->socket_data, buffer, strlen(buffer), 0,
           (struct sockaddr *)&addr, sizeof(addr));
    
    print_log("Resposta %s encaminhada\n", pkt->status);
    
    return 0;
}
