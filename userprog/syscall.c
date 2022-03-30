#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define maxPosibleArgs 3

#define ERROR -1
#define NOT_LOADED 0
#define LOADED 1
#define LOAD_FAIL 2
#define CLOSE_ALL_FD -1
#define USER_VADDR_BOTTOM ((void *) 0x08048000)


/*
int
getpage_ptr(const void *vaddr)
{
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
  {
    exit(0);
  }
  return (int)ptr;
}

void
validate_ptr (const void *vaddr)
{
    if (!is_user_vaddr(vaddr))
    {
      // virtual memory address is not reserved for us (out of bound)
      exit(0);
    }
}

void
get_args (struct intr_frame *f, int *args, int num_of_args)
{
  int i;
  int *ptr;
  for (i = 0; i < num_of_args; i++)
  {
    ptr = (int *) f->esp + i + 1;
    validate_ptr((const void *) ptr);
    args[i] = *ptr;
  }
}*/

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* 
  A la hora de hacer los ifs colocar SYS_"Nombre de la syscall"
  Esto se puede ver en syscall-nr.h.
  Recordar que las definiciones de las definiciones de las syscalls
  se encuentran en syscall.h.
*/

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int arg[maxPosibleArgs];
  //int esp = getpage_ptr((const void *) f->esp);

  exit(57);
  //if (esp == SYS_HALT) halt();
  //if (esp == SYS_EXIT) {/*get_args(f, &arg[0], 1);*/ exit(arg[0]);}
  /*printf ("system call!\n");
  thread_exit ();*/
}

void halt(void) {
  shutdown_power_off();
}

void exit(int num) {
  printf("%s: exit(%d)\n", thread_current()->name, num);
  thread_exit();
}

pid_t exec (const char *file) {

}

int wait (pid_t) {

}

bool create (const char *file, unsigned initial_size) {

}

bool remove (const char *file) {

}

int open (const char *file) {

}

int filesize (int fd) {

}

int read (int fd, void *buffer, unsigned length) {

}

int write (int fd, const void *buffer, unsigned length) {

}

void seek (int fd, unsigned position) {

}

unsigned tell (int fd) {

}

void close (int fd) {

}