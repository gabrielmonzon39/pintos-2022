#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"

// para sys
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"

static void syscall_handler (struct intr_frame *);

// approach del lock
struct lock lock_;

void
syscall_init (void)
{
  lock_init (&lock_);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

////////////////////////////     AGREGADO      ////////////////////////////
////////////////////////////   DEFINICIONES    ////////////////////////////
#define byte__ 0xFF
#define error__ -1

static void check_num(int num, struct intr_frame *f);
int not_valid(int num); 

void do_halt(int num, struct intr_frame *f);
void do_exit(int num, struct intr_frame *f);
void do_execute(int num, struct intr_frame *f);
void do_wait(int num, struct intr_frame *f);
void do_create(int num, struct intr_frame *f);
void do_remove(int num, struct intr_frame *f);
void do_open(int num, struct intr_frame *f);
void do_filesize(int num, struct intr_frame *f);
void do_read(int num, struct intr_frame *f);
void do_write(int num, struct intr_frame *f);
void do_seek(int num, struct intr_frame *f);
void do_tell(int num, struct intr_frame *f);
void do_close(int num, struct intr_frame *f);
void do_not_valid(int num, struct intr_frame *f);

void do_exit2(int num);

static void check_user (const uint8_t *uaddr);
static int32_t getU (const uint8_t *uaddr);
static bool putU (uint8_t *udst, uint8_t byte);
static void CopyPaste (void *src, void *dst, size_t bytes);
static struct file_desc* getF(struct thread *, int fd);

////////////////////////////////////////////////////////////////////////////////////


// Cambiando el handler, para mayor orden
static void
syscall_handler (struct intr_frame *f)
{
  int num;
  CopyPaste(f->esp, &num, sizeof(num));
  check_num(num, f);
}

/* 0  - Halt
 * 1  - Exit
 * 2  - Execute
 * 3  - Wait 
 * 4  - Create
 * 5  - Remove
 * 6  - Open
 * 7  - Filesize
 * 8  - Read
 * 9  - Write
 * 10 - Seek
 * 11 - Tell
 * 12 - Close
 * .. - Invalida
*/

static void check_num(int num, struct intr_frame *f) {
  if (num ==  0)     do_halt(num, f);
  if (num ==  1)     do_exit(num, f);
  if (num ==  2)  do_execute(num, f);
  if (num ==  3)     do_wait(num, f);
  if (num ==  4)   do_create(num, f);
  if (num ==  5)   do_remove(num, f);
  if (num ==  6)     do_open(num, f);
  if (num ==  7) do_filesize(num, f);
  if (num ==  8)     do_read(num, f);
  if (num ==  9)    do_write(num, f);
  if (num == 10)     do_seek(num, f);
  if (num == 11)     do_tell(num, f);
  if (num == 12)    do_close(num, f);
  if (not_valid(num)) do_not_valid(num, f);
}



int not_valid(int num) {
  if (num >= 0 && num <= 12) return 0;
  return 1;
}

void do_halt(int num, struct intr_frame *f) {
  shutdown_power_off();
}

void do_exit(int num, struct intr_frame *f) {
  int exCode_num;
  CopyPaste(f->esp + 4, &exCode_num, sizeof(exCode_num));

  printf("%s: exit(%d)\n", thread_current()->name, exCode_num);

  struct process_control_block *pcb = thread_current()->pcb;
  if (pcb == NULL) thread_exit();
  if (pcb != NULL) {
    pcb->exited = true;
    pcb->exitcode = exCode_num;
    thread_exit();
  }
}

void do_exit2 (int num) {
  printf("%s: exit(%d)\n", thread_current()->name, num);
  struct process_control_block *pcb = thread_current()->pcb;
  if (pcb == NULL) thread_exit();
  if (pcb != NULL) {
    pcb->exited = true;
    pcb->exitcode = num;
    thread_exit();
  }
}

void do_execute(int num, struct intr_frame *f) {
  void* var;
  CopyPaste(f->esp + 4, &var, sizeof(var));

  check_user((const uint8_t*) var);
  lock_acquire (&lock_); //
  pid_t pid = process_execute(var);
  lock_release (&lock_);

  f->eax = (uint32_t) pid;
}

void do_wait(int num, struct intr_frame *f) {
  pid_t pid;
  CopyPaste(f->esp + 4, &pid, sizeof(pid_t));
  f->eax = (uint32_t) process_wait(pid);
}

void do_create(int num, struct intr_frame *f) {
  const char* name_fl;
  unsigned size;
  bool x;

  CopyPaste(f->esp + 4, &name_fl, sizeof(name_fl));
  CopyPaste(f->esp + 8, &size, sizeof(size));

  // memory validation
  check_user((const uint8_t*) name_fl);

  lock_acquire (&lock_);
  x = filesys_create(name_fl, size);
  lock_release (&lock_);

  f->eax = x;
}

void do_remove(int num, struct intr_frame *f) {
  shutdown_power_off();
  /*const char* name_fl;
  bool x;

  CopyPaste(f->esp + 4, &name_fl, sizeof(name_fl));

  // memory validation
  check_user((const uint8_t*) name_fl);

  lock_acquire (&lock_);
  x = filesys_remove(name_fl);
  lock_release (&lock_);


  f->eax = x;*/
}

void do_open(int num, struct intr_frame *f) {
  shutdown_power_off();
  /*const char* name_fl;
  int x;

  CopyPaste(f->esp + 4, &name_fl, sizeof(name_fl));

  check_user((const uint8_t*) name_fl);

  struct file* file_opened;
  struct file_desc* fd = palloc_get_page(0);
  if (!fd) {
    return error__;
  }

  lock_acquire (&lock_);
  file_opened = filesys_open(name_fl);
  if (!file_opened) {
    palloc_free_page (fd);
    lock_release (&lock_);
    return error__;
  }

  fd->file = file_opened;

  struct list* fd_list = &thread_current()->file_descriptors;
  if (list_empty(fd_list)) {
    fd->id = 3;
  }
  else {
    fd->id = (list_entry(list_back(fd_list), struct file_desc, elem)->id) + 1;
  }
  list_push_back(fd_list, &(fd->elem));
  // list_insert_ordered (&fd_list, &fd->elem, sort, NULL); 

  lock_release (&lock_);


  f->eax = fd->id;*/
}

void do_filesize(int num, struct intr_frame *f) {
  shutdown_power_off();
  /*int fd, x;
  CopyPaste(f->esp + 4, &fd, sizeof(fd));

  struct file_desc* file_d;

  lock_acquire (&lock_);
  file_d = getF(thread_current(), fd);

  if(file_d == NULL) {
    lock_release (&lock_);
    return error__;
  }

  int ret = file_length(file_d->file);
  lock_release (&lock_);

  f->eax = ret;*/
}

void do_read(int num, struct intr_frame *f) {
  int fd, x;
  void *buffer;
  unsigned size;

  CopyPaste(f->esp + 4, &fd, sizeof(fd));
  CopyPaste(f->esp + 8, &buffer, sizeof(buffer));
  CopyPaste(f->esp + 12, &size, sizeof(size));

  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);

  lock_acquire (&lock_);
  int ret;

  if(fd != 0) {
    struct file_desc* file_d = getF(thread_current(), fd);

    if(file_d && file_d->file) {
      ret = file_read(file_d->file, buffer, size);
    }
    else {
      ret = error__;
    }
  }
  else {
    unsigned i;
    for(i = 0; i < size; ++i) {
      if(! putU(buffer + i, input_getc()) ) {
        lock_release (&lock_);
        do_exit(error__, f); // segfault
      }
    }
    ret = size;
  }

  lock_release (&lock_);

  f->eax = (uint32_t) ret;
}

void do_write(int num, struct intr_frame *f) {
  int fd, x;
  const void *buffer;
  unsigned size;

  CopyPaste(f->esp + 4, &fd, sizeof(fd));
  CopyPaste(f->esp + 8, &buffer, sizeof(buffer));
  CopyPaste(f->esp + 12, &size, sizeof(size));

  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);

  lock_acquire (&lock_);
  int ret;

  if(fd != 1) {
    struct file_desc* file_d = getF(thread_current(), fd);

    if(file_d && file_d->file) {
      ret = file_write(file_d->file, buffer, size);
    }
    else 
      ret = error__;
  }else {
    putbuf(buffer, size);
    ret = size;
  }

  lock_release (&lock_);


  f->eax = (uint32_t) ret;
}

void do_seek(int num, struct intr_frame *f) {
  shutdown_power_off();
  /*int fd;
  unsigned position;
  CopyPaste(f->esp + 4, &fd, sizeof(fd));
  CopyPaste(f->esp + 8, &position, sizeof(position));


  lock_acquire (&lock_);
  struct file_desc* file_d = getF(thread_current(), fd);

  if(file_d && file_d->file) {
    file_seek(file_d->file, position);
  }
  else
    return;

  lock_release (&lock_);*/
}

void do_tell(int num, struct intr_frame *f) {
  /*int fd;
  unsigned x;

  CopyPaste(f->esp + 4, &fd, sizeof(fd));

  lock_acquire (&lock_);
  struct file_desc* file_d = getF(thread_current(), fd);

  unsigned ret;
  if(file_d && file_d->file) {
    ret = file_tell(file_d->file);
  }
  else
    ret = error__;

  lock_release (&lock_);

  f->eax = (uint32_t) ret;*/
}

void do_close(int num, struct intr_frame *f) {
  shutdown_power_off();
  /*int fd;
  CopyPaste(f->esp + 4, &fd, sizeof(fd));

  lock_acquire (&lock_);
  struct file_desc* file_d = getF(thread_current(), fd);

  if(file_d && file_d->file) {
    file_close(file_d->file);
    list_remove(&(file_d->elem));
    palloc_free_page(file_d);
  }
  lock_release (&lock_);*/
}

void do_not_valid(int num, struct intr_frame *f) {
  //printf("[ERROR] system call %d is unimplemented!\n", num);
  do_exit(error__, f);
}

static void errorOnMemory(void) {
  if (lock_held_by_current_thread(&lock_))
    lock_release (&lock_);

  do_exit2 (error__);
}

static void
check_user (const uint8_t *uaddr) {
  if(getU (uaddr) == error__) errorOnMemory();
}

static int32_t
getU (const uint8_t *uaddr) {
  if (((void*)uaddr < PHYS_BASE)) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (result) : "m" (*uaddr));
    return result;
  }
  return error__;
}

static bool
putU (uint8_t *udst, uint8_t byte) {
  if (((void*)udst < PHYS_BASE)) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != error__;
  }
    return false;

}


static void CopyPaste (void *src, void *dst, size_t bytes) {
  int32_t num;
  size_t i;
  for (i = 0; i < bytes; i++) {
    num = getU(src + i);
    if(num == error__) errorOnMemory();
    *(char*)(dst + i) = num & byte__;
  }
}

static struct file_desc*
getF(struct thread *t, int fd)
{
  //ASSERT (t != NULL);

  if (fd < 3) return NULL;

  struct list_elem *e;

  if (list_empty(&t->file_descriptors)) return NULL;
  
  for(struct list_elem *e = list_begin(&t->file_descriptors); e != list_end(&t->file_descriptors); e = list_next(e)) {
    struct file_desc *desc = list_entry(e, struct file_desc, elem);
    if(desc->id == fd) return desc;
  }

}