//
// request.c: Does the bulk of the work for the web server.
// 

#include "segel.h"
#include "request.h"
#include "queue.h"

int available_threads;  // Variable to indicate the number of available threads
pthread_mutex_t available_threads_mutex;
pthread_cond_t cond_all_available;

// requestError(      fd,    filename,        "404",    "Not found", "OS-HW3 Server could not find this file");
void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg, request_stat_t* request_stat)
{
   char buf[MAXLINE], body[MAXBUF];

   // Create the body of the error message
   sprintf(body, "<html><title>OS-HW3 Error</title>");
   sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
   sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
   sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
   sprintf(body, "%s<hr>OS-HW3 Web Server\r\n", body);

   // Write out the header information for this response
   sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
   Rio_writen(fd, buf, strlen(buf));
   printf("%s", buf);

   sprintf(buf, "Content-Type: text/html\r\n");
   Rio_writen(fd, buf, strlen(buf));
   printf("%s", buf);

   sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
   sprintf(buf, "%sStat-Req-Arrival:: %ld.%06ld\r\n", buf, request_stat->arrival_time.tv_sec, request_stat->arrival_time.tv_usec);
   sprintf(buf, "%sStat-Req-Dispatch:: %ld.%06ld\r\n", buf, request_stat->dispatch_time.tv_sec, request_stat->dispatch_time.tv_usec);
   sprintf(buf, "%sStat-Thread-Id:: %ld\r\n", buf, request_stat->thread_id);
   sprintf(buf, "%sStat-Thread-Count:: %ld\r\n", buf, request_stat->total_count);
   sprintf(buf, "%sStat-Thread-Static:: %ld\r\n", buf, request_stat->static_count);
   sprintf(buf, "%sStat-Thread-Dynamic:: %ld\r\n\r\n", buf, request_stat->dynamic_count);
   Rio_writen(fd, buf, strlen(buf));
   printf("%s", buf);
   // Write out the content
   Rio_writen(fd, body, strlen(body));
   printf("%s", body);

}


//
// Reads and discards everything up to an empty text line
//
void requestReadhdrs(rio_t *rp)
{
   char buf[MAXLINE];

   Rio_readlineb(rp, buf, MAXLINE);
   while (strcmp(buf, "\r\n")) {
      Rio_readlineb(rp, buf, MAXLINE);
   }
   return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int requestParseURI(char *uri, char *filename, char *cgiargs) 
{
   char *ptr;

   if (strstr(uri, "..")) {
      sprintf(filename, "./public/home.html");
      return 1;
   }

   if (!strstr(uri, "cgi")) {
      // static
      strcpy(cgiargs, "");
      sprintf(filename, "./public/%s", uri);
      if (uri[strlen(uri)-1] == '/') {
         strcat(filename, "home.html");
      }
      return 1;
   } else {
      // dynamic
      ptr = index(uri, '?');
      if (ptr) {
         strcpy(cgiargs, ptr+1);
         *ptr = '\0';
      } else {
         strcpy(cgiargs, "");
      }
      sprintf(filename, "./public/%s", uri);
      return 0;
   }
}

//
// Fills in the filetype given the filename
//
void requestGetFiletype(char *filename, char *filetype)
{
   if (strstr(filename, ".html")) 
      strcpy(filetype, "text/html");
   else if (strstr(filename, ".gif")) 
      strcpy(filetype, "image/gif");
   else if (strstr(filename, ".jpg")) 
      strcpy(filetype, "image/jpeg");
   else 
      strcpy(filetype, "text/plain");
}

void requestServeDynamic(int fd, char *filename, char *cgiargs, request_stat_t* request_stat)
{
   char buf[MAXLINE], *emptylist[] = {NULL};

   // The server does only a little bit of the header.  
   // The CGI script has to finish writing out the header.
   sprintf(buf, "HTTP/1.0 200 OK\r\n");
   sprintf(buf, "%sServer: OS-HW3 Web Server\r\n", buf);
   sprintf(buf, "%sStat-Req-Arrival:: %ld.%06ld\r\n", buf, request_stat->arrival_time.tv_sec, request_stat->arrival_time.tv_usec);
   sprintf(buf, "%sStat-Req-Dispatch:: %ld.%06ld\r\n", buf, request_stat->dispatch_time.tv_sec, request_stat->dispatch_time.tv_usec);
   sprintf(buf, "%sStat-Thread-Id:: %ld\r\n", buf, request_stat->thread_id);
   sprintf(buf, "%sStat-Thread-Count:: %ld\r\n", buf, request_stat->total_count);
   sprintf(buf, "%sStat-Thread-Static:: %ld\r\n", buf, request_stat->static_count);
   sprintf(buf, "%sStat-Thread-Dynamic:: %ld\r\n", buf, request_stat->dynamic_count);

   Rio_writen(fd, buf, strlen(buf));
   int pid = 0;
   if ((pid = Fork()) == 0) {
      /* Child process */
      Setenv("QUERY_STRING", cgiargs, 1);
      /* When the CGI process writes to stdout, it will instead go to the socket */
      Dup2(fd, STDOUT_FILENO);
      Execve(filename, emptylist, environ);
   }
   WaitPid(pid, NULL, WUNTRACED);
}


void requestServeStatic(int fd, char *filename, int filesize, request_stat_t* request_stat)
{
   int srcfd;
   char *srcp, filetype[MAXLINE], buf[MAXBUF];

   requestGetFiletype(filename, filetype);

   srcfd = Open(filename, O_RDONLY, 0);

   // Rather than call read() to read the file into memory, 
   // which would require that we allocate a buffer, we memory-map the file
   srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
   Close(srcfd);

   // put together response
   sprintf(buf, "HTTP/1.0 200 OK\r\n");
   sprintf(buf, "%sServer: OS-HW3 Web Server\r\n", buf);
   sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
   sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);
   sprintf(buf, "%sStat-Req-Arrival:: %ld.%06ld\r\n", buf, request_stat->arrival_time.tv_sec, request_stat->arrival_time.tv_usec);
   sprintf(buf, "%sStat-Req-Dispatch:: %ld.%06ld\r\n", buf, request_stat->dispatch_time.tv_sec, request_stat->dispatch_time.tv_usec);
   sprintf(buf, "%sStat-Thread-Id:: %ld\r\n", buf, request_stat->thread_id);
   sprintf(buf, "%sStat-Thread-Count:: %ld\r\n", buf, request_stat->total_count);
   sprintf(buf, "%sStat-Thread-Static:: %ld\r\n", buf, request_stat->static_count);
   sprintf(buf, "%sStat-Thread-Dynamic:: %ld\r\n\r\n", buf, request_stat->dynamic_count);

   Rio_writen(fd, buf, strlen(buf));

   //  Writes out to the client socket the memory-mapped file 
   Rio_writen(fd, srcp, filesize);
   Munmap(srcp, filesize);

}

// handle a request
void* pick_event_to_run(void* arg)
{
   thread_args_t* args = (thread_args_t*)arg;
   Queue* q = args->q;
   int fd;
   request_stat_t request_stat = { 0 };
   request_stat.thread_id = args->id;
   printf("we have %d\n", available_threads);
   while (1) {
      fd = dequeue(q);
      pthread_mutex_lock(&available_threads_mutex);
      available_threads--;  // Mark thread as busy
      pthread_mutex_unlock(&available_threads_mutex);
      gettimeofday(&request_stat.dispatch_time, NULL);
      gettimeofday(&request_stat.arrival_time, NULL); // TODO
      printf("Consuming %d\n", fd);

      sleep(15);
      requestHandle(fd, &request_stat);
      timersub(&request_stat.dispatch_time, &request_stat.arrival_time, &request_stat.dispatch_time);
      Close(fd);
      pthread_mutex_lock(&available_threads_mutex);
      available_threads++;  // Mark thread as busy
      if (available_threads == q->size && q->size != q->count) {
         pthread_cond_signal(&cond_all_available);
      }
      pthread_mutex_unlock(&available_threads_mutex);

   }
}

void requestHandle(int fd, request_stat_t* request_stat)
{

   int is_static;
   struct stat sbuf;
   char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
   char filename[MAXLINE], cgiargs[MAXLINE];
   rio_t rio;

   request_stat->total_count++;

   Rio_readinitb(&rio, fd);
   Rio_readlineb(&rio, buf, MAXLINE);
   sscanf(buf, "%s %s %s", method, uri, version);

   printf("%s %s %s\n", method, uri, version);

   if (strcasecmp(method, "GET")) {
      requestError(fd, method, "501", "Not Implemented", "OS-HW3 Server does not implement this method", request_stat);
      return;
   }
   requestReadhdrs(&rio);

   is_static = requestParseURI(uri, filename, cgiargs);
   if (stat(filename, &sbuf) < 0) {
      requestError(fd, filename, "404", "Not found", "OS-HW3 Server could not find this file", request_stat);
      return;
   }

   if (is_static) {
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
         requestError(fd, filename, "403", "Forbidden", "OS-HW3 Server could not read this file", request_stat);
         return;
      }
      request_stat->static_count++;
      requestServeStatic(fd, filename, sbuf.st_size, request_stat);
   } else {
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
         requestError(fd, filename, "403", "Forbidden", "OS-HW3 Server could not run this CGI program", request_stat);
         return;
      }
      request_stat->dynamic_count++;
      requestServeDynamic(fd, filename, cgiargs, request_stat);
   }
}


