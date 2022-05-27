#include "userprog/process.h"
#include "userprog/syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"




static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void push_arguments (const char *[], int cnt, void **esp);
static void setup_stack (const char *[], int argc, void **esp);

/* Starts a new thread running a user program loaded from
   `cmdline`. The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
pid_t
process_execute (const char *file_name)
{
  char *fn_copy = NULL;  // fn_copy y file_name_2 son copias exactas del file_name
  char *file_name_2 = NULL;
  char *save_ptr = NULL;
  struct process_control_block *pcb = NULL; // para guardar los datos del proceso actual
  tid_t tid;

  /* Se realiza una copia de la linea de comandos (file_name)
     de lo contrario se genera una condicion de carrera entre el call y load()
     caller como halt entre otros */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL) {
    if(fn_copy) palloc_free_page (fn_copy);
    if(file_name_2) palloc_free_page (file_name_2);
    if(pcb) palloc_free_page (pcb);
    return PID_ERROR;
  }

  strlcpy (fn_copy, file_name, PGSIZE);

  // Extraer el nombre del archivo de file_name haciendo una copia
  file_name_2 = palloc_get_page (0);

  if (file_name_2 == NULL) {
    if(fn_copy) palloc_free_page (fn_copy);
    if(file_name_2) palloc_free_page (file_name_2);
    if(pcb) palloc_free_page (pcb);
    return PID_ERROR;
  }

  strlcpy (file_name_2, file_name, PGSIZE);
  file_name_2 = strtok_r(file_name_2, " ", &save_ptr);

  // Crear un nueuvo thread que ejecute el filename.

  // Crea un PCB
  pcb = palloc_get_page(0);

  if (pcb == NULL) {
    if(fn_copy) palloc_free_page (fn_copy);
    if(file_name_2) palloc_free_page (file_name_2);
    if(pcb) palloc_free_page (pcb);
    return PID_ERROR;
  }

  pcb->pid = PID_INITIALIZING;

  pcb->cmdline = fn_copy;
  pcb->waiting = false;
  pcb->exited = false;
  pcb->orphan = false;
  pcb->exitcode = -1; // indefinido

  sema_init(&pcb->sema_initialization, 0);
  sema_init(&pcb->sema_wait, 0);

  // creamos el tread con el nombre del archivo que tokenizamos
  tid = thread_create (file_name_2, PRI_DEFAULT, start_process, pcb);

  if (tid == TID_ERROR) {
    if(fn_copy) palloc_free_page (fn_copy);
    if(file_name_2) palloc_free_page (file_name_2);
    if(pcb) palloc_free_page (pcb);
    return PID_ERROR;
  }

  // Espera hasta que la inicializacion en start_process termine
  sema_down(&pcb->sema_initialization);
  if(fn_copy) {
    palloc_free_page (fn_copy);
  }

  // Si el proceso es creado exitosamente, el children se mantiene en la lista
  if(pcb->pid >= 0) {
    list_push_back (&(thread_current()->child_list), &(pcb->elem));
  }

  palloc_free_page (file_name_2);
  return pcb->pid;

  // liberar recursos
  if(fn_copy) palloc_free_page (fn_copy);
  if(file_name_2) palloc_free_page (file_name_2);
  if(pcb) palloc_free_page (pcb);

  return PID_ERROR;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *pcb_)
{
  struct thread *t = thread_current();
  struct process_control_block *pcb = pcb_;

  char *file_name = (char*) pcb->cmdline; // linea de comandos a tokenizar
  bool success = false;

  // Arreglo para guardar los argumentos a tokenizar
  const char **argv = (const char**) palloc_get_page(0);

  if (argv == NULL) {
    printf("[Error] Kernel Error: Not enough memory\n");
    goto finish_step; // pid being -1, release lock, clean resources
  }

  char* token;
  char* save_ptr;
  int argc = 0;
  for (token = strtok_r(file_name, " ", &save_ptr); token != NULL;
      token = strtok_r(NULL, " ", &save_ptr))
  {
    argv[argc++] = token;
  }

  /* Initialize interrupt frame and load executable. */
  struct intr_frame if_;
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  if (success) {
    setup_stack(argv, argc, &if_.esp);
  }
  palloc_free_page (argv);


finish_step:

  /* Asignamos el process control block para que de esta forma logramos mapear el pid y tid,
    en base al pid se logra verificar el proceso.
  */
  pcb->pid = success ? (pid_t)(t->tid) : PID_ERROR;
  t->pcb = pcb;

  // Habilita el semaforo par poder despertar a otro proceso.
  sema_up(&pcb->sema_initialization);

  /* If load failed, quit. */
  if (!success)
    do_exit2 (-1);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  //----------------------------------
  struct thread *thr = thread_current();
  struct list *children_process= &(thr->child_list);
  struct process_control_block *c_pcb=NULL;
  struct list_elem *pos = NULL;

  if (!list_empty(children_process)) {// verificar que todavia hay procesos en espera
    for (pos = list_front(children_process); pos != list_end(children_process); pos = list_next(pos)) {
      struct process_control_block *pcb = list_entry(
          pos, struct process_control_block, elem);  // entonces si es diferente al ultimo de la espera, se mete al process control block
          

      if(pcb->pid == child_tid) { // verificamos que el pid concida para verificar que sea el proceso correcto
        c_pcb = pcb;
        break;
      }
    }
  }
  // Proceso invalido
  if(c_pcb== NULL){
    return -1;
  }
  if(c_pcb->waiting){
    return -1;
  }
  else{
    c_pcb->waiting=true;
  }

  if(!c_pcb->exited){
    sema_down(&(c_pcb->sema_wait));// ya que todavia no ha terminado no subimos el semaforo ya que esta ocupado
  }
  ASSERT(c_pcb->exited==true); //verificamos que el proceso child ya termino

//Una vez terminado debemos verificar que salga de la lista
ASSERT(pos != NULL);
list_remove(pos);

int rcode= c_pcb->exitcode;
//liberamos el proceso
palloc_free_page(c_pcb);

  //----------------------------------
  return rcode;
}


/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  struct list *fdlist = &cur->file_descriptors;
  while (!list_empty(fdlist)) {
    struct list_elem *e = list_pop_front (fdlist);
    struct file_desc *desc = list_entry(e, struct file_desc, elem);
    file_close(desc->file);
    palloc_free_page(desc);
  }

  struct list *child_list = &cur->child_list;
  while (!list_empty(child_list)) {
    struct list_elem *e = list_pop_front (child_list);
    struct process_control_block *pcb;
    pcb = list_entry(e, struct process_control_block, elem);
    if (pcb->exited == true) {
      palloc_free_page (pcb);
    } else {
      pcb->orphan = true;
    }
  }

  if(cur->executing_file) {
    file_allow_write(cur->executing_file);
    file_close(cur->executing_file);
  }

  sema_up (&cur->pcb->sema_wait);

  if (cur->pcb->orphan == true) {
    palloc_free_page (& cur->pcb);
  }

  pd = cur->pagedir;
  if (pd != NULL)
    {
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

//static void setup_stack (const char *[], int argc, void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  //if (!setup_stack (esp))
    //goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  /* Deny writes to executables. */
  file_deny_write (file);
  thread_current()->executing_file = file;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */

  // do not close file here, postpone until it terminates
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}


/*
 * Push arguments into the stack region of user program
 * (specified by esp), according to the calling convention.
 */
/*static void
push_arguments (const char* cmdline_tokens[], int argc, void **esp)
{
  /* cmdline_tokens es la lista de argumentos 
     agrc es la cantidad de argumentos enviados 
     esp es nuestro stack
  /
  ASSERT(argc >= 0); //Verificamos si existen argumentos.

  int i, len = 0;
  void* argv_addr[argc];
  for (i = 0; i < argc; i++) {                   //recorremos la lista de argumentos 
    len = strlen(cmdline_tokens[i]) + 1;         // bajamos el stack por el tamaño del string y el +1 por el valor nullo al final de la cadena
    *esp -= len;                                 // se baja el para que cada argumento quede en una posicion unica de memoria
    memcpy(*esp, cmdline_tokens[i], len);
    argv_addr[i] = *esp;                         //arreglo que contiene la direccion de cada argumento.
  }

  // Alineacion de las palabras y/o argumentos para alinear el stack con la memoria
  *esp = (void*)((unsigned int)(*esp) & 0xfffffffc);

  // Escribe la direccion del argv
  *esp -= 4;
  *((uint32_t*) *esp) = 0;

  // Escribir la direccion de cada argumento del proceso
  for (i = argc - 1; i >= 0; i--) {
    *esp -= 4;
    *((void**) *esp) = argv_addr[i];
  }

  // setting **argv (addr of stack, esp)
  *esp -= 4;
  *((void**) *esp) = (*esp + 4);

  // Escribir la cantidad de argc (argumentos mas nombre)
  *esp -= 4;
  *((int*) *esp) = argc;

  // La direccion de retorno debe ser 0
  *esp -= 4;
  *((int*) *esp) = 0;

}//*/


/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static void
setup_stack(const char* cmdline_tokens[], int argc, void **esp)
{

  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage != NULL)
  {
    success = install_page(((uint8_t *)PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
    {
      *esp = PHYS_BASE;
    }
    else
    {
      palloc_free_page(kpage);
    }
  }
  
  /* cmdline_tokens es la lista de argumentos 
     agrc es la cantidad de argumentos enviados 
     esp es nuestro stack
  */
  ASSERT(argc >= 0); //Verificamos si existen argumentos.

  int i, len = 0;
  void* argv_addr[argc];
  for (i = 0; i < argc; i++) {                   //recorremos la lista de argumentos 
    len = strlen(cmdline_tokens[i]) + 1;         // bajamos el stack por el tamaño del string y el +1 por el valor nullo al final de la cadena
    *esp -= len;                                 // se baja el para que cada argumento quede en una posicion unica de memoria
    memcpy(*esp, cmdline_tokens[i], len);
    argv_addr[i] = *esp;                         //arreglo que contiene la direccion de cada argumento.
  }

  // Alineacion de las palabras y/o argumentos para alinear el stack con la memoria
  *esp = (void*)((unsigned int)(*esp) & 0xfffffffc);

  // Escribe la direccion del argv
  *esp -= 4;
  *((uint32_t*) *esp) = 0;

  // Escribir la direccion de cada argumento del proceso
  for (i = argc - 1; i >= 0; i--) {
    *esp -= 4;
    *((void**) *esp) = argv_addr[i];
  }

  // setting **argv (addr of stack, esp)
  *esp -= 4;
  *((void**) *esp) = (*esp + 4);

  // Escribir la cantidad de argc (argumentos mas nombre)
  *esp -= 4;
  *((int*) *esp) = argc;

  // La direccion de retorno debe ser 0
  *esp -= 4;
  *((int*) *esp) = 0;
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

// revisar setup_stack las lines de 23 a 27 pertenecen a la fase 3
// fusionar push_arguments con setup_stack
// revisar process_wait, process_exit