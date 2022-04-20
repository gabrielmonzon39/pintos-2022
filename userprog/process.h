#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct waiting_to_load_C {
    tid_t id;
    int value;
    int state;
    struct list_elem elem;
}


#endif /* userprog/process.h */
