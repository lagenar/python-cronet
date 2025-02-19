#include "cronet_c.h"
#include <pthread.h>

#define RUNNABLES_MAX_SIZE 10000

typedef struct {
    Cronet_RunnablePtr arr[RUNNABLES_MAX_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t mutex;
} Runnables;

void runnables_init(Runnables* q) {
    q->front = 0;
    q->rear = 0;
    q->size = 0;
    pthread_mutex_init(&q->mutex, NULL);
}

void runnables_destroy(Runnables* q) {
    pthread_mutex_destroy(&q->mutex);
}

int runnables_enqueue(Runnables* q, Cronet_RunnablePtr runnable) {
    int status = 0;
    
    pthread_mutex_lock(&q->mutex);
    if (q->size == RUNNABLES_MAX_SIZE) {
        status = -1;
    } else {
        q->arr[q->rear] = runnable;
        q->rear = (q->rear + 1) % RUNNABLES_MAX_SIZE;
        q->size++;
    }
    pthread_mutex_unlock(&q->mutex);
    
    return status;
}

Cronet_RunnablePtr runnables_dequeue(Runnables* q) {
    Cronet_RunnablePtr runnable = NULL;
    
    pthread_mutex_lock(&q->mutex);
    if (q->size > 0) {
        runnable = q->arr[q->front];
        q->front = (q->front + 1) % RUNNABLES_MAX_SIZE;
        q->size--;
    }
    pthread_mutex_unlock(&q->mutex);
    
    return runnable;
}