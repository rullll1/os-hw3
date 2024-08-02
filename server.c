#include "segel.h"
#include "request.h"
#include "queue.h"
#include <pthread.h>

typedef enum {
	BLOCK_POLICY = 1,
	DROP_TAIL_POLICY,
	DROP_HEAD_POLICY,
	BLOCK_FLUSH_POLICY
} Policy;

int available_threads;  // Variable to indicate the number of available threads
pthread_mutex_t available_threads_mutex;
pthread_cond_t cond_all_available;

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too
void getargs(int *port, int *threads_num, int *queue_size, int *schedalg, int argc, char *argv[])
{
    if (argc < 5) {
		fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
		exit(EXIT_FAILURE);
    }
    *port = atoi(argv[1]);
	*threads_num = atoi(argv[2]);
	*queue_size = atoi(argv[3]);
	const char* policyName = argv[4];
	if (strcmp(policyName, "block") == 0) {
		*schedalg = BLOCK_POLICY;
	} else if (strcmp(policyName, "drop_tail") == 0) {
		*schedalg = DROP_TAIL_POLICY;
	} else if (strcmp(policyName, "drop_head") == 0) {
		*schedalg = DROP_HEAD_POLICY;
	} else if (strcmp(policyName, "block_flush") == 0) {
		*schedalg = BLOCK_FLUSH_POLICY;
	} else {
		fprintf(stderr, "Unknown policy: %s\n", policyName);
		exit( EXIT_FAILURE);
	}
}


int main(int argc, char *argv[])
{
	int listenfd, connfd, port, clientlen, threads_num, queue_size, schedalg;
	struct sockaddr_in clientaddr;

	getargs(&port, &threads_num, &queue_size, &schedalg, argc, argv);

	Queue q;
	clientlen = sizeof(clientaddr);
	init_queue(&q, clientlen);
	pthread_mutex_init(&available_threads_mutex, NULL);
	pthread_cond_init(&cond_all_available, NULL);

	available_threads = clientlen;

    // getargs(&port, argc, argv);

    //
    // HW3: Create some threads...
    pthread_t *threads = malloc(clientlen * sizeof(pthread_t));
	int rc;
	for(int t = 0; t < threads_num; t++) {
		printf("Creating thread %d\n", t);
		thread_args_t* args = (thread_args_t*)malloc(sizeof(thread_args_t));
		if (args == NULL) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		args->id = t;
		args->q = &q;
		rc = pthread_create(&threads[t], NULL, pick_event_to_run, (void*)args);
		if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			free(threads);
			exit(EXIT_FAILURE);
		}
	}

    //
	printf("hello server\n");
    listenfd = Open_listenfd(port);
    while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
    	// int fd = open("example.txt", O_RDONLY);
    	if (q.size == q.count) { // it is full
    		if (schedalg == BLOCK_POLICY) {
    			enqueue(&q, connfd);
    		}
    		else if (schedalg == DROP_TAIL_POLICY) {
    			// dont add the latest conn
    			Close(connfd);
    		}
    		else if (schedalg == DROP_HEAD_POLICY) {
    			dequeue(&q);
    			enqueue(&q, connfd);
    		}
    		else if (schedalg == BLOCK_FLUSH_POLICY) {
    			pthread_mutex_lock(&available_threads_mutex);
    			while (available_threads != q.size) {
    				printf("we have %d threads available\n", available_threads);
    				printf("going to sleep now...\n");
    				pthread_cond_wait(&cond_all_available, &available_threads_mutex);
    				printf("after waking up, we have %d threads available\n", available_threads);
    			}
    			pthread_mutex_unlock(&available_threads_mutex);
    			Close(connfd);
    			printf("I am awake now\n");
    		}
    	}
    	else {
    		enqueue(&q, connfd);
    	}
    	printf("%d\n", connfd);
		//
		// HW3: In general, don't handle the request in the main thread.
		// Save the relevant info in a buffer and have one of the worker threads
		// do the work.
		//
		// requestHandle(connfd);

		// Close(connfd);
    }

}

// thread 1 q 1 busy 1
// thread 1 q 0 busy 1
// thread 0 q 0