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


#ifdef DEBUG
#define _DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define _DEBUG_PRINTF(...) /* do nothing */
#endif

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
struct lock filesys_lock;
void
syscall_init (void)
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

<<<<<<< HEAD
////////////////////////////     AGREGADO      ////////////////////////////
////////////////////////////   DEFINICIONES    ////////////////////////////
#define byte__ 0xFF

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
static int load_M (void *src, void *des, size_t bytes);

static struct file_desc* getF(struct thread *, int fd);

// in case of invalid memory access, fail and exit.
static void bad_access(void) {
  if (lock_held_by_current_thread(&filesys_lock))
    lock_release (&filesys_lock);

  do_exit2 (-1);
}

////////////////////////////////////////////////////////////////////////////////////


// Cambiando el handler, para mayor orden
static void
syscall_handler (struct intr_frame *f)
{
  int num;
  load_M(f->esp, &num, sizeof(num));
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
  load_M(f->esp + 4, &exCode_num, sizeof(exCode_num));

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
  load_M(f->esp + 4, &var, sizeof(var));

  check_user((const uint8_t*) var);
  lock_acquire (&filesys_lock); //
  pid_t pid = process_execute(var);
  lock_release (&filesys_lock);

  f->eax = (uint32_t) pid;
}

void do_wait(int num, struct intr_frame *f) {
  pid_t pid;
  load_M(f->esp + 4, &pid, sizeof(pid_t));
  f->eax = (uint32_t) process_wait(pid);
}

void do_create(int num, struct intr_frame *f) {
  const char* name_fl;
  unsigned size;
  bool x;

  load_M(f->esp + 4, &name_fl, sizeof(name_fl));
  load_M(f->esp + 8, &size, sizeof(size));

  // memory validation
  check_user((const uint8_t*) name_fl);

  lock_acquire (&filesys_lock);
  x = filesys_create(name_fl, size);
  lock_release (&filesys_lock);

  f->eax = x;
}

void do_remove(int num, struct intr_frame *f) {
  const char* name_fl;
  bool x;

  load_M(f->esp + 4, &name_fl, sizeof(name_fl));

  // memory validation
  check_user((const uint8_t*) name_fl);

  lock_acquire (&filesys_lock);
  x = filesys_remove(name_fl);
  lock_release (&filesys_lock);


  f->eax = x;
}

void do_open(int num, struct intr_frame *f) {
  const char* name_fl;
  int x;

  load_M(f->esp + 4, &name_fl, sizeof(name_fl));

  check_user((const uint8_t*) name_fl);
=======
// in case of invalid memory access, fail and exit.
static void fail_invalid_access(void) {
  if (lock_held_by_current_thread(&filesys_lock))
    lock_release (&filesys_lock);

  sys_exit (-1);
  NOT_REACHED();
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;

  ASSERT( sizeof(syscall_number) == 4 ); // assuming x86

  // The system call number is in the 32-bit word at the caller's stack pointer.
  memread_user(f->esp, &syscall_number, sizeof(syscall_number));

  _DEBUG_PRINTF ("[DEBUG] system call, number = %d!\n", syscall_number);

  // Dispatch w.r.t system call number
  // SYS_*** constants are defined in syscall-nr.h
  switch (syscall_number) {
  case SYS_HALT: // 0
    {
    fd->id = (list_entry(list_back(fd_list), struct file_desc, elem)->id) + 1;
  }
  list_push_back(fd_list, &(fd->elem));
<<<<<<< HEAD
  // list_insert_ordered (&fd_list, &fd->elem, sort, NULL); 

  lock_release (&filesys_lock);


  f->eax = fd->id;
}

void do_filesize(int num, struct intr_frame *f) {
  int fd, x;
  load_M(f->esp + 4, &fd, sizeof(fd));

  struct file_desc* file_d;

  lock_acquire (&filesys_lock);
  file_d = getF(thread_current(), fd);
=======

  lock_release (&filesys_lock);
  return fd->id;
}

int sys_filesize(int fd) {
  struct file_desc* file_d;

  lock_acquire (&filesys_lock);
  file_d = find_file_desc(thread_current(), fd);
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb

  if(file_d == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }

  int ret = file_length(file_d->file);
  lock_release (&filesys_lock);
<<<<<<< HEAD

  f->eax = ret;
}

void do_read(int num, struct intr_frame *f) {
  int fd, x;
  void *buffer;
  unsigned size;

  load_M(f->esp + 4, &fd, sizeof(fd));
  load_M(f->esp + 8, &buffer, sizeof(buffer));
  load_M(f->esp + 12, &size, sizeof(size));

  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);

  lock_acquire (&filesys_lock);
  int ret;

  if(fd != 0) {
    struct file_desc* file_d = getF(thread_current(), fd);

    if(file_d && file_d->file) {
      ret = file_read(file_d->file, buffer, size);
    }
    else {
      ret = -1;
    }
  }
  else {
    unsigned i;
    for(i = 0; i < size; ++i) {
      if(! putU(buffer + i, input_getc()) ) {
        lock_release (&filesys_lock);
        do_exit(-1, f); // segfault
      }
    }
    ret = size;
  }

  lock_release (&filesys_lock);

  f->eax = (uint32_t) ret;
}

void do_write(int num, struct intr_frame *f) {
  int fd, x;
  const void *buffer;
  unsigned size;

  load_M(f->esp + 4, &fd, sizeof(fd));
  load_M(f->esp + 8, &buffer, sizeof(buffer));
  load_M(f->esp + 12, &size, sizeof(size));

  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);

  lock_acquire (&filesys_lock);
  int ret;

  if(fd != 1) {
    struct file_desc* file_d = getF(thread_current(), fd);

    if(file_d && file_d->file) {
      ret = file_write(file_d->file, buffer, size);
    }
    else 
      ret = -1;
  }else {
    putbuf(buffer, size);
    ret = size;
  }

  lock_release (&filesys_lock);


  f->eax = (uint32_t) ret;
}

void do_seek(int num, struct intr_frame *f) {
  int fd;
  unsigned position;
  load_M(f->esp + 4, &fd, sizeof(fd));
  load_M(f->esp + 8, &position, sizeof(position));


  lock_acquire (&filesys_lock);
  struct file_desc* file_d = getF(thread_current(), fd);
=======
  return ret;
}

void sys_seek(int fd, unsigned position) {
  lock_acquire (&filesys_lock);
  struct file_desc* file_d = find_file_desc(thread_current(), fd);
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb

  if(file_d && file_d->file) {
    file_seek(file_d->file, position);
  }
  else
<<<<<<< HEAD
    return;
=======
    return; // TODO need sys_exit?
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb

  lock_release (&filesys_lock);
}

<<<<<<< HEAD
void do_tell(int num, struct intr_frame *f) {
  int fd;
  unsigned x;

  load_M(f->esp + 4, &fd, sizeof(fd));

  lock_acquire (&filesys_lock);
  struct file_desc* file_d = getF(thread_current(), fd);
=======
unsigned sys_tell(int fd) {
  lock_acquire (&filesys_lock);
  struct file_desc* file_d = find_file_desc(thread_current(), fd);
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb

  unsigned ret;
  if(file_d && file_d->file) {
    ret = file_tell(file_d->file);
  }
  else
<<<<<<< HEAD
    ret = -1;

  lock_release (&filesys_lock);

  f->eax = (uint32_t) ret;
}

void do_close(int num, struct intr_frame *f) {
  int fd;
  load_M(f->esp + 4, &fd, sizeof(fd));

  lock_acquire (&filesys_lock);
  struct file_desc* file_d = getF(thread_current(), fd);
=======
    ret = -1; // TODO need sys_exit?

  lock_release (&filesys_lock);
  return ret;
}

void sys_close(int fd) {
  lock_acquire (&filesys_lock);
  struct file_desc* file_d = find_file_desc(thread_current(), fd);
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb

  if(file_d && file_d->file) {
    file_close(file_d->file);
    list_remove(&(file_d->elem));
    palloc_free_page(file_d);
  }
  lock_release (&filesys_lock);
}

<<<<<<< HEAD
void do_not_valid(int num, struct intr_frame *f) {
  printf("[ERROR] system call %d is unimplemented!\n", num);
  do_exit(-1, f);
}

=======
int sys_read(int fd, void *buffer, unsigned size) {
  // memory validation : [buffer+0, buffer+size) should be all valid
  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);

  lock_acquire (&filesys_lock);
  int ret;

  if(fd == 0) { // stdin
    unsigned i;
    for(i = 0; i < size; ++i) {
      if(! put_user(buffer + i, input_getc()) ) {
        lock_release (&filesys_lock);
        sys_exit(-1); // segfault
      }
    }
    ret = size;
  }
  else {
    // read from file
    struct file_desc* file_d = find_file_desc(thread_current(), fd);

    if(file_d && file_d->file) {
      ret = file_read(file_d->file, buffer, size);
    }
    else // no such file or can't open
      ret = -1;
  }

  lock_release (&filesys_lock);
  return ret;
}

int sys_write(int fd, const void *buffer, unsigned size) {
  // memory validation : [buffer+0, buffer+size) should be all valid
  check_user((const uint8_t*) buffer);
  check_user((const uint8_t*) buffer + size - 1);

  lock_acquire (&filesys_lock);
  int ret;

  if(fd == 1) { // write to stdout
    putbuf(buffer, size);
    ret = size;
  }
  else {
    // write into file
    struct file_desc* file_d = find_file_desc(thread_current(), fd);

    if(file_d && file_d->file) {
      ret = file_write(file_d->file, buffer, size);
    }
    else // no such file or can't open
      ret = -1;
  }

  lock_release (&filesys_lock);
  return ret;
}
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb

/****************** Helper Functions on Memory Access ********************/

static void
check_user (const uint8_t *uaddr) {
<<<<<<< HEAD
  if(getU (uaddr) == -1) bad_access();
=======
  // check uaddr range or segfaults
  if(get_user (uaddr) == -1)
    fail_invalid_access();
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb
}

/**
 * Reads a single 'byte' at user memory admemory at 'uaddr'.
 * 'uaddr' must be below PHYS_BASE.
 *
 * Returns the byte value if successful (extract the least significant byte),
 * or -1 in case of error (a segfault occurred or invalid uaddr)
 */
static int32_t
<<<<<<< HEAD
getU (const uint8_t *uaddr) {
  if (((void*)uaddr < PHYS_BASE)) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (result) : "m" (*uaddr));
    return result;
  }
  return -1;
=======
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if (! ((void*)uaddr < PHYS_BASE)) {
    return -1;
  }

  // as suggested in the reference manual, see (3.1.5)
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb
}

/* Writes a single byte (content is 'byte') to user address 'udst'.
 * 'udst' must be below PHYS_BASE.
 *
 * Returns true if successful, false if a segfault occurred.
 */
static bool
<<<<<<< HEAD
putU (uint8_t *udst, uint8_t byte) {
  if (((void*)udst < PHYS_BASE)) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
  }
    return false;

=======
put_user (uint8_t *udst, uint8_t byte) {
  // check that a user pointer `udst` points below PHYS_BASE
  if (! ((void*)udst < PHYS_BASE)) {
    return false;
  }

  int error_code;

  // as suggested in the reference manual, see (3.1.5)
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb
}


/**
 * Reads a consecutive `bytes` bytes of user memory with the
 * starting address `src` (uaddr), and writes to dst.
 *
 * Returns the number of bytes read.
 * In case of invalid memory access, exit() is called and consequently
 * the process is terminated with return code -1.
 */
static int
<<<<<<< HEAD
load_M (void *src, void *dst, size_t bytes)
{
  int32_t num;
  size_t i;
  for(i=0; i<bytes; i++) {
    num = getU(src + i);
    if(num == -1)
      bad_access();

    *(char*)(dst + i) = num & byte__;
=======
memread_user (void *src, void *dst, size_t bytes)
{
  int32_t value;
  size_t i;
  for(i=0; i<bytes; i++) {
    value = get_user(src + i);
    if(value == -1) // segfault or invalid memory access
      fail_invalid_access();

    *(char*)(dst + i) = value & 0xff;
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb
  }
  return (int)bytes;
}

/****************** Helper Functions on File Access ********************/

static struct file_desc*
<<<<<<< HEAD
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

=======
find_file_desc(struct thread *t, int fd)
{
  ASSERT (t != NULL);

  if (fd < 3) {
    return NULL;
  }

  struct list_elem *e;

  if (! list_empty(&t->file_descriptors)) {
    for(e = list_begin(&t->file_descriptors);
        e != list_end(&t->file_descriptors); e = list_next(e))
    {
      struct file_desc *desc = list_entry(e, struct file_desc, elem);
      if(desc->id == fd) {
        return desc;
      }
    }
  }

  return NULL; // not found
>>>>>>> a24f7eb888fddb040894685026ff8cf5b6a844fb
}
