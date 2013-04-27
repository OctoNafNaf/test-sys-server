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
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#include "bufstring.h"
#include "core-server.h"

#define MAX_HEADERS 50
#define BUFSIZE 4096

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
    char c = -1;
    string s = string_start();
    while(1) {
        int rd = read(sockfd, &c, 1);
        if (!rd || isspace(c))
            break;
        string_add_char(&s, c);
    }
    if (c == '\r')
        read(sockfd, &c, 1);
    return string_finish(&s, NULL);
}

char *getstrline(int sockfd)
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

char *getstrlinecrlf(int sockfd)
{
    char c;
    string s = string_start();
    while(1) {
        int rd = read(sockfd, &c, 1);
        if (!rd)
            break;
        string_add_char(&s, c);
        if (c == '\n')
            break;
    }
    return string_finish(&s, NULL);
}

void process_header(char *header, header_key_val *kv)
{
    char *colon = strchr(header, ':');
    if (colon == NULL)
        return;
    *colon++ = 0;
    kv->key = strdup(header);
    kv->value = strdup(colon + 1);
}

char* getboundary(http_header *header)
{
    int i;
    char *boundary = NULL;
    for(i = 0; i < header->count; i++)
        if (strcasecmp("Content-Type", header->headers[i].key) == 0)
        {
            char *pos = strcasestr(header->headers[i].value, "boundary");
            sscanf(pos + 9, "%m[^ ]", &boundary);
            break;
        }
    return boundary;
}

void readheaders(int sockfd, http_header *header)
{
    char *method = getstr(sockfd);
    
    if (strcasecmp("GET", method) == 0)
        header->method = GET;
    else if (strcasecmp("POST", method) == 0)
        header->method = POST;
    else
        header->method = UNKNOWN;
        
    header->uri = getstr(sockfd);
    header->version = getstr(sockfd);
    
    free(method);
    
    header->count = 0;
    header->headers = malloc(MAX_HEADERS * sizeof(header_key_val));
    while(1)
    {
        if (header->count == MAX_HEADERS)
        {
            break;
            printf("TOO MANY HEADERS");
        }
        char *s = getstrline(sockfd);
        if (strcmp("", s) == 0)
        {
            free(s);
            break;
        }
        process_header(s, &header->headers[header->count++]);
        free(s);
    }
}

// TODO free(fname)
char* savepostdata(int sockfd, http_header *header)
{
    char *boundary = getboundary(header);
    char *tmp = getstrlinecrlf(sockfd);
    char *filename;
    header_key_val cur_header;
    while(1)
    {
        tmp = getstrline(sockfd);
        if (strcmp("", tmp) == 0)
        {
            free(tmp);
            break;
        }
        process_header(tmp, &cur_header);
        if (strcasecmp("Content-Disposition", cur_header.key) == 0)
        {
            char *fname = strcasestr(cur_header.value, "filename");
            sscanf(fname + 10, "%m[^\"]", &filename);
        }
        free(tmp);
        free(cur_header.key);
        free(cur_header.value);
    }
    int c = mkdir("solutions", 0777);
    if (c == 0 || (c == -1 && errno == EEXIST))
    {
        char *dir = concat("solutions/", filename);
        FILE *file = fopen(dir, "w");
        int flag = 1;
        while(flag)
        {
            tmp = getstrlinecrlf(sockfd);
            if (strstr(tmp, boundary))
                flag = 0;
            else
                fwrite(tmp, sizeof(char), strlen(tmp), file);
            free(tmp);
        }
        fclose(file);
        struct stat st;
        stat(dir, &st);
        truncate(dir, st.st_size - 2);
        free(dir);
    }
    return filename;
}

const char *response = "<html><h1>SURPRISE MOTHERFUCKA</h1></html>\r\n";

void send_static_html(int sockfd, char *filepath)
{

    FILE *page = fopen(filepath, "r");
    if (!page)
    {
        dprintf(sockfd, "HTTP/1.1 404 NOT FOUND\r\n");
        dprintf(sockfd, "Connection: close\r\n");
        dprintf(sockfd, "Content-Type: text/html; charset=utf-8\r\n");
        dprintf(sockfd, "\r\n");
        dprintf(sockfd, "<html><h1>404 - FILE NOT FOUND <br>SURPRISE MOTHERFUCKA</h1></html>\r\n");
        return;
    }
    
    dprintf(sockfd, "HTTP/1.1 200 OK\r\n");
    dprintf(sockfd, "Connection: close\r\n");
    dprintf(sockfd, "Content-Type: text/html; charset=utf-8\r\n");
    dprintf(sockfd, "\r\n");

    char buffer[BUFSIZE];
    while (1) {
        int got = fread(buffer, sizeof(char), BUFSIZE, page);
        write(sockfd, buffer, got);
        if (got < BUFSIZE)
            break;
    }
    fclose(page);
}

void* process_socket(void *ptr)
{
    printf("Reading headers\n");
    int sockfd = *((int *) ptr);
    http_header header_info;
    
    readheaders(sockfd, &header_info);
    
    if (header_info.method == POST)
        savepostdata(sockfd, &header_info);
    
    /*
    int i;
    for(i = 0; i < header_info.count; i++)
        printf("Header [%d]\t\t%s: %s\n", i + 1, header_info.headers[i].key, header_info.headers[i].value);
    */
    printf("Headers successfuly read\n");
    
    shutdown(sockfd, SHUT_RD);
    
    /*
    dprintf(sockfd, "HTTP/1.1 200 OK\r\n");
    dprintf(sockfd, "Content-Length: %d\r\n", strlen(response));
    dprintf(sockfd, "Connection: close\r\n");
    dprintf(sockfd, "\r\n");
    dprintf(sockfd, "%s", response);
    * */
    
    send_static_html(sockfd, "static/file-post-form.html");
    
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
    while (1) {
        client = accept(server, (struct sockaddr *)&address, &addrlen);
        pthread_t thread;
        if (pthread_create(&thread, NULL, process_socket, &client) != 0)
            printf("Error while creating new thread\n");
    }
    printf("===============DONE=================\n");
    return 0;
}
