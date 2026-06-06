/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

void set_errno(int value){
  unsigned long * ptr = (unsigned long *)0x003fe000;
  *ptr = value;
}

int get_errno(){
  unsigned long * ptr = (unsigned long *)0x003fe000;
  return *ptr;
}

int REGS[7]; // Space to save REGISTERS

void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}

void perror()
{
  char buffer[256];
  unsigned long * ptr = (unsigned long *)0x003fe000;
  itoa(*ptr, buffer);
  write(1, buffer, strlen(buffer));
  write(1, "\n", 1);
}

void pthread_wrapper(void * (* func)(void *), void *param){
  func(param);
  ThreadExit();
}

void kevent_exit(void);

void kevent_wrapper(void * (* func)(char key, int pressed), char key, int pressed){
  func(key, pressed);
  kevent_exit();
}
