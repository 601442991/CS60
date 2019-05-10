#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);


// cache.h
int get(char *url, int read_write, int connfd);
void put(char *url, char *data);
void cache_init();

int main(int argc, char **argv)
{
    int listenfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }
    
    cache_init();

    port = atoi(argv[1]);               
    printf("port: %d\n", port);
    listenfd = Open_listenfd(argv[1]);  
    printf("listenfd: %d\n", listenfd);
    while(1) {
        clientlen = sizeof(clientaddr);
        int *connfd = (int*)malloc(sizeof(int));
	    *connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        pthread_t tidp;
        void *thread(void *connfd);
        Pthread_create(&tidp, NULL, thread, (void*)connfd);
        Pthread_detach(tidp);
        //doit(connfd);
	    //Close(connfd);  
        //printf("close done\n\n"); fflush(stdout);
    }
    return 0;
}
void *thread(void *connfd) {
    doit(*(int*)connfd);
    Close(*(int*)connfd);
    free(connfd);
    return NULL;
}


void doit(int connfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header[MAXLINE];
    
    char hostname[MAXLINE], path[MAXLINE];
    int port;

    rio_t rio, server_rio;

    int end_serverfd;
    // Read request line and headers
    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(connfd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }              
 
    if(get(uri, 1, connfd) != -1) {
        printf("in cache\n");
        return ;    
    } // in cache
    else {
        printf("not in cache\n");
    }
    
    //parse the uri to get hostname, file path, port 
    parse_uri(uri, hostname, path, &port); 
    //build the http header which will send to the end server
    build_http_header(endserver_http_header, hostname, path, port, &rio); 
    

    char portStr[10];
    sprintf(portStr, "%d", port);
    //connect to the end server
    end_serverfd = Open_clientfd(hostname, portStr);
    //printf("end_server = %d\n", end_serverfd); 
    if(end_serverfd < 0){ 
        printf("connection failed\n"); 
        return; 
    } 

    Rio_readinitb(&server_rio, end_serverfd); 
    //write the http header to endserver
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header)); 
    //receive message from end server and send to the client
    size_t n, size_buf = 0; 
    char cachebuf[MAX_OBJECT_SIZE];
    while((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) { 
        //printf("proxy received %d bytes,then send\n", (int)n); 
        size_buf += n;
        if(size_buf < MAX_CACHE_SIZE) strcat(cachebuf, buf);
        Rio_writen(connfd, buf, n); 
    } 
    Close(end_serverfd);

    if(size_buf < MAX_OBJECT_SIZE) {
        put(uri, cachebuf);
    }

}

//parse the uri to get hostname, port, file path 
void parse_uri(char *uri, char *hostname, char *path, int *port) { 
    // eg. "http://hostname:port/....."
    // eg. "/....."
    char* pos = strstr(uri, "//"); // skip "http://", "https://"
    pos = pos != NULL? pos+2: uri; 

    char* pos2 = strstr(pos, ":"); // "...:port"
    if(pos2 != NULL) { //"hostname:port" + "path"
        *pos2 = '\0'; 
        sscanf(pos, "%s", hostname); 
        *pos2 = ':';
        sscanf(pos2+1, "%d%s", port, path); 
    } 
    else { //"hostname"+"path"
        *port = 80; //default
        pos2 = strstr(pos, "/"); 
        if(pos2 != NULL) { 
            *pos2 = '\0'; 
            sscanf(pos, "%s", hostname); 
            *pos2 = '/'; 
            sscanf(pos2, "%s", path); 
        } 
        else sscanf(pos, "%s", hostname); 
    } 
    return; 
}

// Repost the http_header, and modify some according to the Part I
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio) { 
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
    sprintf(request_hdr, requestlint_hdr_format, path); 
    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0) { 
        if(strcmp(buf, endof_hdr) == 0) break; //EOF 
        if(!strncasecmp(buf, host_key, strlen(host_key))) { //Host  
            strcpy(host_hdr, buf); 
            continue; 
        } 
        if(!strncasecmp(buf, connection_key, strlen(connection_key))
         &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
         &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key))) { 
             strcat(other_hdr,buf); 
        } 
    } 
    if(strlen(host_hdr) == 0) //Host字段为空, 将url中的hostname作为Host字段值
        sprintf(host_hdr, host_hdr_format, hostname);
    sprintf(http_header, "%s%s%s%s%s%s%s",
        request_hdr,
        host_hdr,
        conn_hdr,
        prox_hdr,
        user_agent_hdr,
        other_hdr,
        endof_hdr); 
    return ; 
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];
    // Build the HTTP response body
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy</em>\r\n", body);
    // Print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


//////////////////////////////////////



/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define URL_LENGTH 100
#define N (MAX_CACHE_SIZE/MAX_OBJECT_SIZE)


/*cache function*/
void cache_init();
int cache_find(char *url);
int cache_eviction();
void cache_LRU(int index);
void cache_uri(char *uri, char *buf);
void readerPre(int i);
void readerAfter(int i);

struct Cache {
    int used;
    char url[URL_LENGTH];
    char data[MAX_OBJECT_SIZE];
    int time;
    
    int readCnt;            /*count of readers*/
    sem_t wmutex;           /*protects accesses to cache*/
    sem_t rdcntmutex;       /*protects accesses to readcnt*/

    //int writeCnt;
    //sem_t wtcntMutex;
    //sem_t queue;
};
struct Cache cache[N];
int time_now;

void R_pre(int i) {
    P(&cache[i].rdcntmutex);
    cache[i].readCnt++;
    if(cache[i].readCnt == 1)
        P(&cache[i].wmutex);
    V(&cache[i].rdcntmutex);
}
void R_after(int i) {
    P(&cache[i].rdcntmutex);
    cache[i].readCnt--;
    if(cache[i].readCnt == 0)
        V(&cache[i].wmutex);
    V(&cache[i].rdcntmutex);
}
void W_pre(int i) {
    P(&cache[i].wmutex);
}
void W_after(int i) {
    V(&cache[i].wmutex);
}

void cache_init() {
    for(int i = 0; i < N; i++) {
        cache[i].used = 0;
        Sem_init(&cache[i].wmutex, 0, 1);
        Sem_init(&cache[i].rdcntmutex, 0, 1);
        cache[i].readCnt = 0;
    }
}

int get(char *url, int read_write, int connfd) {
    for(int i = 0; i < N; i++) { 
        R_pre(i);
        if(cache[i].used) 
            printf("%d is used: %s\n", i, cache[i].url);
        if(cache[i].used && !strcmp(url, cache[i].url)) {
            cache[i].time = ++time_now;//atomic
            if(read_write == 1)  // read 
                Rio_writen(connfd, cache[i].data, strlen(cache[i].data));
            R_after(i);
            return i;
        }
        R_after(i);
    }
    return -1;
}

void put(char *url, char *data) {
    printf("put: %s\n", url);
    /*
    int pos = get(url, 0, -1);
    if(pos != -1) {
        W_pre(pos);
        printf("pos != -1, put in %d\n", pos);
        strcpy(cache[pos].data, data);
        W_after(pos);
        return ;
    }
    */
    int pos = -1;
    int min_time = 0x3f3f3f3f;
    for(int i = 0; i < N; i++) {
        R_pre(i);
        if(!cache[i].used) {
            pos = i;
            R_after(i);
            break ;
        }
        if(cache[i].used && cache[i].time < min_time) {
            min_time = cache[i].time;
            pos = i;
        }
        R_after(i);
    }

    W_pre(pos);
    printf("put in %d\n", pos);
    cache[pos].used = 1;
    strcpy(cache[pos].url, url);
    strcpy(cache[pos].data, data);
    W_after(pos);
}
