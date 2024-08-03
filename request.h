#ifndef __REQUEST_H__
#include <stddef.h>
#include <sys/time.h>
#include "queue.h"

typedef struct request_stat {
    size_t thread_id;
    struct timeval arrival_time;
    struct timeval dispatch_time;
    size_t total_count;
    size_t static_count;
    size_t dynamic_count;
} request_stat_t;

typedef struct {
    int id;
    Queue *q;
    struct timeval arrival_time;
} thread_args_t;

void requestHandle(int fd, request_stat_t* request_stat, void* q_ptr, int* run_last);
void* pick_event_to_run(void* q_ptr);
#endif
