#include <stdio.h>
#include "csapp.h"
#include <sys/epoll.h>

/* Recommended max cache and object sizes */

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXEVENTS 64
#define BUFSIZE 8



/* You won't lose style points for including this long line in your code */
//static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
/*
typedef struct {
    char* buf[MAXBUF];
    int n;
    int front;
    int rear;
} log_t;
log_t mylog;

void logBuf_init(log_t *sp, int n){
    sp->n  = n;
    sp->front = sp->rear = 0;
}

void logBuf_deinit(log_t *sp){
    Free(sp->buf);
}

void createLog(char* item){
    char* temp = malloc(strlen(item));
    strcpy(temp, item);
    mylog.buf[(++mylog.rear)%(mylog.n)] = temp;
}

typedef struct cacheObject{
    char host[MAXBUF];
    char filename[MAXBUF];
    char *data;
    int size;
    struct cacheObject* next;
} cacheObject;

cacheObject* cache;
int numCacheItems = 0;

void cacheInit(){
    cache = malloc(sizeof(cacheObject));
    strcpy(cache->host, "");
    strcpy(cache->filename, "");
    cache->data = NULL;
    cache->next = NULL;
    cache->size = 0;
}

cacheObject* cacheFind(char *host, char *filename) {
    cacheObject* curr = cache;
    while (curr != NULL) {
        if (strcmp(curr->host, host) == 0 && strcmp(curr->filename, filename) == 0)
            return curr;
        else
            curr = curr->next;
    }
    return NULL;
}

void cacheInsert(char* host, char* filename, char* data, int size){
    cacheObject *temp = cache;
    cache = malloc(sizeof(cacheObject));
    strcpy(cache->host, host);
    strcpy(cache->filename, filename);
    cache->data = malloc(size);
    memcpy(cache->data, data, size);
    cache->next = temp;
    cache->size = size;
    numCacheItems += size;
    if (numCacheItems == MAX_CACHE_SIZE) {
        cacheObject* curr = cache;
        cacheObject* prev = NULL;
        while (curr->next != NULL) {
            prev = curr;
            curr = curr->next;
        }
        prev->next = NULL;
        numCacheItems -= curr->size;
        Free(curr);
    }
}

void cache_deinit(){
    
        cacheObject* temp = cache;
        cacheObject* prev = NULL;
        while(temp != NULL){
            prev = temp;
            temp = temp->next;
            free(prev);
            cache = temp;
        }
   
    free(cache);
}

char* logBuf_remove(log_t *sp)
{
    char* item;
    item = sp->buf[(++sp->front)%(sp->n)]; 
    return item;
}
*/
void command(void);
int handle_client(int connfd);
int handle_new_client(int listenfd);

struct event_action{
    int(*callback)(int);
    void *arg;
    int clientfd;
    int serverfd;
    int state;
    char* buf;
    int numReadClient;
    
};

int efd;

int main(int argc, char **argv)
{   
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    int *argptr;
    struct epoll_event event;
    struct epoll_event *events;
    int i;
    int len;
    struct event_action *ea;
    //logBuf_init(&mylog, BUFSIZE);
    //cacheInit();
    size_t n;
    char buf[MAXLINE];
    struct sockaddr_in hints;
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    //listen
    memset(&hints, 0, sizeof(struct sockaddr_in));
    hints.sin_family = AF_INET;
    hints.sin_port = htons(atoi(argv[1]));
    hints.sin_addr.s_addr = INADDR_ANY;
    
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    if(bind(listenfd, (struct sockaddr *)&hints, sizeof(struct sockaddr_in)) < 0){
        close(listenfd);
        perror("bind error");
        exit(EXIT_FAILURE);
    }
    if(listen(listenfd, 100) < 0){
        close(listenfd);
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    if((efd = epoll_create1(0)) < 0){
        fprintf(stderr, "error creating epoll fd\n");
        exit(1);
    }

    ea = malloc(sizeof(struct event_action));
    ea->callback = handle_new_client;
    argptr = malloc(sizeof(int));
    *argptr = listenfd;
    ea->arg = argptr;
    event.data.ptr = ea;
    event.events = EPOLLIN | EPOLLET;
    if (fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
        fprintf(stderr, "error setting socket option\n");
        exit(1);
    }
    if(epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &event) < 0){
        fprintf(stderr, "error adding event\n");
        exit(1);
    }
   
    events = calloc(MAXEVENTS, sizeof(event));
    
    while(1){
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for(i = 0; i < n; i++){
            ea = (struct event_action *)events[i].data.ptr;
            argptr = ea->arg;
            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                /* An error has occured on this fd */
                fprintf (stderr, "epoll error on fd %d\n", *argptr);
                close(*argptr);
                free(ea->arg);
                free(ea);
                continue;
            }

            if (!ea->callback(*argptr)) {
                close(*argptr);
                free(ea->arg);
                free(ea);
            }
        }
    }
    free(events);
}

int handle_new_client(int listenfd){
    socklen_t clientlen;
    int connfd;
    struct sockaddr_storage clientaddr;
    struct epoll_event event;
    int *argptr;
    struct event_action *ea;

    clientlen = sizeof(struct sockaddr_storage);

    while((connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen)) > 0){
        if(fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK)< 0){
            fprintf(stderr, "error setting socket option\n");
            exit(1);
        }
        ea = malloc(sizeof(struct event_action));
        ea->callback = handle_client;
        argptr = malloc(sizeof(int));
        *argptr = connfd;

        ea->arg = argptr;
        event.data.ptr = ea;
        event.events = EPOLLIN | EPOLLET;
        if(epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) < 0){
            fprintf(stderr, "error adding event\n");
            exit(1);
        }
    }
    
    if(errno == EWOULDBLOCK || errno == EAGAIN){
        return 1;
    } else {
        perror("error accepting");
        return 0;
    }
}

int handle_client(int connfd){
    int len; 
    char buf[MAXLINE];
    while((len = recv(connfd, buf, MAXLINE, 0)) > 0){
        send(connfd, buf, len, 0);
    }
    if(len == 0){
        return 0;
    } else if(errno == EWOULDBLOCK || errno == EAGAIN){
        return 1;
    } else {
        perror("error reading");
        return 0;
    }
}

void doit(int fd){
    size_t nleft;
    ssize_t nwritten;
    char* bufp;
    int rc;
    struct addrinfo hints, *listp, *p;
    char requestBuffer[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    
}
