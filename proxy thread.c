/*
 * My student port is 2542
 * Tiny port is 6235
 * fix getaddrinfo and rio_read
 * P locks (decrements) V unlocks (increments)
 volatile int cnt = 0;
 */

#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct sockaddr SA;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

int relay_request(char *host, char *method, char *filename, rio_t *connrio, int clientfd);
int relay_response(rio_t *clientrio, int connfd);
void parse_uri(char *uri, char *host, char *port, char *filename);
int parse_requestline(rio_t *connrio, char *method, char *uri, char *version);
void *relay(void *fd);

int main(int argc, char **argv)
{
  int listenfd, *connfd;
  pthread_t tid;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  /* semaphor that protects connfd */
  sem_t connex;
  /* semaphor that protects clientfd */
  sem_t clientex;
  struct sockaddr_storage clientaddr;
  
  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  
  listenfd = Open_listenfd(argv[1]);
  /* ignore broken pipe signals */
  Signal(SIGPIPE, SIG_IGN);
  Sem_init(&connex, 0, 1);
  Sem_init(&clientex, 0, 1);

  /* stay open indefinitely */
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
                port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    Pthread_create(&tid, NULL, relay, connfd);
  }
}
/* $end tinymain */

/*
 * relay - Reads from connfd and relays the HTTP request to server,
 * then relays the HTTP response to client by writing to connfd.
 */
void *relay(void *fd)
{
  /* deal with threading */
  Pthread_detach(pthread_self());
  int connfd = *((int *)fd);
  Free(fd);

  int clientfd;
  /* rio associated with clientfd to target server */
  rio_t clientrio;
  /* rio associated with the connfd to original client */
  rio_t connrio;
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], filename[MAXLINE];
  /* set default to well-known port */
  strcpy(port, "80");
  
  Rio_readinitb(&connrio, connfd);
  if (parse_requestline(&connrio, method, uri, version) != 0)
  {
    Close(connfd);
    return NULL;
  }
  parse_uri(uri, host, port, filename);
  
  /* open a connection to the requested host at port */
  printf("Connecting to (%s, %s)...\n", host, port);
  /*** don't exit on failure ***/
  if ((clientfd = open_clientfd(host, port)) < 0)
  {
    printf("Failed connection to (%s, %s)\n", host, port);
    Close(connfd);
    return NULL;
  }
  Rio_readinitb(&clientrio, clientfd);
  printf("Made connection to (%s, %s)\n", host, port);
  if (relay_request(host, method, filename, &connrio, clientfd) != 0)
  {
    printf("Failed to send request\n");
    Close(connfd);
    Close(clientfd);
    return NULL;
  }
  if (relay_response(&clientrio, connfd) != 0)
    printf("Failed to send response\n");
  Close(connfd);
  Close(clientfd);
  printf("\nTransaction complete. Connection closed\n");
  return NULL;
}

/*
 * relay_request - read HTTP request in connrio and write them to clientrio
 * making changes to request headers as necessary
 */
int relay_request(char *host, char *method, char *filename, rio_t *connrio, int clientfd)
{
  char request[MAXLINE], buf[MAXLINE];
  /* host is specified in header */
  int host_specified = 0;
  
  /* empty out request buffer */
  strcpy(request, "");
  /* enter our own request line */
  sprintf(request, "%s %s HTTP/1.0\r\n", method, filename);
  
  /* read request headers */
  while(strcmp(buf, "\r\n"))
  {
    if (rio_readlineb(connrio, buf, MAXLINE) < 0)
      return -1;
    
    if (strstr(buf, "Host:"))
      host_specified = 1;
    
    /* ignore these headers. We're using our own */
    if (!strstr(buf, "User-Agent:") &&
        !strstr(buf, "Connection:") &&
        !strstr(buf, "Proxy-Connection:") &&
        strcmp(buf, "\r\n"))
      strcat(request, buf);
  }
  if (!host_specified)
  {
    sprintf(buf, "Host: %s\r\n", host);
    strcat(request, buf);
  }
  strcat(request, user_agent_hdr);
  strcat(request, connection_hdr);
  strcat(request, proxy_connection_hdr);
  strcat(request, "\r\n");
  if (rio_writen(clientfd, request, strlen(request)) < 0)
    return -1;
  printf("Sent request:\n%s", request);
  return 0;
}

int relay_response(rio_t *clientrio, int connfd)
{
  char buf[MAXLINE], response[MAXLINE];
  ssize_t size = 1;
  
  /* empty out response buffer */
  strcpy(response, "");
  /* read response line and headers */
  while (strcmp(buf, "\r\n"))
  {
    if (rio_readlineb(clientrio, buf, MAXLINE) < 0)
      return -1;
    strcat(response, buf);
  }
  /* send response line and headers */
  Rio_writen(connfd, response, strlen(response));
  printf("Sending reponse:\n%s", response);
  
  /* read and send response body */
  while (size > 0)
  {
    if ((size = Rio_readnb(clientrio, buf, MAXLINE)) < 0)
      return -1;
    if (rio_writen(connfd, buf, (size_t) size) < 0)
      return -1;
    printf("%s", buf);
  }
  return 0;
}

/*
 * parse_uri - parses the uri and divides it into <host>, <port>,
 * and <filename>. Port is optional and defults to 80.
 */
void parse_uri(char *uri, char *host, char *port, char *filename)
{
  char protocol[MAXLINE];
  /* cut out "http://" */
  sscanf(uri, "%[^:]://%s", protocol, uri);
  
  if (index(uri, ':')) /* port is specified */
  {
    if (index(uri, '/')) /* filename specified */
    {
      sscanf(uri, "%[^:]:%[^/]%s", host, port, filename);
    }
    else
    {
      sscanf(uri, "%[^:]:%[^/]", host, port);
      strcpy(filename, "/");
    }
  }
  else
  {
    if (index(uri, '/'))
    {
      sscanf(uri, "%[^/]%s", host, filename);
    }
    else
    {
      sscanf(uri, "%[^/]", host);
      strcpy(filename, "/");
    }
  }
  printf("Parsed '%s' into '%s' '%s' '%s'\n", uri, host, port, filename);
}

/*
 * parse_requestline - parses the requestline in connrio and divides it into
 * <method>, <uri>, and <version>. The next Rio_readline will give the
 * first request header.
 */
int parse_requestline(rio_t *connrio, char *method, char *uri, char *version)
{
  char buf[MAXLINE];
  if (!Rio_readlineb(connrio, buf, MAXLINE))
  {
    printf("Request was empty\n");
    return -1;
  }
  printf("Original request: %s\n", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET"))
  {
    printf("Proxy does not implement %s\n", method);
    return -1;
  }
  return 0;
}