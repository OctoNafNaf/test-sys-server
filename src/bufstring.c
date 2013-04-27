#include <string.h>
#include <stdlib.h>

#include "bufstring.h"

string string_start()
{
    string s;
    s.str_size = 0;
    s.buf_size = 16;
    s.buffer = (char *) malloc(s.buf_size);
    return s;
}

void string_extend(string *s)
{
    if (s->str_size == s->buf_size) {
        s->buf_size *= 2;
        s->buffer = realloc(s->buffer, s->buf_size * sizeof(char));
    }
}

void string_add_char(string *s, char c)
{
    s->buffer[s->str_size++] = c;
    string_extend(s);
}

char *string_finish(string *s, int *len)
{
    if (len)
        *len = s->str_size;
    string_add_char(s, '\0');
    return realloc(s->buffer, s->buf_size * sizeof(char));
}


char *concat(const char *a, const char *b)
{
    size_t nlen = strlen(a) + strlen(b) + 1;
    char *buf, *cbuf;
    cbuf = buf = (char *) malloc(nlen);
    cbuf = stpcpy(cbuf, a);
    cbuf = stpcpy(cbuf, b);
    return buf;
}
