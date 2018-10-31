#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
//#include "tests/lib.h"

static struct lock frt_evict_lock;

void vm_frt_init (void){
  list_init (&frt);
  lock_init (&frt_lock);
  lock_init (&frt_evict_lock);
  return;
}


void *vm_frame_alloc (enum palloc_flags flags){
  void *page;
  void *frame = palloc_get_page (flags);
  if (frame == NULL){
	//PANIC ("WE WHOULD EVICT!!!");
	//printf("vm_frame_alloc: WE HAVE TO EVICT SOME FRAME\n");
	struct frt_entry *victim  = vm_frame_evict ();
	//ASSERT (temp != NULL);

	struct thread *t = get_thread (victim->tid);
	ASSERT (t != NULL);
	
	int swap_index = vm_swap_out (victim->frame);
	
	pagedir_clear_page (t->pagedir, victim->upage);

	if (!vm_set_swap (&t->spt, victim->upage, swap_index))
	  PANIC ("vm_set_swap: NO SUCH spt entry");
	
	//frame = victim->kpage;
	vm_frame_free (victim->frame);

	frame = palloc_get_page (flags);
	if (frame == NULL)
	  PANIC ("Eviction failed again");
  }

  struct frt_entry *f;
  f = malloc (sizeof (*f));
  if (f == NULL){
	printf("vm_frame_alloc: CAN'T MAKE NEW frt_entry\n");
	return NULL;
  }
  f->frame = frame;
  f->tid = thread_current ()->tid;
  f->in_use = true;
  acquire_frt_lock ();
  list_push_back (&frt, &f->frt_elem);
  release_frt_lock ();
  return frame;
}
struct frt_entry *vm_frame_evict (void){

  struct frt_entry *victim;

  lock_acquire (&frt_evict_lock);

  victim = vm_evict_SC ();
  ASSERT (victim != NULL);

  lock_release (&frt_evict_lock);
  return victim;
}

struct frt_entry *vm_evict_SC (void){
  struct list_elem *e;
  bool second = false;
  struct frt_entry *f;
  struct thread *t;
  //struct thread *t = thread_current ();
  if (list_empty (&frt)){
	printf("vm_evict_SC: frt empty\n");
  }

SCAN:
  for (e = list_begin (&frt);
	  e != list_end (&frt);
	  e = list_next (e)){
	f = list_entry (e, struct frt_entry, frt_elem);
	if (f->in_use)
	  //continue;
	//msg ("[%d] directory checked", f->tid);
	//printf ("[%d] directory checked", f->tid);
	t = get_thread (f->tid);

	return f;
	/*
	if (pagedir_is_accessed (t->pagedir, f->upage)){
	  if (second)
		printf("CAN'T be happend\n");
	  pagedir_set_accessed (t->pagedir, f->upage, false);
	}else{
	  //list_remove (e);
	  return f;
	  //break;
	}
	*/
  }

  if (!second){
	printf("FUCKYOU\n");
	second = true;
	goto SCAN;
  }else{
	return NULL;
  }
  return NULL;
}

void vm_frame_set (void *kpage, void*upage){

  struct frt_entry *f = get_frt_entry (kpage);
  
  ASSERT (f->tid == thread_current ()->tid);
  ASSERT (f->in_use);

  ASSERT (pg_ofs (upage) == 0);
  acquire_frt_lock ();
  
  f->upage = upage;
  f->in_use = false;

  release_frt_lock ();
  return;
}

void vm_frame_free (void *frame)
{
  struct frt_entry *f = get_frt_entry (frame);
  if(f == NULL){
	printf ("vm_frame_free: NO SUCH FRAME EXIST\n");
	return;
  }
  //Remove frt_entry from the frt
  acquire_frt_lock ();
  list_remove (&f->frt_elem);
  free (f);
  //Free the frame
  //printf("vm_frame_palloc?\n");
  palloc_free_page (frame);

  release_frt_lock ();
  return;
}

void vm_frame_destroy (void *frame)
{
  struct frt_entry *f = get_frt_entry (frame);
  if(f == NULL){
	printf ("vm_frame_free: NO SUCH FRAME EXIST\n");
	return;
  }
  //Remove frt_entry from the frt
  acquire_frt_lock ();
  list_remove (&f->frt_elem);
  free (f);
  //Free the frame
  //printf("vm_frame_palloc?\n");
  release_frt_lock ();
  //palloc_free_page (frame);

  return;
}
//HELP TO ACCESS frt, frt_lock 

void acquire_frt_lock (void){
  lock_acquire (&frt_lock);
}

void release_frt_lock (void){
  lock_release (&frt_lock);
}

struct frt_entry *get_frt_entry (void *frame){
  struct list_elem *e;
  struct frt_entry *temp;

  acquire_frt_lock ();
  for (e=list_begin (&frt); e!=list_end (&frt); e=list_next (e)){
	temp = list_entry (e, struct frt_entry, frt_elem);
	if (temp->frame == frame){
	  release_frt_lock ();
	  return temp;
	}
  }
  release_frt_lock ();
  return NULL;
}

