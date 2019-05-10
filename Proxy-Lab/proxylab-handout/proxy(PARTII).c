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



int main(int argc, char **argv)
{
    int listenfd, port;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    if(argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }
    port = atoi(argv[1]);               printf("port: %d\n", port);
    listenfd = Open_listenfd(argv[1]);  printf("listenfd: %d\n", listenfd);
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
        printf("close done\n\n"); fflush(stdout);
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

    //parse the uri to get hostname, file path, port 
    parse_uri(uri, hostname, path, &port); 
    //build the http header which will send to the end server
    build_http_header(endserver_http_header, hostname, path, port, &rio); 
    
    char portStr[10];
    sprintf(portStr, "%d", port);
    //connect to the end server
    end_serverfd = Open_clientfd(hostname, portStr); 
    if(end_serverfd < 0){ 
        printf("connection failed\n"); 
        return; 
    } 

    Rio_readinitb(&server_rio, end_serverfd); 
    //write the http header to endserver
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header)); 
    //receive message from end server and send to the client
    size_t n; 
    while((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) { 
        printf("proxy received %d bytes,then send\n", (int)n); 
        Rio_writen(connfd, buf, n); 
    } 
    Close(end_serverfd);
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