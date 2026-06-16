#include "queue.h"

/* Criar fila vazia */
MessageQueue* queue_create() {
    MessageQueue *q = (MessageQueue*)malloc(sizeof(MessageQueue));
    if (q == NULL) return NULL;
    
    q->front = 0;
    q->rear = -1;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    memset(q->messages, 0, sizeof(q->messages));
    
    return q;
}

/* Adicionar mensagem na fila */
int queue_enqueue(MessageQueue *q, char dest, const char *message) {
    if (q == NULL) return -1;
    
    pthread_mutex_lock(&q->lock);
    
    if (q->count >= MAX_QUEUE) {
        pthread_mutex_unlock(&q->lock);
        return -1; /* Fila cheia */
    }
    
    q->rear = (q->rear + 1) % MAX_QUEUE;
    q->messages[q->rear].dest_alias = dest;
    strncpy(q->messages[q->rear].message, message, MAX_MSG_LENGTH - 1);
    q->messages[q->rear].message[MAX_MSG_LENGTH - 1] = '\0';
    q->messages[q->rear].attempts = 0;
    q->count++;
    
    pthread_mutex_unlock(&q->lock);
    return 0;
}

/* Remover mensagem da fila */
int queue_dequeue(MessageQueue *q, QueueMessage *msg) {
    if (q == NULL || msg == NULL) return -1;
    
    pthread_mutex_lock(&q->lock);
    
    if (q->count <= 0) {
        pthread_mutex_unlock(&q->lock);
        return -1; /* Fila vazia */
    }
    
    memcpy(msg, &q->messages[q->front], sizeof(QueueMessage));
    q->front = (q->front + 1) % MAX_QUEUE;
    q->count--;
    
    pthread_mutex_unlock(&q->lock);
    return 0;
}

/* Verificar se fila está vazia */
int queue_is_empty(MessageQueue *q) {
    if (q == NULL) return 1;
    
    pthread_mutex_lock(&q->lock);
    int empty = (q->count == 0);
    pthread_mutex_unlock(&q->lock);
    
    return empty;
}

/* Verificar se fila está cheia */
int queue_is_full(MessageQueue *q) {
    if (q == NULL) return 1;
    
    pthread_mutex_lock(&q->lock);
    int full = (q->count >= MAX_QUEUE);
    pthread_mutex_unlock(&q->lock);
    
    return full;
}

/* Retornar tamanho da fila */
int queue_size(MessageQueue *q) {
    if (q == NULL) return 0;
    
    pthread_mutex_lock(&q->lock);
    int size = q->count;
    pthread_mutex_unlock(&q->lock);
    
    return size;
}

/* Liberar fila */
void queue_destroy(MessageQueue *q) {
    if (q == NULL) return;
    pthread_mutex_destroy(&q->lock);
    free(q);
}
