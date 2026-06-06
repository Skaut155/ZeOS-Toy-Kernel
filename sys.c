/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#include <interrupt.h>

#define LECTURA 0
#define ESCRIPTURA 1

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1 && fd!=10) return -EBADF; 
  if (permissions!=ESCRIPTURA) return -EACCES; 
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
	return -ENOSYS; 
}

int sys_getpid()
{
	return current()->PID;
}

int global_PID=1000;
int global_TID=1;

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  union task_union *uparent = (union task_union *) current();
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));
  
  /* new pages dir */
  allocate_DIR((struct task_struct*)uchild);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-pag-1 as temp logical pages to map to */
  int offset = NUM_PAG_KERNEL + NUM_PAG_CODE;
  int temp = get_frame(parent_PT, TOTAL_PAGES - 1);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. We modify it so it does not mess with our threads*/
    set_ss_pag(parent_PT, TOTAL_PAGES - 1, get_frame(process_PT, pag+offset));
    copy_data((void*)((offset+pag)<<12), (void*)((TOTAL_PAGES-1)<<12), PAGE_SIZE);
    set_cr3(get_DIR(current()));
  }
  if (temp) set_ss_pag(parent_PT, TOTAL_PAGES - 1, temp);
  else del_ss_pag(parent_PT, TOTAL_PAGES - 1);
  
  set_cr3(get_DIR(current()));
  
  /* We copy user stack to the child */
  int j = 0;
  int iter = uparent->task.user_stack_pages;
  int pl = (uparent->stack[KERNEL_STACK_SIZE - 2] >> 12) + (iter - 1);
  for (int i = 0; i < iter; ++i){
    int pf = alloc_frame();
    set_ss_pag(process_PT, pl, pf);
    int temp = get_frame(parent_PT, TOTAL_PAGES - j - 1); 
    set_ss_pag(parent_PT, TOTAL_PAGES - j - 1, pf);
    copy_data((void*)(pl<<12), (void*)((TOTAL_PAGES - j - 1)<<12), PAGE_SIZE);
    if (temp) set_ss_pag(parent_PT, TOTAL_PAGES - j - 1, temp);
    else del_ss_pag(parent_PT, TOTAL_PAGES - j - 1);
    --pl;
    ++j;
  }
  
  // We set a page for the TLS (currently just errno)
  int pf = alloc_frame();
  uchild->task.errno_pf = pf;
  
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID=++global_PID;
  uchild->task.TID=++global_TID;  /* The new process' thread will have his very own TID */
  uchild->task.state=ST_READY;

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->task.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=(DWord)&ret_from_fork;
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);
  
  return uchild->task.PID;
}

int sys_thread_create(void (*function)(void* arg), void* parameter){
  struct list_head *lhcurrent = NULL;
  union task_union *unew;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  unew=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), unew, sizeof(union task_union));
  
  /* new pages dir */
  unew->task.dir_pages_baseAddr = current()->dir_pages_baseAddr;
  
  // user stack
  page_table_entry * PT = get_PT(current());
  int i = PAG_LOG_INIT_DATA+NUM_PAG_DATA;
  int total = 0;
  while (i < TOTAL_PAGES){
     if (get_frame(PT, i) == 0) ++total;
     else total = 0;
     
     if (total == 4){
     	set_ss_pag(PT, i, alloc_frame());
     	++i;
     	break;
     }
     ++i;
  }
  
  unew->task.TID=++global_TID;  /* The new thread will have his very own TID */
  unew->task.state=ST_READY;
  
  unsigned long * ptr = (unsigned long *) (i << 12);
  /* Preparing first thread execution */
  *(ptr - 1) = (unsigned long) parameter;
  *(ptr - 2) = (unsigned long) function;
  unew->stack[KERNEL_STACK_SIZE-2] = (unsigned long) (ptr - 3);
  unew->stack[KERNEL_STACK_SIZE-5] = unew->stack[KERNEL_STACK_SIZE-14];
  unew->task.register_esp = (int) &(unew->stack[KERNEL_STACK_SIZE-18]);

  /* Set stats to 0 */
  init_stats(&(unew->task.p_stats));

  /* Queue new process into readyqueue */
  unew->task.state=ST_READY;
  list_add_tail(&(unew->task.list), &readyqueue);
  
  // We set a page for the TLS (currently just errno)
  int pf = alloc_frame();
  unew->task.errno_pf = pf;
  
  return unew->task.TID;
}

void exit_thread(union task_union * tu)
{
  page_table_entry *process_PT = get_PT(&(tu->task));

  // Free thread's user stack (it's top can be found in the hardware context of the thread)
  int pl = tu->stack[KERNEL_STACK_SIZE - 2] >> 12;
  int iter = tu->task.user_stack_pages;
  for (int i = 0; i < iter; ++i){
     free_frame(get_frame(process_PT, pl));
     del_ss_pag(process_PT, pl);
     ++pl;
  }
  
  if (&(tu->task) != current()) list_del(&(tu->task.list));  // If the thread is not running, delete it from whatever queue it may be in
  list_add_tail(&(tu->task.list), &freequeue);
  
  if (&(tu->task) != current()){
    tu->task.PID = -1;
    tu->task.TID = -1;
  }
}

void sys_thread_exit(void){
  struct task_struct * TCB = current();
  union task_union * tu = (union task_union *) TCB;
  page_table_entry * PT = get_PT(TCB);
  
  int pl = tu->stack[KERNEL_STACK_SIZE - 2] >> 12;
  int iter = TCB->user_stack_pages;
  for (int i = 0; i < iter; ++i){
     free_frame(get_frame(PT, pl));
     del_ss_pag(PT, pl);
     ++pl;
  }
  
  // Unmap PT pages if it is the last thread of the process
  int unmap = 1;
  for (int i = 0; i < NR_TASKS; ++i){
    if ((task[i].task.PID == current()->PID) && (task[i].task.TID != current()->TID)){
      unmap = 0;
      break;
    }
  }
  if (unmap){
    int offset = NUM_PAG_KERNEL;
    for (int i = 0; i < NUM_PAG_DATA; ++i){
      free_frame(get_frame(PT, i + offset));
      del_ss_pag(PT, i + offset);
    }
  }
  
  TCB->PID = -1;
  TCB->TID = -1;
  TCB->dir_pages_baseAddr = NULL;
  update_process_state_rr(TCB, &freequeue);
  sched_next_rr();
}

int sys_wait_for_tick()
{
  struct task_struct * t = current();
  list_add_tail(&(t->list), &waitingtick);
  sched_next_rr();
  return 0;
}

void sys_exit()
{  
  // Exit all threads belonging to this process
  for (int i = 0; i < NR_TASKS; ++i){
    if (task[i].task.PID == current()->PID){
      exit_thread((union task_union *)&task[i].task);
    }
  }
  // Free thread's user data
  int offset = NUM_PAG_KERNEL;
  for (int i = 0; i < NUM_PAG_DATA; ++i){
    free_frame(get_frame(get_PT(current()), i + offset));
    del_ss_pag(get_PT(current()), i + offset);
  }
  current()->PID = -1;
  current()->TID = -1;
  /* Restarts execution of the next process */
  sched_next_rr();
}

int eventProgrammed = 0;
void * function = NULL;
unsigned long * kevent_wrapper = NULL;

int sys_keyboard_event(void (*func)(char key, int pressed), unsigned long * wrapper){
  eventProgrammed = 1;
  function = func;
  kevent_wrapper = wrapper;
  return 0;
}

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes) {
int bytes_left;
int ret;

	if ((ret = check_fd(fd, ESCRIPTURA)))
		return ret;
	if (nbytes < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_READ, buffer, nbytes))
		return -EFAULT;
	
	switch (fd)
	{
	  case 1:
	     bytes_left = nbytes;
	     char localbuffer [TAM_BUFFER];
	     while (bytes_left > TAM_BUFFER) {
		copy_from_user(buffer, localbuffer, TAM_BUFFER);
		ret = sys_write_console(localbuffer, TAM_BUFFER);
		bytes_left-=ret;
		buffer+=ret;
	     }
	     if (bytes_left > 0) {
		copy_from_user(buffer, localbuffer,bytes_left);
		ret = sys_write_console(localbuffer, bytes_left);
		bytes_left-=ret;
	     }
	     return (nbytes-bytes_left);
	    
	  case 10:
	     if (nbytes != (80*25*2)) return -EINVAL;
	     char tempbuffer [nbytes];
	     copy_from_user(buffer, tempbuffer, nbytes);
	     for (int i = 0; i < 80; ++i){
	     	for (int j = 0; j < 25; ++j){
	     	   printc_xy((Byte) i, (Byte) j, tempbuffer[i*50+j*2+1], tempbuffer[i*50 + j*2]);
	     	}
	     }
	}
	return 0;
}


extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

/* System call to force a task switch */
int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (pid<0) return -EINVAL;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}
