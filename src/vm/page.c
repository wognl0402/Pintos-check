#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"

bool vm_spt_init (void){
  struct thread *t = thread_current ();
  //printf("init!!!\n");
  return hash_init (&t->spt, spt_hash_func, spt_hash_less_func, NULL);
}

void vm_spt_destroy (struct hash *h){
  hash_destroy (h, vm_spt_free);
}

//bool hash_init ();
//void hash_clear ();
//hash_destroy ();
//size_t hash_size ();
//bool hash_empty ();

/* HASH OPERATION FUNC */
struct spt_entry *vm_get_spt_entry (struct hash *h, void *upage){
  struct spt_entry spte;

  spte.upage = upage;
  struct hash_elem *e = hash_find (h, &spte.spt_elem);
  
  //PANIC("DURING vm_get");
  if (e == NULL)
	return NULL;
  
  return hash_entry (e, struct spt_entry, spt_elem);
}

bool vm_is_in_spt (struct hash *h, void *upage){
  struct spt_entry *spte = vm_get_spt_entry (h, upage);
  //printf("??????\n");
  if (spte == NULL)
	return false;
  
  return true;
}

bool vm_put_spt_entry (struct hash *h, void *upage, void *kpage){
  struct spt_entry *spte;
  spte = malloc (sizeof *spte);
  
  if (spte == NULL){
	printf("vm_put_spt_entry: CAN'TMAKE spt_entry\n");
	return false;
  }

  spte->upage = upage;
  spte->kpage = kpage;
  spte->swap_index = -1;
  spte->status = ON_FRAME;

  struct hash_elem *e = hash_insert (h, &spte->spt_elem);
  if (e == NULL){
	return true;
  }else{
	free (spte);
	return false;
  }
}

bool vm_spt_reclaim (struct hash *h, struct spt_entry *spte){
  void *kpage = vm_frame_alloc (PAL_USER);

  if (kpage == NULL)
	PANIC ("HAVE YOU IMPLEMENTED EVCITION?");

  if (!pagedir_set_page (thread_current ()->pagedir, spte->upage, kpage, true)){
	vm_frame_free (kpage);
	return false;
  }

  vm_swap_in (spte->swap_index, spte->upage);
  
  spte->kpage = kpage;
  spte->status = ON_FRAME;

  return true;
}

bool vm_set_swap (struct hash *h, void *upage, int swap_index){
  struct spt_entry *spte = vm_get_spt_entry (h, upage);
  if (spte == NULL)
	return false;
	//PANIC ("vm_set_swap: NO SUCH spt entry");

  spte->status = ON_SWAP;
  spte->swap_index = swap_index;
  return true;
}
void vm_stack_grow (struct hash *h, void *fault_page){
  //printf("GO TO GROWING!!!!!!\n");
  //struct thread *t = thread_current ();
  void *frame = vm_frame_alloc (PAL_USER | PAL_ZERO);
  if (frame == NULL){
	PANIC ("NO FRAME FOR GROW");
	
	//printf("Still no eviction...\n");
  }//else{

  //struct spt_entry *spte;
  //spte = malloc (sizeof (*spte));
  //spte->upage = pg_round_down (fault_addr);
  //spte->upage = fault_page;
  //spte->kpage = frame;
  //spte->status = CLEARED;

  struct thread *t = thread_current ();
  if (pagedir_get_page (t->pagedir, fault_page) != NULL)
	PANIC ("WHILE GROW, ALREADY IN PAGEDIR");
  if (!pagedir_set_page (t->pagedir, fault_page, frame, true))
	PANIC ("CANNOT SET PAGEDIR");
  if (!vm_put_spt_entry (&t->spt, fault_page, frame))
	PANIC ("CANNOT PUT INTO SPT");
  
  return;
	  /*
  struct hash_elem *e = hash_insert (h, &spte->spt_elem);
  if (e == NULL){
	PANIC ("WHILE GROW, DUPLICATED HASH ELEM");
  }
  struct thread *t = thread_current ();
  //pagedir_set_page (t->pagedir, fault_page)
  
  return;
  */
 }
//struct hash_elem *vm_put_spt_entry (struct hash *h, void *upage){

/* HASH INIT FUNC */

unsigned spt_hash_func (const struct hash_elem *h, void *aux){
  //Hash the vaddr
  struct spt_entry *temp;
  temp = hash_entry (h, struct spt_entry, spt_elem);
  return hash_bytes (temp->upage, sizeof (temp->upage));
  //return;
}

bool spt_hash_less_func (const struct hash_elem *a,
	const struct hash_elem *b,
	void *aux){

  const struct spt_entry *sa = hash_entry (a, struct spt_entry, spt_elem);
  const struct spt_entry *sb = hash_entry (b, struct spt_entry, spt_elem);
  //return (sa->upage - sb->upage) > 0;
  return sa->upage > sb->upage;
}

void vm_spt_free (struct hash_elem *e, void *aux UNUSED){
  struct spt_entry *spte;
  spte = hash_entry (e, struct spt_entry, spt_elem);
  if (spte->kpage != NULL){
	//PANIC ("WHY ARE YOU STILL HERE!!!\n");
	//vm_frame_free (spte->kpage);
	vm_frame_destroy (spte->kpage);
	//if (!vm_frame_free (spte->kpage)){
	  //printf ("vm_spt_free: vm_frame_free error\n");
  }else if (spte->status == ON_SWAP){
	vm_swap_free (spte->swap_index);
  }

  free (spte);
}

