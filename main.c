#include "common.h"
#include "network_ops.h"
#include "queue.h"
#include "packet.h"
#include "crc32.h"
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>

NetManager *global_nm = NULL;

/* Thread para receber descoberta e saudações */
void* discover_thread(void *arg) {
    NetManager *nm = (NetManager *)arg;
    char buffer[512];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    print_log("Thread DISCOVER iniciada\n");
    
    while (nm->run) {
        memset(buffer, 0, sizeof(buffer));
        
        int n = recvfrom(nm->socket_discover, buffer, sizeof(buffer) - 1, 0,
                        (struct sockaddr *)&addr, &addr_len);
        
        if (n < 0) {
            if (errno != EINTR) {
                perror("recvfrom discover");
            }
            continue;
        }
        
        buffer[n] = '\0';
        
        int type = get_packet_type(buffer);
        
        if (type == PACKET_DISCOVER) {
            process_discover(nm, buffer, &addr);
        } else if (type == PACKET_HELLO) {
            process_hello(nm, buffer);
        }
    }
    
    print_log("Thread DISCOVER finalizada\n");
    return NULL;
}

/* Thread para receber tokens e dados */
void* data_thread(void *arg) {
    NetManager *nm = (NetManager *)arg;
    char buffer[2048];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    print_log("Thread DATA iniciada\n");
    
    while (nm->run) {
        memset(buffer, 0, sizeof(buffer));
        
        int n = recvfrom(nm->socket_data, buffer, sizeof(buffer) - 1, 0,
                        (struct sockaddr *)&addr, &addr_len);
        
        if (n < 0) {
            if (errno != EINTR) {
                perror("recvfrom data");
            }
            continue;
        }
        
        buffer[n] = '\0';
        
        int type = get_packet_type(buffer);
        
        if (type == PACKET_TOKEN) {
            print_log("TOKEN recebido!\n");
            
            pthread_mutex_lock(&nm->token_lock);
            nm->has_token = 1;
            nm->token_count++;
            pthread_cond_signal(&nm->token_cond);
            pthread_mutex_unlock(&nm->token_lock);
            
        } else if (type == PACKET_DATA) {
            DataPacket pkt;
            if (deserialize_data(buffer, &pkt) == 0) {
                process_data(nm, &pkt);
            }
        }
    }
    
    print_log("Thread DATA finalizada\n");
    return NULL;
}

/* Thread para controlar token (máquina A) */
void* token_controller_thread(void *arg) {
    NetManager *nm = (NetManager *)arg;
    
    print_log("Thread CONTROLADOR DE TOKEN iniciada\n");
    
    /* Apenas máquina A controla token */
    if (nm->config.alias != 'A') {
        return NULL;
    }
    
    /* Aguardar rede se formar */
    sleep(2);
    
    /* Gerar token inicial */
    pthread_mutex_lock(&nm->token_lock);
    nm->has_token = 1;
    nm->token_count = 1;
    nm->total_tokens_generated = 1;
    nm->token_last_time = get_timestamp_ms();
    pthread_cond_signal(&nm->token_cond);
    print_log("TOKEN INICIAL GERADO\n");
    pthread_mutex_unlock(&nm->token_lock);
    
    while (nm->run) {
        sleep(1);
        
        /* Verificar timeout */
        if (token_timeout_occurred(nm)) {
            print_log("ALERTA: Token perdido! (timeout excedido)\n");
            
            pthread_mutex_lock(&nm->token_lock);
            nm->has_token = 1;
            nm->token_count = 1;
            nm->total_tokens_generated++;
            nm->token_last_time = get_timestamp_ms();
            pthread_cond_signal(&nm->token_cond);
            print_log("Novo token gerado\n");
            pthread_mutex_unlock(&nm->token_lock);
        }
        
        /* Verificar token duplicado */
        if (duplicate_token_detected(nm)) {
            print_log("ALERTA: Múltiplos tokens na rede! Removendo...\n");
            
            pthread_mutex_lock(&nm->token_lock);
            nm->token_count--;
            pthread_mutex_unlock(&nm->token_lock);
        }
    }
    
    return NULL;
}

/* Thread para gerenciar transmissão de dados quando tem token */
void* transmission_thread(void *arg) {
    NetManager *nm = (NetManager *)arg;
    
    print_log("Thread TRANSMISSÃO iniciada\n");
    
    while (nm->run) {
        pthread_mutex_lock(&nm->token_lock);
        
        /* Aguardar receber token */
        while (!nm->has_token && nm->run) {
            pthread_cond_wait(&nm->token_cond, &nm->token_lock);
        }
        
        if (!nm->run) {
            pthread_mutex_unlock(&nm->token_lock);
            break;
        }
        
        pthread_mutex_unlock(&nm->token_lock);
        
        /* Verificar se há mensagens na fila */
        if (!queue_is_empty(nm->msg_queue)) {
            QueueMessage msg;
            
            if (queue_dequeue(nm->msg_queue, &msg) == 0) {
                print_log("Transmitindo mensagem de fila para %c\n", msg.dest_alias);
                
                /* Calcular CRC da mensagem */
                uint32_t crc = calculate_crc32((unsigned char *)msg.message,
                                              strlen(msg.message));
                
                /* Inserir erro aleatoriamente */
                char status[20];
                strcpy(status, MACHINE_NOT_EXISTS);
                
                if (msg.dest_alias != 'BROADCAST') {
                    /* Verificar se máquina existe */
                    pthread_mutex_lock(&nm->net_lock);
                    int found = 0;
                    for (int i = 0; i < nm->num_machines; i++) {
                        if (nm->machines[i].alias == msg.dest_alias) {
                            found = 1;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&nm->net_lock);
                    
                    if (!found && msg.dest_alias != 'B') {
                        strcpy(status, MACHINE_NOT_EXISTS);
                    }
                }
                
                /* Enviar para próxima máquina */
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                
                pthread_mutex_lock(&nm->net_lock);
                if (nm->next_index >= 0 && nm->next_index < nm->num_machines) {
                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = inet_addr(nm->machines[nm->next_index].ip);
                    addr.sin_port = htons(DATA_PORT);
                } else {
                    pthread_mutex_unlock(&nm->net_lock);
                    continue;
                }
                pthread_mutex_unlock(&nm->net_lock);
                
                char buffer[2048];
                serialize_data(buffer, sizeof(buffer), nm->config.alias, msg.dest_alias,
                              status, crc, msg.message);
                
                sendto(nm->socket_data, buffer, strlen(buffer), 0,
                       (struct sockaddr *)&addr, sizeof(addr));
                
                print_log("DATA enviada: %s\n", buffer);
            }
        }
        
        /* Aguardar um tempo antes de passar token */
        sleep(nm->config.token_time);
        
        /* Passar token para próximo */
        pthread_mutex_lock(&nm->net_lock);
        int next = nm->next_index;
        pthread_mutex_unlock(&nm->net_lock);
        
        send_token(nm, next);
    }
    
    return NULL;
}

/* Interface de linha de comando */
void* command_thread(void *arg) {
    NetManager *nm = (NetManager *)arg;
    char line[512];
    
    print_log("Thread COMANDOS iniciada\n");
    print_log("Comandos disponíveis:\n");
    print_log("  s <dest> <msg> - enviar mensagem\n");
    print_log("  t - ver status do token\n");
    print_log("  r - ver anel\n");
    print_log("  q - sair\n");
    
    while (nm->run) {
        printf("\n> ");
        fflush(stdout);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        
        line[strcspn(line, "\n")] = '\0';
        
        if (strlen(line) == 0) continue;
        
        char cmd = line[0];
        
        switch (cmd) {
            case 's': {
                /* Enviar mensagem */
                char dest;
                char msg[512];
                int n = sscanf(line, "s %c %[^\n]", &dest, msg);
                
                if (n >= 2) {
                    send_data(nm, dest, msg);
                } else {
                    printf("Uso: s <destino> <mensagem>\n");
                }
                break;
            }
            
            case 't': {
                /* Status token */
                pthread_mutex_lock(&nm->token_lock);
                printf("Token status: has=%d, count=%d, total_gerados=%d\n",
                       nm->has_token, nm->token_count, nm->total_tokens_generated);
                pthread_mutex_unlock(&nm->token_lock);
                break;
            }
            
            case 'r': {
                /* Ver anel */
                pthread_mutex_lock(&nm->net_lock);
                printf("Anel atual (%d máquinas): ", nm->num_machines);
                for (int i = 0; i < nm->num_machines; i++) {
                    printf("%c ", nm->machines[i].alias);
                }
                printf("\nMáquina atual: %c (índice %d), próximo: %c (índice %d)\n",
                       nm->config.alias, nm->my_index,
                       nm->machines[nm->next_index].alias, nm->next_index);
                pthread_mutex_unlock(&nm->net_lock);
                break;
            }
            
            case 'q': {
                /* Sair */
                printf("Encerrando...\n");
                nm->run = 0;
                break;
            }
            
            default:
                printf("Comando desconhecido\n");
        }
    }
    
    nm->run = 0;
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <config_file>\n", argv[0]);
        printf("Exemplo: %s config.txt\n", argv[0]);
        return 1;
    }
    
    print_log("===== REDE EM ANEL - Token Ring =====\n");
    
    /* Inicializar rede */
    global_nm = network_init(argv[1]);
    if (global_nm == NULL) {
        fprintf(stderr, "Erro ao inicializar rede\n");
        return 1;
    }
    
    /* Enviar DISCOVER */
    send_discover(global_nm);
    
    /* Criar threads */
    pthread_t tid_discover, tid_data, tid_token, tid_transmission, tid_cmd;
    
    pthread_create(&tid_discover, NULL, discover_thread, global_nm);
    pthread_create(&tid_data, NULL, data_thread, global_nm);
    pthread_create(&tid_token, NULL, token_controller_thread, global_nm);
    pthread_create(&tid_transmission, NULL, transmission_thread, global_nm);
    pthread_create(&tid_cmd, NULL, command_thread, global_nm);
    
    /* Aguardar threads */
    pthread_join(tid_cmd, NULL);
    pthread_join(tid_discover, NULL);
    pthread_join(tid_data, NULL);
    pthread_join(tid_token, NULL);
    pthread_join(tid_transmission, NULL);
    
    /* Limpar */
    network_cleanup(global_nm);
    
    print_log("Programa encerrado\n");
    
    return 0;
}
