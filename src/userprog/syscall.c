#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "userprog/pagedir.h"

#define PHYS_TOP ((void *) 0x08048000)

bool low_fd_func (const struct list_elem *a,
	const struct list_elem *b,
	void *aux){
  return list_entry (a, struct file_desc, fd_elem)->fd <
	
	list_entry (b, struct file_desc, fd_elem)->fd;
}
static int (*syscall_case[20]) (struct intr_frame *f);

static void syscall_handler (struct intr_frame *);
static void valid_usrptr (const void *uaddr);
void valid_multiple (int *esp, int num);
struct file_desc *fd_find (int fd);

static int syscall_halt_ (struct intr_frame *f){
  power_off ();
  return -1;
}
void exit_ (int status){  
  //struct dead_body *db;
  struct thread *cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);

  struct thread *parent = get_thread (cur->pa_tid);
  if (parent != NULL){
	struct list_elem *e;
	struct dead_body *temp;
	for (e=list_begin (&parent->ch_list);
		e!=list_end (&parent->ch_list);
		e=list_next (e)){
	  temp = list_entry (e, struct dead_body, ch_elem);
	  if (temp->ch_tid == cur->tid){
		lock_acquire (&parent->ch_lock);
		temp->user_kill = true;
		temp->exit_status = status;
		temp->alive = false;
		lock_release (&parent->ch_lock);
	  }
	}
  }
    
  //thread_current ()->exit_status = status;
  //printf("exit_ call with %d\n", status);
  //printf("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}
static int syscall_exit_ (struct intr_frame *f){  

  valid_usrptr(f->esp+4);
  int status = * (int *) (f->esp+4);
  //printf("EXIT CALL WITH %d\n", status);
  if (status == NULL){
	status = 0;
  }
  //printf("Given child status = %d\n", status);
  /*
   * struct list_elem *e;
  struct dead_body *temp;
  struct thread *parent = get_thread (thread_current ()->pa_tid);
  for (e=list_begin (&parent->ch_list);
	  e!=list_end (&parent->ch_list);
	  e=list_next(e)){
	temp = list_entry (e, struct dead_body, ch_elem);
	if (temp->ch_tid == thread_current ()->tid){
	  temp->exit_status = status;
	}
  }*/
  exit_ (status);
  return 0;
  
}
static int syscall_exec_ (struct intr_frame *f){
  valid_multiple(f->esp,1);
  //valid_usrptr(*(uint32_t *) (f->esp+4));
  //printf("GOOD ESP\n");
  char * file_name = * (char **) (f->esp+4);
  valid_usrptr (file_name);
  
  tid_t tid;
  tid = process_execute (file_name);
  f->eax = tid;
  /*
  char *f_name = malloc (strlen(file_name)+1);
  strlcpy (f_name, file_name, strlen(file_name)+1);
  char *save_ptr;
  f_name = strtok_r (f_name, " ", &save_ptr);
  acquire_filesys_lock ();
  struct file* ff = filesys_open (f_name);
  release_filesys_lock ();
  if(ff==NULL){
	f->eax = -1;
	return 0;
  }
  acquire_filesys_lock ();
  file_close (ff);
  release_filesys_lock ();
  free(f_name);
  
  f->eax = process_execute(file_name);
   
  */
  //printf("NEW PID [%d] is BORN\n", tid);
  return 0;
}

static int syscall_wait_ (struct intr_frame *f){
  valid_multiple (f->esp, 1);
  tid_t child_tid = * (int *) (f->esp+4);
  //printf("child_tid [%d] \n",child_tid);
  f->eax = process_wait(child_tid);
  return 0;
}
static int syscall_create_ (struct intr_frame *f){
  valid_multiple((uint32_t *) f->esp, 2);
  valid_usrptr (*(uint32_t *) (f->esp+4));
  char *file = * (char **) (f->esp+4);
  unsigned size = * (unsigned *) (f->esp+8);
  bool success = false;
  acquire_filesys_lock ();

  success = filesys_create (file, size);
  f->eax = success;

  release_filesys_lock ();
  return 0;
} 
static int syscall_remove_ (struct intr_frame *f){
  valid_multiple(f->esp, 1);
  valid_usrptr(*(uint32_t *) (f->esp+4));
  char *file = * (char **) (f->esp+4);
  bool success = false;

  acquire_filesys_lock ();

  success = filesys_remove (file);
  f->eax = success;
  release_filesys_lock ();
  
  return 0;
} 

static int syscall_open_ (struct intr_frame *f){
  valid_multiple (f->esp, 1);
  valid_usrptr (* (uint32_t *) (f->esp+4));
  char *file_name = * (char **) (f->esp+4);
  acquire_filesys_lock ();
  struct file *ff = filesys_open (file_name);
  
  release_filesys_lock ();
  //printf("OPENING BY PID[%d]\n", thread_current ()->tid);
  
  
  
  if (ff==NULL){
	//printf("NOFILE\n");
	f->eax=-1;
	return 0;
  }
  struct file_desc *fd_ = malloc(sizeof(*fd_));
  fd_->file = ff;
  if (list_empty (&thread_current ()->fd_list)){
	  ///sdfsdfsdf
	  fd_->fd=3;
	  list_push_back (&thread_current ()->fd_list, &fd_->fd_elem);
	  //release_filesys_lock ();
	  f->eax=fd_->fd;
  	  //printf("...........fd[%d]\n",fd_-> fd);
	  return fd_->fd;
  }else{
	//what if jsut fd+1//
	/*	
	fd_->fd = list_entry (list_back(&thread_current ()->fd_list), struct file_desc, fd_elem) -> fd +1;
	list_push_back (&thread_current ()->fd_list, &fd_->fd_elem);
	*/
	//=================//
	
	struct list_elem *e;
	int temp_fd = 3;
	struct file_desc *temp;
	for (e=list_begin (&thread_current ()->fd_list);
		e!=list_end (&thread_current ()->fd_list);
		e=list_next(e)){
	  temp = list_entry (e, struct file_desc, fd_elem);
	  if (temp_fd != temp->fd){
		fd_->fd=temp_fd;
		list_insert_ordered (&thread_current ()->fd_list, &fd_->fd_elem, low_fd_func, NULL);
		//release_filesys_lock ();
		f->eax = fd_->fd;
		return fd_->fd;
	  }
	  temp_fd++;
	}
	fd_->fd=temp_fd;
	list_insert_ordered (&thread_current ()->fd_list, &fd_->fd_elem, low_fd_func, NULL);
	//release_filesys_lock ();
	f->eax = fd_->fd;
	return fd_->fd;
	
	//=====continued part=====//
	/*
	f->eax = fd_->fd;
  	//printf("...........fd[%d]\n",fd_-> fd);
	return fd_->fd;
	*/
	//=======================//
  }

  return -1;
}

static int syscall_filesize_ (struct intr_frame *f){
  valid_multiple (f->esp, 1);
  int fd = * (int *) (f->esp+4);

  struct file_desc *temp = fd_find (fd);
  if (temp == NULL){
	f->eax = -1;
    return -1;
  }
  acquire_filesys_lock ();
  f->eax = file_length (temp->file);
  release_filesys_lock ();
  /*
  struct list_elem *e;
  struct file_desc *temp;
  for (e=list_begin (&thread_current ()->fd_list);
	  e!=list_end (&thread_current ()->fd_list);
	  e=list_next(e)){
	temp = list_entry (e, struct file_desc, fd_elem);
	if (fd==temp->fd){
	  acquire_filesys_lock ();
	  f->eax = file_length (temp->file);
	  release_filesys_lock ();
	  return 0;
	}
  }*/

  return 0;
}

static int syscall_read_ (struct intr_frame *f){
  //printf("START TO READ PID[%d]\n",  thread_current ()->tid);
  valid_multiple (f->esp, 3);
  //valid_usrptr (* (uint32_t *) (f->esp+8));
  int fd = * (int *) (f->esp+4);
  char *buffer = * (char **) (f->esp+8);
  //printf("CHECK BUF\n");
  valid_usrptr ((const uint8_t *) buffer);
  unsigned size = * (unsigned *) (f->esp+12);
  //printf("[%d]......TRYINg TO READ fd[%d]\n", thread_current ()->tid, fd);
  
  //acquire_filesys_lock ();
  if (fd==0){
	acquire_filesys_lock ();
	int i = 0;
	while(i< (int) size){
	  * (buffer + i) = input_getc();
	  i++;
	}
	release_filesys_lock ();
	f->eax= size;
	//goto finish;
	return 0;
  }else{
	struct file_desc *temp = NULL;
	struct list_elem *e = NULL;
	//printf("When is fault...#1\n");
	for (e=list_begin (&thread_current ()->fd_list);
		e!=list_end (&thread_current ()->fd_list);
		e=list_next(e)){
	  //printf("When is fault....#k\n");
	  temp = list_entry (e, struct file_desc, fd_elem);
	  //printf("[%d].................[%d]\n", fd, temp->fd);
	  if (fd==temp->fd){
		if (temp->file == NULL){
		  f->eax = -1;
		  //goto finish;
		  return 0;
		}
		acquire_filesys_lock ();
		//printf("read_attemp\n");
		f->eax=file_read (temp->file, buffer, size);
		//printf("read_done.....[size:%d], [f->eax:%d]\n", size, f->eax);
		release_filesys_lock ();
		//goto finish;
		return 0;
	  }
	}
  }
   
  //return -1;
  //finish:
  //release_filesys_lock ();
  return -1;
}

static int syscall_write_ (struct intr_frame *f){
  valid_multiple (f->esp, 3);
  valid_usrptr (* (uint32_t *) (f->esp+8));
  int fd = *(int *) (f->esp+4);
  char *buffer = * (char **) (f->esp+8);
  unsigned size = *(unsigned *) (f->esp+12);

  if (fd==1){
	acquire_filesys_lock ();
	putbuf(buffer ,size);
	f->eax = (int) size;
	release_filesys_lock ();
	return 0;
  }else{
	struct list_elem *e;
	struct file_desc *temp;
	for (e=list_begin (&thread_current ()->fd_list);
		e!=list_end (&thread_current ()->fd_list);
		e=list_next(e)){
	  temp = list_entry (e, struct file_desc, fd_elem);
	  if (fd==temp->fd){
		acquire_filesys_lock ();
		f->eax=file_write(temp->file, buffer, size);
		release_filesys_lock ();
		return 0;
	  }
	}
  }
  return -1;
}

static int syscall_seek_ (struct intr_frame *f){
  valid_multiple (f->esp, 2);
  int fd = * (int *) (f->esp+4);
  unsigned position = * (unsigned *) (f->esp+8);
  struct file_desc *temp = fd_find(fd);
  if (temp == NULL){
	//f->eax = -1;
	return 0;
  }
  acquire_filesys_lock ();
  file_seek (temp->file, position);
  release_filesys_lock ();
  /*struct list_elem *temp = fd_find(fd);
  if (e==NULL){
	f->eax = -1;
	return 0;
  }
  struct file_desc *temp = list_entry (e, struct file_desc, fd_elem);
  acquire_filesys_lock ();
  f->eax = file_seek (temp->file, position);
  release_filesys_lock ();
  */
  return 0;
}


static int syscall_tell_ (struct intr_frame *f){
  valid_multiple (f->esp, 1);
  int fd = * (int *) (f->esp+4);
  /*
   * struct list_elem *e = fd_find(fd);
  if (e==NULL){
	f->eax =-1;
	return 0;
  }
  struct file_desc *temp = list_entry (e, struct file_desc, fd_elem);
  */
  struct list_elem *e;
  struct file_desc *temp;
  for (e=list_begin (&thread_current ()->fd_list);
	  e!=list_end (&thread_current ()->fd_list);
	  e=list_next(e)){
	temp = list_entry (e, struct file_desc, fd_elem);
	if (fd == temp->fd){
	  acquire_filesys_lock ();
	  f->eax = file_tell (temp->file);
	  release_filesys_lock ();
	  return 0;
	}
  }
  f->eax = -1;
  return 0;
}


static int syscall_close_ (struct intr_frame *f){
  
  valid_multiple (f->esp, 1);
  //valid_usrptr (* (uint32_t *) (f->esp+4));
  int fd  = * (int *) (f->esp+4);
  
  struct list_elem *e;
  struct file_desc *temp;
  for (e=list_begin (&thread_current ()->fd_list);
	  e!=list_end (&thread_current ()->fd_list);
	  e=list_next(e)){
	temp = list_entry (e, struct file_desc, fd_elem);
	if (temp->fd == fd){
	  acquire_filesys_lock ();
	  file_close (temp->file);
	  release_filesys_lock ();
	  list_remove(e);
	  free(temp);
	  return 0;
	}
  }
  
  return -1;
}


static int syscall_mmap_ (struct intr_frame *f){
  return -1;
}


static int syscall_munmap_ (struct intr_frame *f){
  return -1;
}


static int syscall_chdir_ (struct intr_frame *f){
  return -1;
}


static int syscall_mkdir_ (struct intr_frame *f){
  return -1;
}


static int syscall_readdir_ (struct intr_frame *f){
  return -1;
}


static int syscall_isdir_ (struct intr_frame *f){
  return -1;
}


static int syscall_inumber_ (struct intr_frame *f){
  return -1;
}



void
syscall_init (void) 
{
  //syscall 0~4
  syscall_case[SYS_HALT] = &syscall_halt_;
  syscall_case[SYS_EXIT] = &syscall_exit_;
  syscall_case[SYS_EXEC] = &syscall_exec_;
  syscall_case[SYS_WAIT] = &syscall_wait_;
  syscall_case[SYS_CREATE] = &syscall_create_;
  //syscall 5~9
  syscall_case[SYS_REMOVE] = &syscall_remove_;
  syscall_case[SYS_OPEN] = &syscall_open_;
  syscall_case[SYS_FILESIZE] = &syscall_filesize_;
  syscall_case[SYS_READ] = &syscall_read_;
  syscall_case[SYS_WRITE] = &syscall_write_;
  //syscall 10~12
  syscall_case[SYS_SEEK] = &syscall_seek_;
  syscall_case[SYS_TELL] = &syscall_tell_;
  syscall_case[SYS_CLOSE] = &syscall_close_;
  //syscall for P3 13,14
  syscall_case[SYS_MMAP] = &syscall_mmap_;
  syscall_case[SYS_MUNMAP] =&syscall_munmap_;
  //syscall for P4 15~19
  syscall_case[SYS_CHDIR] = &syscall_chdir_;
  syscall_case[SYS_MKDIR] = &syscall_mkdir_;
  syscall_case[SYS_READDIR] = &syscall_readdir_;
  syscall_case[SYS_ISDIR] = &syscall_isdir_;
  syscall_case[SYS_INUMBER] = &syscall_inumber_;

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int syscall_write(struct intr_frame *f)
{
 int fd = *(int *)(f->esp + 4);
 void *buffer = *(char**)(f->esp +8);
 unsigned size = *(unsigned *)(f->esp + 12);

}
static void
syscall_handler (struct intr_frame *f) 
{
  valid_usrptr(f->esp);
  valid_multiple(f->esp, 3);
  int syscall_num = * (int *) f->esp;
//  int syscall_num;
/*  asm volatile ("movl %0, %%esp; popl %%eax" : "=a" (syscall_num) : "g" (f->esp));
*/
  //printf("STARTING %d function call\n", syscall_num);
 
  if (syscall_case[syscall_num] (f) == -1){
	exit_(-1);
	return;
  }
  //thread_exit ();
}

static void valid_usrptr (const void *uaddr){
  //printf("PID[%d] is checking validity....\n",thread_current ()->tid);
  if (!is_user_vaddr (uaddr)){
	//printf("CASE1!\n");
	exit_(-1);
	return;
  }
  /*
  if (uaddr < PHYS_TOP){
	exit_(-1);
	return;
  }*/
  if (uaddr == NULL){
	//printf("CASE2!\n");
	exit_(-1);
	return;
  }
  if ((pagedir_get_page (thread_current ()->pagedir, uaddr))==NULL){
	//printf("CASE3!\n");
	exit_(-1);
	return;
  }
  /*
  if (!is_user_vaddr (uaddr)){
	exit(-1);
	return;
  }
  
  if (uaddr >= PHYS_BASE){
	exit(-1);
	//SOMETHING preventing 'leak' should be inserted
	return;
  }
  
  if (uaddr < PHYS_TOP){
	exit(-1);
	return;
  }
  //printf("CHECKING VALIDITY\n"); 
  void *temp = pagedir_get_page (thread_current ()->pagedir, uaddr);
  if ( temp == NULL){
	//printf("BAAAAAAAD\n");
	exit(-1);
	//SOEMTHING preventing 'leak'
	return;
  }
  */
}
void valid_multiple (int * esp, int num){
  //int *ptr;
  int i;
  //printf("CHECKING MULTIPLES\n");
  for (i =0; i < num; i++){
	//ptr = (int *) f->esp + i + 1;
	valid_usrptr( esp + i + 1);
  }
}

struct file_desc *fd_find (int fd){
  struct list_elem *e;
  struct file_desc *temp = NULL;
  for (e=list_begin (&thread_current ()->fd_list);
	  e!=list_end (&thread_current ()->fd_list);
	  e=list_next (e)){
	temp = list_entry (e, struct file_desc, fd_elem);
	if (fd==temp->fd){
	  return temp;
	}
  }
  return NULL;

}
