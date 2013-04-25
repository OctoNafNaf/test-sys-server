#ifndef BUFSTRING_H
#define BUFSTRING_H

typedef struct string string;

struct string
{
    char *buffer;
    int str_size, buf_size;
};

string string_start();
void string_extend(string *s);
void string_add_char(string *s, char c);
char *string_finish(string *s, int *len);
char *concat(const char *a, const char *b);

#endif
