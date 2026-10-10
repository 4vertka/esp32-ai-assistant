#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H 

#include <stddef.h>

void ai_ask(const char* prompt, char* out, size_t out_size);

void ai_init(void);
void ai_deinit(void);

#endif
