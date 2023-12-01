#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

char *strerror(int errno);

char *strcpy(char *dest, const char *src);

char *strncpy(char *dest, const char *src, int count);

char *strcat(char *dest, const char *src);

char *strncat(char *dest, const char *src, int count);

int strcmp(const char *cs, const char *ct);

int strncmp(const char *cs, const char *ct, int count);

char *strchr(const char *s, char c);

char *strrchr(const char *s, char c);

int strspn(const char *cs, const char *ct);

int strcspn(const char *cs, const char *ct);

char *strpbrk(const char *cs, const char *ct);

char *strstr(const char *cs, const char *ct);

int strlen(const char *s);

void *memcpy(void *dest, const void *src, int n);

void *memmove(void *dest, const void *src, int n);

int memcmp(const void *cs, const void *ct, int count);

void *memchr(const void *cs, char c, int count);

void *memset(void *s, char c, int count);
#endif
