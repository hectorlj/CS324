#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 5
/* You won't lose style points for including this long line in your code */
//static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct {
    int* buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

sbuf_t sbuf;

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
/* $end sbuf_init */

/* Clean up buffer sp */
/* $begin sbuf_deinit */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
/* $end sbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}

typedef struct cacheObject{
    char host[MAXBUF];
    char filename[MAXBUF];
    char* data;
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

cacheObject* cacheFind(char* host, char* filename){
    cacheObject* temp = cache;
    while(temp != NULL){
        if(strcmp(temp->host, host) == 0 && strcmp(temp->filename, filename) == 0)
            return temp;
        else
            temp = temp->next;
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
    if(numCacheItems == MAX_CACHE_SIZE){
        cacheObject* current = cache;
        cacheObject* prev = NULL;
        while(current->next != NULL){
            prev = current;
            current = current->next;
        }
        prev->next = NULL;
        numCacheItems -= current->size;
        Free(current);
    }
}

sem_t outerQ, rsem, rmutex, wmutex, wsem;
int readcnt = 0;
int writecnt = 0;

void readerOpen(){
    P(&outerQ);
    P(&rsem);
    P(&rmutex);
    readcnt++;
    if(readcnt == 1)
        P(&wsem);
    V(&rmutex);
    V(&rsem);
    V(&outerQ);
}

void readerClose(){
    P(&rmutex);
    readcnt--;
    if(readcnt == 0)
        V(&wsem);
    V(&rmutex);
}

void writerOpen(){
    P(&wsem);
    writecnt++;
    if(writecnt == 1)
        P(&rsem);
    V(&wsem);
    P(&wmutex);
}

void writerClose(){
    V(&wmutex);
    P(&wsem);
    writecnt--;
    if(writecnt == 0)
        V(&rsem);
    V(&wsem);
}

void clienterror(int fd, char* err, char* errnum, char* msg, char* longmsg){
    char buf[MAXLINE], body[MAXBUF];
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor = \"ffffff\">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, longmsg, err);
    sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);
    
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, msg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html/\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requestheaders(rio_t* rp){
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")){
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

void doit(int fd){
    char requestBuffer[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    int sfd, s;
    struct addrinfo hints, *result, *rp;
    rio_t requestRio;
    Rio_readinitb(&requestRio, fd);
    if(!Rio_readlineb(&requestRio, requestBuffer, MAXLINE))
        return;
    printf("%s\n", requestBuffer);
    sscanf(requestBuffer, "%s, %s %s", method, uri, version);

    if(strcasecmp(method, "GET")){
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    read_requestheaders(&requestRio);

    strtok(uri, ":");
    char* host = strtok(NULL, ":");
    host++;
    host++;

    char* port = strtok(NULL, "/");
    char* filename = strtok(NULL, ":");
    char* temp = strdup(filename);
    strcpy(filename, "/");
    strcat(filename, temp);
    free(temp);
    ssize_t size;

    readerOpen();
    cacheObject* cached = cacheFind(host, filename);
    char* data;
    if(cached != NULL){
        data = cached->data;
        size = cached->size;
        Rio_writen(fd, data, size);
    }
    readerClose();
    if(cached != NULL){
        writerOpen();
        cacheInsert(host, filename, data, size);
        writerClose();
    }
    int pfd;
    rio_t responseRio;
    char responseBuffer[MAX_OBJECT_SIZE];
    char objectToCache[MAX_OBJECT_SIZE];
    char* objectToCacheIn = objectToCache;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    s = getaddrinfo(host, port, &hints, &result);
    if(s != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    for(rp = result; rp != NULL; rp = rp->ai_next){
        pfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(pfd == -1)
            continue;
        if(connect(pfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;
        close(pfd);
    }
    if(rp == NULL)
        fprintf(stderr, "Could not connect\n");
    freeaddrinfo(result);

//    pfd = Open_clientfd(host, port);
    Rio_readinitb(&responseRio, pfd);
    sprintf(responseBuffer, "GET %s HTTP/1.0\r\n", filename);
    Rio_writen(pfd, responseBuffer, strlen(responseBuffer));
    sprintf(responseBuffer, "\r\n");
    Rio_writen(pfd, responseBuffer, strlen(responseBuffer));

    size = Rio_readlineb(&responseRio, responseBuffer, MAX_OBJECT_SIZE);
    while(size != 0){
        Rio_writen(fd, responseBuffer, size);
        memcpy(objectToCacheIn, responseBuffer, size);
        objectToCacheIn += size;
        size = Rio_readlineb(&responseRio, responseBuffer, MAX_OBJECT_SIZE);
    }

    int fullSize = objectToCacheIn - objectToCache;
    writerOpen();
    cacheInsert(host, filename, objectToCache, fullSize);
    writerClose();
}

void *thread(void * vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        doit(connfd);
        Close(connfd);
    }
    return NULL;
}

int main(int argc, char** argv)
{
    int connfd, listenfd;
    struct sockaddr_storage clientaddr;
    struct sockaddr_in ipv4addr;
    ipv4addr.sin_family = AF_INET;
    ipv4addr.sin_port = htons(atoi(argv[1]));
    ipv4addr.sin_addr.s_addr = INADDR_ANY;
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    if(bind(listenfd, (struct sockaddr *)&ipv4addr, sizeof(struct sockaddr_in)) < 0){
        close(listenfd);
        perror("bind error");
        exit(EXIT_FAILURE);
    }
    if(listen(listenfd, 100) < 0){
        close(listenfd);
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    socklen_t clientlen;
    pthread_t tid;
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    
//    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, MAX_OBJECT_SIZE);
    cacheInit();
    Sem_init(&outerQ, 0, 1);
    Sem_init(&rsem, 0, 1);
    Sem_init(&rmutex, 0, 1);
    Sem_init(&wmutex, 0, 1);
    Sem_init(&wsem, 0, 1);
//    for(i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
//    }
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }

 //   printf("%s", user_agent_hdr);
//    return 0;
}
