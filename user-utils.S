#include <asm.h>

ENTRY(syscall_sysenter)
	push %ecx
	push %edx
	push $SYSENTER_RETURN
	push %ebp
	mov %esp, %ebp
	sysenter
ENTRY(SYSENTER_RETURN)
	pop %ebp
	pop %edx
	pop %edx
	pop %ecx
	ret

/* int write(int fd, char *buffer, int size) */
ENTRY(write)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $4, %eax
	movl 0x8(%ebp), %ebx;	//fd
	movl 0xC(%ebp), %ecx;	//buffer
	movl 0x10(%ebp), %edx;	//size
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret
	
/* int ThreadCreate(void (*function)(void* arg), void* parameter) */
ENTRY(ThreadCreate)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $7, %eax
	movl 0x8(%ebp), %ebx;	//function
	movl 0xC(%ebp), %ecx;	//parameter
	movl $pthread_wrapper, %edx //pthread_wrapper address will now be in the system stack, and sys_thread_create will use it accordingly
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret
	
ENTRY(ThreadExit)
	pushl %ebp
	movl %esp, %ebp
	movl $8, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int KeyboardEvent(void (*func)(char key, int pressed)) */
ENTRY(KeyboardEvent)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $9, %eax
	movl 0x8(%ebp), %ebx;	//func
	movl $kevent_wrapper, %ecx //kevent_wrapper address will now be in the system stack, and sys_keyboard_event will use it accordingly
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

/* int WaitForTick() */	
ENTRY(WaitForTick)
	pushl %ebp
	movl %esp, %ebp
	movl $11, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* Common code for negative return */
nok:
	neg %eax
	pushl %eax
	call set_errno
	popl %eax
	mov $-1, %eax
	popl %ebp
	ret

/* int gettime() */
ENTRY(gettime)
	pushl %ebp
	movl %esp, %ebp
	movl $10, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int getpid() */
ENTRY(getpid)
	pushl %ebp
	movl %esp, %ebp
	movl $20, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int fork() */
ENTRY(fork)
	pushl %ebp
	movl %esp, %ebp
	movl $2, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* void exit() */
ENTRY(exit)
	pushl %ebp
	movl %esp, %ebp
	movl $1, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int yield() */
ENTRY(yield)
	pushl %ebp
	movl %esp, %ebp
	movl $13, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int get_stats(int pid, struct stats *st) */
ENTRY(get_stats)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $35, %eax
	movl 0x8(%ebp), %ebx;	//pid
	movl 0xC(%ebp), %ecx;	//st
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

ENTRY(stack_growth)
	pushl %ebp
	movl %esp, %ebp
	subl $0x1000, %esp
	call fork
	leave
	ret

ENTRY(interrupt_test)
	int $0x2b
	ret

ENTRY(kevent_exit)
	int $0x2b
