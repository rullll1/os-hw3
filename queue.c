//
// Created by rulll on 27/07/2024.
//
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

void init_queue(Queue *q, int size, int max_threads) {
    q->size = size;
    q->items = (int *)malloc(size * sizeof(int));
    if (q->items == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        exit(EXIT_FAILURE);
    }
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    q->max_threads = max_threads;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond_not_empty, NULL);
    pthread_cond_init(&q->cond_not_full, NULL);
}

void enqueue(Queue *q, int item) {
    pthread_mutex_lock(&q->mutex);

    while (q->count == q->size) {
        pthread_cond_wait(&q->cond_not_full, &q->mutex);
    }

    q->items[q->rear] = item;
    q->rear = (q->rear + 1) % q->size;
    q->count++;

    pthread_cond_signal(&q->cond_not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int dequeue(Queue *q) {
    pthread_mutex_lock(&q->mutex);

    while (q->count == 0) {
        pthread_cond_wait(&q->cond_not_empty, &q->mutex);
    }

    int item = q->items[q->front];
    q->front = (q->front + 1) % q->size;
    q->count--;

    pthread_cond_signal(&q->cond_not_full);
    pthread_mutex_unlock(&q->mutex);

    return item;
}

void destroy_queue(Queue *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond_not_empty);
    pthread_cond_destroy(&q->cond_not_full);
    free(q->items);
}

int remove_latest(Queue *q) {
    int item;

    pthread_mutex_lock(&q->mutex);

    // Wait until the queue has items
    // while (q->count == 0) {
    //     pthread_cond_wait(&q->cond_not_empty, &q->mutex);
    // }
    if (q->count != 0) {
        // Move rear backwards and remove the last item
        q->rear = (q->rear - 1 + q->size) % q->size;
        item = q->items[q->rear];
        q->count--;
    }
    else {
        item = 0;
    }
    // Signal that the queue has space
    pthread_cond_signal(&q->cond_not_full);
    pthread_mutex_unlock(&q->mutex);
    return item;
}