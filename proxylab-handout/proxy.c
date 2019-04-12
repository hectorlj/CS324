#include <stdio.h>
#include "csapp.h"
#include <sys/epoll.h>

/* Recommended max cache and object sizes */

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAXEVENTS 64
#define BUFSIZE 8
#define READ_REQUEST 0
#define SEND_REQUEST 1
#define READ_RESPONSE 2
#define SEND_RESPONSE 3


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
*/
void writeLog(char* item){
    FILE *fp;
    fp = fopen("log.txt", "a");
    if(fp == NULL)
        fp = fopen("log.txt", "w");
    fprintf(fp, "%s\n", item);
    fflush(fp);
}
/*
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
//int readRequest(int clientfd);
int sendit(int serverfd);

struct event_action{
    int(*callback)(int);
    //void *arg;
    int clientfd;
    int serverfd;
    int state;
    char* bufptr;
    char buf[MAX_OBJECT_SIZE];
    int numReadClient, numWriteServer, numWrittenServer, numReadServer, numWrittenClient; 
};

int efd;
//FILE* fp;
void cliente(char* request, char* cause, char* errnum, char* shortmsg, char* longmsg){
    char* bufp = request;
    char body[MAXBUF];
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
    /* Print the HTTP response */
    sprintf(bufp, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(request, "Content-type: text/html\r\n");
    sprintf(request, "Content-length: %d\r\n\r\n", (int)strlen(body));
    bufp = body;
}

void read_requesthdrs(char* bufp){
//    char buf[MAXLINE];
//    strcpy(buf, bufp);
    printf("%s", bufp);
    while(strcmp(bufp, "\r\n")){
        printf("%s", bufp);
    }
    return;
}

void readRequest(char* requestBuffer){    
    writeLog("read req");
  //  fprintf(fp, "%s" ,"getrequest");
   // fflush(fp);
/*    struct event_action *ea;
    ea = malloc(sizeof(struct event_action));*/
    //writeLog(ea->bufptr);
    //size_t nleft;
    //ssize_t nwritten;
//    char* requestBuffer = ea->bufptr;
//    int rc;
    //struct addrinfo hints, *listp, *p;
    //requestBuffer = bufp;
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    //tempbuf = requestBuffer;
    //strcpy(tempbuf, bufp);
    printf("%s\n", requestBuffer);
    sscanf(requestBuffer, "%s, %s, %s", method, uri, version);
    writeLog(uri);
    //fopen("log.txt", "a");
    //fprintf(fp, "%s\n", uri);
    //fflush(fp);
    if(strcasecmp(method, "GET")){
        cliente(requestBuffer, method, "501", "Not Implemented", "Tiny does not implement this method");
        return ;
    }

    read_requesthdrs(requestBuffer);
    strtok(uri, ":");
    char* host = strtok(NULL, ":");
    host++;
    host++;
    char* port = strtok(NULL, "/");
    char* filename = strtok(NULL, ":");
    char *tmp = strdup(filename);
    strcpy(filename, "/");
    strcat(filename, tmp);
    free(tmp);
    int proxyfd, rc;
    struct addrinfo hints, *listp, *p;
    memset(&hints, 0, sizeof(struct addrinfo)) ;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_flags |= AI_ADDRCONFIG;
    if((rc = getaddrinfo(host, port, &hints, &listp)) != 0){
                fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", host, port, gai_strerror(rc));
    }
    for(p = listp; p; p = p->ai_next){
        if((proxyfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;
        if(connect(proxyfd, p->ai_addr, p->ai_addrlen)!=-1)
            break;
        close(proxyfd);
    }
    if (p == NULL)
        fprintf(stderr, "Could not connect");
    freeaddrinfo(listp);
    send(proxyfd, "hello\n", 5, 0);
    if(p != NULL){
        if (fcntl(proxyfd, F_SETFL, fcntl(proxyfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
            fprintf(stderr, "error setting socket option\n");
            exit(1);
        }
        //ea->serverfd = proxyfd;
        //ea->filename = filename;
        //ea->state = SEND_REQUEST;
        //ea->callback = connect_servers; 
        return ;
    }
    return ;
}

int connect_servers(int serverfd){
    return 0;
}

int sendit(int serverfd){
    struct event_action *ea;    
    ea = malloc(sizeof(struct event_action));
    memset(ea->buf, 0, sizeof(ea->buf));
    char* response = ea->buf;
//    sprintf(response, "GET %s HTTP/1.0\r\n", filename);
    return 0;
}
int main(int argc, char **argv)
{   
    
    //fp = fopen("log.txt", "a");
    writeLog("start!\n");
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    int argptr;
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

    if (fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
        fprintf(stderr, "error setting socket option\n");
        exit(1);
    }

    if((efd = epoll_create1(0)) < 0){
        fprintf(stderr, "error creating epoll fd\n");
        exit(1);
    }

    ea = malloc(sizeof(struct event_action));
    ea->callback = handle_new_client;
    //argptr = malloc(sizeof(int));
    argptr = listenfd;
    ea->clientfd = argptr;
    event.data.ptr = ea;
    event.events = EPOLLIN | EPOLLET;
    
    if(epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &event) < 0){
        fprintf(stderr, "error adding event\n");
        exit(1);
    }
   
    events = calloc(MAXEVENTS, sizeof(event));
    
    while(1){
        n = epoll_wait(efd, events, MAXEVENTS, 1000);
        for(i = 0; i < n; i++){
            ea = (struct event_action *)events[i].data.ptr;
            argptr = ea->clientfd;
            if (events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                /* An error has occured on this fd */
                fprintf (stderr, "epoll error on fd %d\n", argptr);
                close(argptr);
                //free(ea->clientfd);
                free(ea);
                continue;
            }

            if (!ea->callback(argptr)) {
                close(argptr);
                //free(ea->buf);
                free(ea);
            }
        }
    }
    free(events);
//    fclose(fp);
}

int handle_new_client(int listenfd){
    socklen_t clientlen;
    int connfd;
    struct sockaddr_storage clientaddr;
    struct epoll_event event;
    int arg;
    struct event_action *ea;

    clientlen = sizeof(struct sockaddr_storage);

    while((connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen)) > 0){
        if(fcntl(connfd, F_SETFL, fcntl(connfd, F_GETFL, 0) | O_NONBLOCK)< 0){
            fprintf(stderr, "error setting socket option\n");
            exit(1);
        }
        ea = malloc(sizeof(struct event_action));
        ea->callback = handle_client;
        
        //argptr = malloc(sizeof(int));
        arg = connfd;
        
        ea->clientfd = arg;
        event.data.ptr = ea;
        event.events = EPOLLIN | EPOLLET;
        ea->state = READ_REQUEST;
        ea->numReadClient = 0;
        ea->numWriteServer = 0;
        ea->numWrittenServer = 0; 
        ea->numReadServer = 0;
        ea->numWrittenClient = 0;
        if(epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &event) < 0){
            fprintf(stderr, "error adding event\n");
            exit(1);
        }
    }
    
    if(errno == EWOULDBLOCK || errno == EAGAIN || errno == ECONNRESET){
        close(ea->clientfd);
        return 1;
    } else {
        perror("error accepting");
        return 0;
    }
}

int handle_client(int connfd){
    writeLog("client handling");
    struct event_action *ea;
    ea = malloc(sizeof(struct event_action));
    int len; 
    char buffer[MAXLINE];
    char *buf = buffer;
    while((len = recv(connfd, buf, MAXLINE, 0)) > 0){
        ea->numReadClient += len;
        strcat(ea->buf, buf);
    }
    if(len == 0){
        ea->bufptr = buf;
        writeLog(buf);
        //fprintf(fp,"%s\n", "made it here!");
        //writeLog(ea->bufptr);
        readRequest(ea->bufptr);
        return 0;
    } else if(errno == EWOULDBLOCK || errno == EAGAIN || errno == ECONNRESET){
        //close(ea->clientfd);
        return 1;
    } else {
        perror("error reading");
        return 0;
    }
}

