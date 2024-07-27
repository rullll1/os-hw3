#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct {
    int *items;
    int front;
    int rear;
    int count;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
} Queue;

void init_queue(Queue *q, int size);
void enqueue(Queue *q, int item);
int dequeue(Queue *q);
void destroy_queue(Queue *q);
int remove_latest(Queue *q);


#endif // QUEUE_H