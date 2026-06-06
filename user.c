#include <libc.h>

char buff[24];

int pid;

void write_baka_baka(void * arg){
  write (1, arg, 10); 
}

void write_fork(void * arg){
  int times = *(int *)arg;
  char * text = "Baka Baka\n";
  int pid = fork();
  if (pid == 0){
    ThreadCreate(write_baka_baka, text);
    for (int i = 0; i < times; ++i){
      write (1, "I am the child\n", 15); 
    }
  }
  else write(1, "Hello\n", 6);
}

void race(void * arg){
   write(1, arg, strlen(arg));
   exit();
}

// Function written in ASM that grows the user stack by one page and calls fork
void stack_growth();
void interrupt_test();

void test_errno(void * arg){
  write(1, arg, -1);
  perror();
}

int sum=0;

// this function adds the value of errno (should be 115 because of the write) to sum if a key was pressed 
void test_kevent(char key, int pressed){
  write(1, "Barkus\n", 7);
  if (pressed){
    sum+=get_errno();
  }
}

void do_nothing(void * arg){
  write(1, arg, 3);
  while(1){
  
  }
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  // The following tests check all implemented functionalities. This main function can be used to write any desired code, and it will be executed in this toy kernel in user mode.

  /*// TEST errno | Another thread is created, each one of them will print his own errno
  char * test = "Test\n";
  ThreadCreate(test_errno, test);
  write(-1, test, 1);
  perror();
  // TEST ThreadCreate + fork | "Barkus" should be printed one time, "Hello" should be printed one time, "I am the child" should be printed 2 times and "Baka Baka" should be printed 1 time.
  int param[] = {2};
  ThreadCreate(write_fork, &param);
  write(1, "Barkus\n", 7);
  // TEST exit() | Only one "Got here!" should be printed
  char * text = "Got here!\n";
  int pid = fork();
  if (pid == 0){
    ThreadCreate(race, text);
    ThreadCreate(race, text);
  }
  // TEST Dynamic growth of the user stack | "Stack growth ok" will be printed 2 times, or 4 if the previous test is active (since the new process will reaach this line before the two threads
  // reach the exit syscall.
  stack_growth();
  write(1, "Stack growth ok\n", 16);*/
  
  // ---------------------------------------------------------------------------------------------------------------
  /*char * foo = "hi\n";
  // A couple of threads to have a "real" scenario to test
  ThreadCreate(do_nothing, foo);
  ThreadCreate(do_nothing, foo);
  
  // Test KeyboardEvent | This should do what the function's comment says it does
  KeyboardEvent(test_kevent);
  
  // Test int 0x2b outside keyboard support | This should do nothing
  interrupt_test();
  int n = 0;
  char buffer[16];
  while(1) {
    ++n;
    if (n == 10000000){
    	n = 0;
  	itoa(sum, buffer);
  	write(1, buffer, strlen(buffer));
  	write(1, "\n", 1);
    }
  }*/
  
  // ---------------------------------------------------------------------------------------------------------------
  // Test screen support | With only one thread and one process, we test accessing the screen buffer. A 30 fps minimum should be achieved
  /*char frame[80*25*2];
  for (int i = 0; i < 80; ++i){
    for (int j = 0; j < 50; j+=2){
       frame[i*50 + j+1] = 'A'+i;
       frame[i*50 + j] = 0x00 + i;
    }
  }
  
  int iterations=0;
  int start = gettime();
  while(gettime() < (start + 18)) {
   write(10, frame, 80*25*2);
   ++iterations;
  }
  write(1, "Current FPS: ", 14);
  char buffer[16];
  itoa(iterations, buffer);
  write(1, buffer, strlen(buffer));*/
  
  // ----------------------------------------------------------------------------------------------------------------
  // Test WaitForTick | Both messages will be printed, but the child will go first
  /*int pid = fork();
  if (pid != 0){
    WaitForTick();
    write(1, "Parent\n", 7);
  }
  else write(1, "Child\n", 6);*/
  
  // Infinite user mode loop
  while(1){
  
  }
}
