#ifndef CORESERVER_H
#define CORESERVER_H

enum http_method
{
    GET,
    POST,
    UNKNOWN,
};

const char *http_method_name[] = 
{
    [GET]       "GET",
    [POST]      "POST",
    [UNKNOWN]   "",
};

typedef struct http_header http_header;
typedef struct header_key_val header_key_val;

struct header_key_val
{
    char *key, *value;
};

struct http_header
{
    int method, count;
    char *uri, *version;
    header_key_val *headers;
};

int validpath(char *path);
char *getstr(int sockfd);
char *getstrline(int sockfd);
char *getstrlinecrlf(int sockfd);
void process_header(char *header, header_key_val *kv);
void readheaders(int sockfd, http_header *header);
char* savepostdata(int sockfd, http_header *header);

#endif
