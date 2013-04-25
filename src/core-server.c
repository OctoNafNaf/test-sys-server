#define _GNU_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>

#include <bufstring.h>

#define BUFSIZE 4096

enum http_method
{
    GET,
    POST,
    UNKNOWN,
};

char *http_method_name[] = 
{
    [GET]       "GET",
    [POST]      "POST",
    [UNKNOWN]   "",
};

typedef struct http_header http_header;

struct http_header
{
    int content_length, method;
};

int validpath(char *path)
{
    if (*path != '/')
        return 0;
    int up = 0, down = 0;
    if (strstr(path, "//"))
        return 0;
    char *i = path + 1;
    while(*i) {
        down += (*i == '/');
        if (i != path && *(i + 1))
            if (*i == '.' && *(i + 1) == '.')
                if (*(i - 1) == '/' && (*(i + 2) == '/' || !*(i + 2))) {
                    up++;
                    if (up > down)
                        return 0;
                }
        i++;
    }
    return 1;
}

char *getstr(int sockfd)
{
    char c;
    string s = string_start();
    while(1) {
        int rd = read(sockfd, &c, 1);
        if (!rd || isspace(c))
            break;
        string_add_char(&s, c);
    }
    return string_finish(&s, NULL);
}

char *getheader(int sockfd)
{
    char c, last = -1;
    string s = string_start();
    while(1) {
        int rd = read(sockfd, &c, 1);
        if (!rd || (last == '\r' && c == '\n'))
            break;
        if (c != '\r')
            string_add_char(&s, c);
        last = c;
    }
    return string_finish(&s, NULL);
}

void process_header(char *header, http_header *header_info)
{
    char *colon = strchr(header, ':'), *buf;
    if (colon == NULL)
        return;
    *colon++ = 0;
    if (strcasecmp("Content-Length", header) == 0)
    {
        unsigned sz;
        sscanf(colon, "%d", &sz);
        asprintf(&buf, "%d", sz);
        putenv(concat("CONTENT_LENGTH=", buf));
        free(buf);
    }
}

void readheaders(int sockfd, http_header *header_info)
{
    char *method, *uri, *version;
    method = getstr(sockfd);
    
    if (strcasecmp("GET", method) == 0)
        header_info->method = GET;
    else if (strcasecmp("POST", method) == 0)
        header_info->method = POST;
    else
        header_info->method = UNKNOWN;
    
    uri = getstr(sockfd);
    version = getstr(sockfd);
    
    printf("%s\n%s\n%s\n", method, uri, version);
    
    (void)method;
    (void)uri;
    (void)version;
    
    while(1)
    {
        char *header = getheader(sockfd);
        if (strcmp("", header) == 0)
        {
            free(header);
            break;
        }
        printf("HEADER$ %s\n", header);
        process_header(header, header_info);
        free(header);
    }
}

const char *response = "<html><h1>SURPRISE MOTHERFUCKA</h1></html>\r\n";

void* process_socket(void *ptr)
{
    printf("Reading headers\n");
    int sockfd = *((int *) ptr);
    http_header header_info;
    
    readheaders(sockfd, &header_info);
    printf("Headers successfuly read\n");
    
    shutdown(sockfd, SHUT_RD);
    
    dprintf(sockfd, "HTTP/1.1 200 OK\r\n");
    dprintf(sockfd, "Content-Length: %d\r\n", strlen(response));
    dprintf(sockfd, "Connection: close\r\n");
    dprintf(sockfd, "\r\n");
    dprintf(sockfd, "%s", response);
    
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return NULL;
}

int main(int argc, char *argv[])
{
    //int port = atoi(argv[1]);
    int port = 9000;
    int server, client;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    server = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bind(server, (struct sockaddr *) &address, sizeof(address));
    listen(server, 1024);
    signal(SIGCHLD, SIG_IGN);
    
    struct timeval tv;

    
    
    while (1) {
        client = accept(server, (struct sockaddr *)&address, &addrlen);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
        pthread_t thread;
        if (pthread_create(&thread, NULL, process_socket, &client) != 0)
            printf("Error while creating new thread\n");
    }
    printf("===============DONE=================\n");
    return 0;
}
