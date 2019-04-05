#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 15
#define BUFSIZE 8
/* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct {
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

sbuf_t sbuf;

void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;                    /* Buffer holds max of n items */
    sp->front = sp->rear = 0;     /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);   /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);   /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);   /* Initially, buf has 0 items */
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);                         /* Wait for available slot */
    P(&sp->mutex);                         /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;  /* Insert the item */
    V(&sp->mutex);                         /* Unlock the buffer */
    V(&sp->items);                         /* Announce available item */
}

int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);                         /* Wait for available item */
    P(&sp->mutex);                         /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)]; /* Remove the item */
    V(&sp->mutex);                         /* Unlock the buffer */
    V(&sp->slots);                         /* Announce available slot */
    return item;
}

sem_t outerQ, rsem, rmutex, wmutex, wsem;
int readcnt = 0;
int writecnt = 0;

void readerOpen() {
    P(&outerQ);
    P(&rsem);
    P(&rmutex);
    readcnt++;
    if (readcnt == 1)
        P(&wsem);
    V(&rmutex);
    V(&rsem);
    V(&outerQ);
}

void readerClose() {
    P(&rmutex);
    readcnt--;
    if (readcnt == 0)
        V(&wsem);
    V(&rmutex);
}

void writerOpen(){
    P(&wsem);
    writecnt ++;
    if(writecnt == 1){
        P(&rsem);
    }
    V(&wsem);
    P(&wmutex);
}

void writerClose() {
    V(&wmutex);
    P(&wsem);
    writecnt--;
    if (writecnt == 0)
        V(&rsem);
    V(&wsem);
}

typedef struct {
    char *buf[MAXBUF];
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} log_t;

log_t mylog;

void logBuf_init(log_t *sp, int n){
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void logBuf_deinit(log_t *sp){
    Free(sp->buf);
}

void createLog(char* item){
    char* temp = malloc(strlen(item));
    strcpy(temp, item);
    P(&mylog.slots);
    P(&mylog.mutex);
    mylog.buf[(++mylog.rear)%(mylog.n)] = temp;
    V(&mylog.mutex);
    V(&mylog.items);
}

/* Remove and return the first item from buffer sp */
char* logBuf_remove(log_t *sp)
{
    char* item;
    P(&sp->items);                         /* Wait for available item */
    P(&sp->mutex);                         /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)]; /* Remove the item */
    V(&sp->mutex);                         /* Unlock the buffer */
    V(&sp->slots);                         /* Announce available slot */
    return item;
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


void cacheInit() {
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

void cacheInsert(char *host, char *filename, char *data, int size) {
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

static ssize_t my_rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
               sizeof(rp->rio_buf));
    //if (rp->rio_cnt < 0) {
      //  if (errno != EINTR) /* Interrupted by sig handler return */
    //}
     if (rp->rio_cnt == 0)  /* EOF */
        return 0;
    else 
        rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;          
    if (rp->rio_cnt < n)   
    cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}

ssize_t my_rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        if ((rc = my_rio_read(rp, &c, 1)) == 1) {
        *bufp++ = c;
        if (c == '\n') {
                n++;
            break;
            }
    } else if (rc == 0) {
        if (n == 1)
        return 0; /* EOF, no data read */
        else
        break;    /* EOF, some data was read */
    }
    }
    *bufp = 0;
    return n-1;
}

void my_rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}

void cliente(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf [MAXLINE], body[MAXBUF];
    

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    size_t nleft = strlen(buf);
    ssize_t nwritten = 0;
    char *bufp = buf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            //if (errno == EINTR)  /* Interrupted by sig handler return */
            //nwritten = 0;    /* and call write() again */
                   /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    sprintf(buf, "Content-type: text/html\r\n");
    nleft = strlen(buf);
    nwritten = 0;
    bufp = buf;
    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* Interrupted by sig handler return */
            nwritten = 0;    /* and call write() again */
                   /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    nleft = strlen(buf);
    nwritten = 0;
    bufp = buf;
    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* Interrupted by sig handler return */
            nwritten = 0;    /* and call write() again */
                   /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    nleft = strlen(body);
    nwritten = 0;
    bufp = body;
    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* Interrupted by sig handler return */
            nwritten = 0;    /* and call write() again */
                   /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
}

void read_requesthdrs(rio_t * rp)
{
    char buf[MAXLINE];
    my_rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n")) {
        my_rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}



void doit(int fd)
{
    /* Variables for Request */
    size_t nleft;
    ssize_t nwritten;
    char* bufp;
    int rc;
    struct addrinfo hints, *listp, *p;


    char requestBuffer[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t requestRio;

    /* Read request line and headers */
    my_rio_readinitb(&requestRio, fd);
    if (!my_rio_readlineb(&requestRio, requestBuffer, MAXLINE))
        return;
    printf("%s\n", requestBuffer);

    sscanf(requestBuffer, "%s %s %s", method, uri, version);
//    char* url = uri;
//    strcpy(url, uri);
    createLog(uri);
    /* If it's not a GET request */
    if (strcasecmp(method, "GET")) {
        cliente(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    
    read_requesthdrs(&requestRio);

    /* Parse URI*/

    /* Type: HTTP */
    strtok(uri, ":");

    /* Host */
    char* host = strtok(NULL, ":");
    host++;
    host++;

    /* Port */
    char* port = strtok(NULL, "/");

    /* File Path */
    char *filename = strtok(NULL, ":");
    char *tmp = strdup(filename);
    strcpy(filename, "/");
    strcat(filename, tmp);
    free(tmp);
    ssize_t size;
    
    readerOpen();
    cacheObject *cachedObject = cacheFind(host, filename);
    
    char* data;
        
    if (cachedObject != NULL) {
        
        data = cachedObject->data;
        size = cachedObject->size;
        size_t nleft = strlen(data);
        ssize_t nwritten = 0;
        
        while(nleft > 0){
            if((nwritten = write(fd, data, nleft)) <= 0){
                if(errno == EINTR)
                    nwritten = 0;
            }
        }
        nleft -= nwritten;
        data += nwritten;
        //Rio_writen(fd, data, size);
    }
    readerClose();
    
    /* Variables for Forwarded Request */
    int proxyfd;
    rio_t responseRio;
    char responseBuffer[MAX_OBJECT_SIZE];


    char objectToCache[MAX_OBJECT_SIZE];
    char* objectToCacheIndex = objectToCache;


    /* Open Proxy Connection */
    //proxyfd = Open_clientfd(host, port);
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_flags |= AI_ADDRCONFIG;
    if((rc = getaddrinfo(host, port, &hints, &listp)) != 0){
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", host, port, gai_strerror(rc));
    }
    for(p = listp; p; p = p->ai_next){
        if((proxyfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;
        if(connect(proxyfd, p->ai_addr, p->ai_addrlen) != -1)
            break;
        close(proxyfd);  
    }

    if(p == NULL)
        fprintf(stderr, "Could not connect");
    freeaddrinfo(listp);

    /* Forward Request to Proxy */
    my_rio_readinitb(&responseRio, proxyfd);
    sprintf(responseBuffer, "GET %s HTTP/1.0\r\n", filename);

    nleft = strlen(responseBuffer);
    nwritten = 0;
    bufp = responseBuffer;

    while (nleft > 0) {
        if ((nwritten = write(proxyfd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* Interrupted by sig handler return */
            nwritten = 0;    /* and call write() again */
                   /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    sprintf(responseBuffer, "\r\n");
    nleft = strlen(responseBuffer);
    nwritten = 0;
    bufp = responseBuffer;

    while (nleft > 0) {
        if ((nwritten = write(proxyfd, bufp, nleft)) <= 0) {
            if (errno == EINTR)  /* Interrupted by sig handler return */
            nwritten = 0;    /* and call write() again */
                   /* errno set by write() */
        }
        nleft -= nwritten;
        bufp += nwritten;
    }

    /* Send Forwarded Response to Client */
    size = my_rio_readlineb(&responseRio, responseBuffer, MAX_OBJECT_SIZE);

    while (size != 0) {

        nleft = size;
        nwritten = 0;
        bufp = responseBuffer;

        while (nleft > 0) {
            if ((nwritten = write(fd, bufp, nleft)) <= 0) {
                if (errno == EINTR)  
                nwritten = 0;    
                       
            }
            nleft -= nwritten;
            bufp += nwritten;
        }

//        Rio_writen(fd, responseBuffer, size);
        memcpy(objectToCacheIndex, responseBuffer, size);
        objectToCacheIndex += size;

        size = my_rio_readlineb(&responseRio, responseBuffer, MAX_OBJECT_SIZE);
        
    }
    //createLog(url);
    
    int fullsize = objectToCacheIndex - objectToCache;
    writerOpen();
        cacheInsert(host, filename, objectToCache, fullsize);
    writerClose();
}


/* Thread routine */
void *thread(void *vargp)
{
    //int connfd = *((int*)vargp);
    Pthread_detach(pthread_self()); 
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        //Free(vargp);
        doit(connfd);
        Close(connfd);
        
    }
    return NULL;
}

void *logThread(void *vargp){
    Pthread_detach(pthread_self());
    FILE *fp;
    fp = fopen("log.txt", "a");
    if(fp == NULL){
        fp = fopen("log.txt", "w");
    }
    while(1){
        char* item = logBuf_remove(&mylog);
        fprintf(fp, "%s\n", item);
        fflush(fp);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    int listenfd, *connfd;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    struct sockaddr_in hints;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

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
    sbuf_init(&sbuf, BUFSIZE);
    logBuf_init(&mylog, BUFSIZE);
    cacheInit();
    Sem_init(&outerQ, 0, 1);
    Sem_init(&rsem, 0, 1);
    Sem_init(&rmutex, 0, 1);
    Sem_init(&wmutex, 0, 1);
    Sem_init(&wsem, 0, 1);
    Pthread_create(&tid, NULL, logThread, NULL);
    for(int i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }
    
    socklen_t clientlen;
    while (1) {
        //connfd = Malloc(sizeof(int));
        clientlen = sizeof(struct sockaddr_storage);
        int temp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        connfd = malloc(sizeof(int));
        *connfd = temp;
        
 //       Pthread_create(&tid, NULL, thread, connfd);
        sbuf_insert(&sbuf, *connfd);
        free(connfd);
    }
//    Free(connfd);
}

