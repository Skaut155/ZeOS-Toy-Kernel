/*
 * libc.h - macros per fer els traps amb diferents arguments
 *          definició de les crides a sistema
 */
 
#ifndef __LIBC_H__
#define __LIBC_H__

#include <stats.h>

int get_errno();

void set_errno(int value);

int KeyboardEvent(void (*func)(char key, int pressed));

void pthread_wrapper(void * (* func)(void *), void *param);

void ThreadExit(void);

int ThreadCreate(void (*function)(void * arg), void* parameter);

int write(int fd, char *buffer, int size);

int gettime();

int WaitForTick();

void itoa(int a, char *b);

int strlen(char *a);

void perror();

int getpid();

int fork();

void exit();

int yield();

int get_stats(int pid, struct stats *st);

#endif  /* __LIBC_H__ */
