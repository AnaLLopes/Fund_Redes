#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

/* Fila de mensagens */
typedef struct {
    QueueMessage messages[MAX_QUEUE];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
} MessageQueue;

/* Criar fila vazia */
MessageQueue* queue_create();

/* Adicionar mensagem na fila */
int queue_enqueue(MessageQueue *q, char dest, const char *message);

/* Remover mensagem da fila */
int queue_dequeue(MessageQueue *q, QueueMessage *msg);

/* Verificar se fila está vazia */
int queue_is_empty(MessageQueue *q);

/* Verificar se fila está cheia */
int queue_is_full(MessageQueue *q);

/* Retornar tamanho da fila */
int queue_size(MessageQueue *q);

/* Liberar fila */
void queue_destroy(MessageQueue *q);

#endif
