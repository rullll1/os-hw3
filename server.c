#include "segel.h"
#include "request.h"
#include "queue.h"
#include <pthread.h>

#define BLOCK_POLICY 1
#define DROP_TAIL_POLICY 2
#define DROP_HEAD_POLICY 3
#define BLOCK_FLUSH_POLICY 4
#define DROP_RANDOM_POLICY 5

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
void getargs(int *port, int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
}


// int main(int argc, char *argv[])
int main()
{

    int listenfd, connfd, port, clientlen, policy;
    struct sockaddr_in clientaddr;
	port = 8083;
	clientlen = 2;
	int q_size = 3;
	policy = BLOCK_FLUSH_POLICY;
	Queue q;
	init_queue(&q, q_size);
	pthread_mutex_init(&available_threads_mutex, NULL);
	pthread_cond_init(&cond_all_available, NULL);

	available_threads = clientlen;

    // getargs(&port, argc, argv);

    // 
    // HW3: Create some threads...
    pthread_t *threads = malloc(clientlen * sizeof(pthread_t));
	int rc;
	for(int t = 0; t < clientlen; t++) {
		// printf("Creating thread %d\n", t);
		rc = pthread_create(&threads[t], NULL, pick_event_to_run, (void*)&q);
		if (rc) {
			// printf("ERROR; return code from pthread_create() is %d\n", rc);
			free(threads);
			exit(-1);
		}
	}

    //
	// printf("hello server\n");
    listenfd = Open_listenfd(port);
    while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
    	// int fd = open("example.txt", O_RDONLY);
    	if (q.size == q.count) { // it is full
    		if (policy == BLOCK_POLICY) {
    			enqueue(&q, connfd);
    		}
    		else if (policy == DROP_TAIL_POLICY) {
    			// dont add the latest conn
    			Close(connfd);
    		}
    		else if (policy == DROP_HEAD_POLICY) {
    			dequeue(&q);
    			enqueue(&q, connfd);
    		}
    		else if (policy == BLOCK_FLUSH_POLICY) {
    			pthread_mutex_lock(&available_threads_mutex);
    			while (available_threads != q.size) {
    				// printf("we have %d threads available\n", available_threads);
    				// printf("going to sleep now...\n");
    				pthread_cond_wait(&cond_all_available, &available_threads_mutex);
    				// printf("after waking up, we have %d threads available\n", available_threads);
    			}
    			pthread_mutex_unlock(&available_threads_mutex);
    			Close(connfd);
    			// printf("I am awake now\n");
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