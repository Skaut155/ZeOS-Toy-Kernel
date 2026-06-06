/*
 * interrupt.c -
 */
#include <types.h>
#include <utils.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <mm.h>

#include <sched.h>

#include <zeos_interrupt.h>

#define AUX_STACK_PAGE 1000

Gate idt[IDT_ENTRIES];
Register    idtR;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','¡','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','ñ',
  '\0','º','\0','ç','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

char hexa[] =
{
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

int zeos_ticks = 0;

void pf_routine(unsigned long error, unsigned long eip)
{
  /* Allocate another page for the user stack if that is what caused the exception */
  union task_union *tu = (union task_union *) current();
  unsigned long esp = (unsigned long) tu->stack[KERNEL_STACK_SIZE - 2];
  unsigned long pl = esp >> 12;
  if ((esp & 0x00000fff) == 0) --pl; // Check if esp is pointing exactly at the top of the current page and adjustes pl.
  int page_near = 0;
  page_table_entry * PT = get_PT(current());
  for (int i = 1; i < 4; ++i){
     // Check if esp is in the user stack zone and has a mapped logical page near it.
     if ((esp >= (PAG_LOG_INIT_DATA + NUM_PAG_DATA)) && (get_frame(PT, pl + i) != 0) && (get_frame(PT, pl) == 0)) page_near = 1;
  }
  if (!page_near) {
    printk("Process generates a PAGE FAULT exception at EIP: 0x");
    unsigned long bits = eip;
    for (int i = 0; i < 8; ++i)
    {
      printc(hexa[bits >> 28]);
      bits = bits << 4;
    }
    while(1) {  }
  }
  else{
    int pf = alloc_frame();
    set_ss_pag(get_PT(current()), pl, pf);
    set_cr3(get_DIR(current()));
    tu->task.user_stack_pages++;
  }
}

extern struct list_head waitingtick;

void clock_routine()
{
  zeos_show_clock();
  zeos_ticks ++;
  while (!list_empty(&waitingtick)){
    struct list_head * l = list_first(&waitingtick);
    list_del(l);
    list_add_tail(l, &readyqueue);
  }
  schedule();
}

extern int eventProgrammed;
extern void * function;
extern unsigned long * kevent_wrapper;

void keyboard_routine()
{
  unsigned char b = inb(0x60);
  if (eventProgrammed){
    int pressed = 0;
    if (!(b >> 7)) pressed = 1;
    b = b & 0x7f;
    
    union task_union * tu = (union task_union *) current();
    
    int pf = alloc_frame(); // Página física para el stack auxiliar
    tu->task.user_stack_pages++;
    set_ss_pag(get_PT(current()), AUX_STACK_PAGE, pf); // Página lógica 1000 nos servirá como user stack auxiliar
    
    // Guardamos el contexto del thread
    copy_data(&(tu->stack[KERNEL_STACK_SIZE-16]), tu->task.ctx, sizeof(unsigned long) * 16);
    
    // Preparamos las pilas
    unsigned long * ptr = (unsigned long *) ((AUX_STACK_PAGE+1) << 12);
    *(ptr - 1) = (unsigned long) pressed;
    *(ptr - 2) = (unsigned long) b;
    *(ptr - 3) = (unsigned long) function;
    tu->stack[KERNEL_STACK_SIZE-2]=((AUX_STACK_PAGE+1)<<12)-16; // 3 parámetros + 1 fake return address
    tu->stack[KERNEL_STACK_SIZE-5] = (unsigned long) kevent_wrapper;
  }
}

void exit_kevent_routine()
{
  union task_union * tu = (union task_union *) current();
  page_table_entry * PT = get_PT(current());
  unsigned long user_esp = tu->stack[KERNEL_STACK_SIZE-2];
  int stack_page = user_esp >> 12;
  
  // Liberamos las páginas que está utilizando el kevent y cambiamos la pila de usuario
  while(get_frame(PT, stack_page)){
    free_frame(get_frame(PT, stack_page));
    del_ss_pag(PT, stack_page);
    stack_page--;
    tu->task.user_stack_pages--;
  }
  
  // Restauramos el contexto anterior
  copy_data(tu->task.ctx, &(tu->stack[KERNEL_STACK_SIZE-16]), sizeof(unsigned long) * 16);
}

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void pf_handler();
void clock_handler();
void keyboard_handler();
void system_call_handler();
void exit_kevent_handler();

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (unsigned long)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(14, pf_handler, 0);
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);
  setInterruptHandler(0x2b, exit_kevent_handler, 3);

  setSysenter();

  set_idt_reg(&idtR);
}

