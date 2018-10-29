#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

struct frt_entry {
  void *frame;
  void *upage;
  tid_t tid;
  bool in_use;
  struct list_elem frt_elem;
};

struct lock frt_lock; 
struct list frt;

void vm_frt_init (void);
void *vm_frame_alloc (enum palloc_flags);
void vm_frame_set (void *, void *);
void vm_frame_free (void *);


void acquire_frt_lock (void);
void release_frt_lock (void);
struct frt_entry *get_frt_entry (void *);

