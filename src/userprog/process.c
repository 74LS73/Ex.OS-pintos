#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>
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
#include "threads/synch.h"
#include "threads/vaddr.h"


static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

#ifdef VM
unsigned mapfile_hash_hash_func (const struct hash_elem *, void *);
bool mapfile_hash_less_func (const struct hash_elem *, const struct hash_elem *, void *);
void mapfile_hash_destory_func (struct hash_elem *e, void *aux);
#endif

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
pid_t
process_execute (const char *cmd_line) 
{
  char *fn_copy;
  tid_t tid;
  //ADD
  struct process *p = palloc_get_page(0);
  if (p == NULL) {
    return TID_ERROR;
  }
  //END

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, cmd_line, PGSIZE);

  // ADD
  // 初始化很重要
  p->cmd_line = fn_copy;
  p->pid = -1;
  memset(p-> file_descriptor_table, 0, sizeof (p-> file_descriptor_table));
  sema_init (&p->exit_sema, 0);
  lock_init (&p->ensure_once_wait);
  sema_init (&p->init_process_sema, 0);
#ifdef VM
  p->map_files = malloc (sizeof (map_file));
  hash_init (p->map_files, mapfile_hash_hash_func, mapfile_hash_less_func, NULL);
#endif
  p->executing_file = NULL;

  char *file_name, *save_ptr; 
  file_name = palloc_get_page (0);
  if (file_name == NULL) {
    return TID_ERROR;
  }
  strlcpy (file_name, cmd_line, PGSIZE);
  file_name = strtok_r (file_name, " ", &save_ptr);
  //END
  /* Create a new thread to execute FILE_NAME. */
  
    
  tid = thread_create (file_name, PRI_DEFAULT, start_process, p);
  if (tid == TID_ERROR)
    {
      palloc_free_page (fn_copy); 
      palloc_free_page (p); 
      palloc_free_page (file_name); 
      return -1;
    }
  // ADD
  else 
    {
      sema_down (&p->init_process_sema);
      list_push_back (&thread_current ()->children, &p->child_elem);
    }
  // END
  palloc_free_page (file_name); 
  return p->pid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *process_)
{
  struct process* cur_process = process_;
  struct thread*  cur_thread  = thread_current ();
  cur_thread -> process = cur_process;

  char *cmd_line = cur_process->cmd_line;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (cmd_line, &if_.eip, &if_.esp);

  // ADD 将father需要的信息通过struct process传回去
  // 结束
  cur_process->pid = success ? cur_thread->tid : TID_ERROR;
  sema_up (&cur_process->init_process_sema);
  // END
  
  /* If load failed, quit. */
  palloc_free_page (cmd_line);
  if (!success) 
    thread_exit ();

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
process_wait (pid_t child_pid) 
{
  struct list_elem *e;
  struct thread* cur = thread_current ();
  struct process *child;
  int status;
  bool is_find = false;
  for (e = list_begin (&cur->children); e != list_end (&cur->children);
       e = list_next (e))
    {
      child = list_entry (e, struct process, child_elem);
      if (child->pid == child_pid) {is_find = true; break;}
    }
  // 如果是子程序，并且是第一次对这个子程序调用wait
  if (is_find && lock_try_acquire (&child->ensure_once_wait)) 
    {
      sema_down (&child->exit_sema);
      list_remove (&child->child_elem);
      status = child->exit_status; 
      palloc_free_page (child);
    }
  else
    {
      status = -1;
    }
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  struct process *p = cur->process;
  uint32_t *pd;
  
  //ADD 关闭所有打开的文件
  int i = FD_START;
  while (i <= FD_END) 
    {
    
      if (p->file_descriptor_table[i] != NULL) {
        file_close (p->file_descriptor_table[i]);
        p->file_descriptor_table[i] = NULL;
      }
      i++;
    }
  // AND
#ifdef VM
  // UNMAP所有文件
  hash_destroy (p->map_files, mapfile_hash_destory_func);
#endif
  //END
  
  
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

#ifdef VM
  // 释放SPT
  vm_spt_destory (cur->spt);
#endif

  // ADD
  // 有可能是加载文件失败的程序
  if (p->executing_file != NULL)
    file_allow_write (p->executing_file);
  sema_up(&p->exit_sema);
  // END
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

static bool setup_stack (void **esp, int argc, char **argv);
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
#ifdef VM
  t->spt = vm_spt_create ();
#endif
  if (t->pagedir == NULL || t->spt == NULL) 
    goto done;
  process_activate ();

  // 分解命令行参数
/*  char cmd_line[128];*/
/*  strlcpy(cmd_line, file_name, 128);*/
  int argc;
  char *argv[128];
  parse_command_args (file_name, &argc, argv);
  
  /* Open executable file. */
  file = filesys_open (argv[0]);
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
  if (!setup_stack (esp, argc, argv))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;
  
  //ADD
  // 防止当前运行文件被写
  // 所以就保持文件开启
  // 故直接返回
  //int i = 0;
  for (i=0;i<argc;i++)
    {
      free(argv[i]);
    }
  file_deny_write (file);
  t->process->executing_file = file;
  return success;
  //END
  
 done:
  /* We arrive here whether the load is successful or not. */
  for (i=0;i<argc;i++)
    {
      free(argv[i]);
    }
  file_close (file);
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
  
#ifdef VM
  struct thread *cur_thread = thread_current ();
#endif

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;


#ifdef VM
      // Lazy load
      // 只是添加SPTE而不是直接加载到内存
      // SPTE储存需要的信息
      vm_spte *spte = vm_spte_create_for_file (upage, file, ofs, page_read_bytes, page_zero_bytes, writable);
      if (!vm_spt_insert (cur_thread->spt, spte)) 
        {
          // hash表插入失败
          free (spte);
          return false;
        }

#else
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
#endif

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;

#ifdef VM
      // 实际上并没有写文件，所以需要更新ofs
      ofs += PGSIZE;
#endif

    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, int argc, char **argv) 
{
  uint8_t *kpage;
  bool success = false;
#ifdef VM
  kpage = falloc_get_frame (PAL_USER | PAL_ZERO);
#else
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
#endif
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        {
          *esp = PHYS_BASE;
          // ADD 开始填栈
          // 1. 首先是argv里面的内容  
          int i = argc;
          // 1.0记录放到栈里之后首部的地址，以便第三步使用
          uint32_t * arr[argc];
          while (--i >= 0)
            {
              *esp -= sizeof (char) * (strlen(argv[i]) + 1);
              arr[i] = (uint32_t *) *esp;
              memcpy(*esp, argv[i], strlen(argv[i]) + 1);    
            }
          while ((int)(*esp) % 4) {
            (*esp)--;
          }
          *esp  -= 4;  
          // 2. 然后是一个占位的0
          (*(int *)(*esp)) = 0;
          // 3. 然后是argv[]的地址
          i = argc;
          while (--i >= 0) 
            {
              *esp -= 4;
              (*(uint32_t **)(*esp)) = arr[i];
            }
          // 4. 然后是**argv的地址
          *esp -= 4;
          (*(uint32_t **)(*esp)) = (*esp + 4);
          // 5. 最后是argc
          *esp -= 4;
          (*(int *)(*esp)) = argc;
          *esp -= 4;
        }
      else
#ifdef VM
        falloc_free_frame (kpage);
#else
        palloc_free_page (kpage);
#endif
    }
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
  bool success = (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
#ifdef VM
  vm_spte *spte = vm_spte_create_for_stack (((uint8_t *) PHYS_BASE) - PGSIZE);
  struct thread *cur_thread = thread_current ();
  success = vm_spt_insert (cur_thread->spt, spte);
#endif
  return success;
}

void
parse_command_args (char *cmd_line, int *argc, char **argv)
{
  char *token, *save_ptr;
  *argc = 0;
  for (token = strtok_r (cmd_line, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr))
    {
      argv[*argc] = malloc (strlen (token) + 1);
      strlcpy (argv[*argc], token, strlen(token)+1);
      (*argc)++;
    }
}


//ADD
struct file *
process_get_file (int fd) 
{
  if (fd >= FD_START && fd <= FD_END)
    return thread_current () -> process ->file_descriptor_table[fd];
  else return NULL;
}

int 
process_add_file (struct file * file) 
{
  if (file == NULL) 
    return -1;
  struct process *p = thread_current ()->process;
  int i = FD_START;
  while (i <= FD_END) 
    {
      if (p->file_descriptor_table[i] == NULL) {
        p->file_descriptor_table[i] = file;
        return i;
      }
      i++;
    }
}

void 
process_remove_file (struct file * file) 
{
  struct process *p = thread_current ()->process;
  int i = FD_START;
  while (i <= FD_END) 
    {
      if (p->file_descriptor_table[i] == file) {
        p->file_descriptor_table[i] = NULL;
        return;
      }
      i++;
    }
}

#ifdef VM


void 
process_munmap_file (map_file *mf)
{
  if (mf == NULL) return;
  struct thread *cur_thread = thread_current ();
  struct process *cur_process = cur_thread->process;
  struct file *target_file = mf->file;
  uint8_t *upage = mf->start_upage;
  off_t ofs = 0;
  uint32_t read_bytes = file_length (target_file);
  uint32_t write_bytes = 0;
  file_seek (target_file, ofs);
  while (write_bytes < read_bytes) 
    {
      size_t page_write_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      vm_spte *spte = vm_spt_find (cur_thread->spt, upage);
      if (spte == NULL) continue; // 不可能
      if (pagedir_is_dirty (cur_thread->pagedir, upage))
        {
          file_write_at (target_file, upage, page_write_bytes, ofs);
        }
      write_bytes += page_write_bytes;
      upage += PGSIZE;
      ofs += page_write_bytes;
    }
  file_close (target_file);
  // 清除map_file_entry,
  // 不检查返回值,因为如果调用hash_destory就已经删除
  map_file *tmp = malloc (sizeof (map_file));
  tmp->mapid = mf->mapid;
  hash_delete (cur_process->map_files, &tmp->elem);
  free (mf);
  free (tmp);
  return;
}

// 哈希函数，根据FTE的kpage来hash
unsigned
mapfile_hash_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  return hash_bytes (e, sizeof (struct hash_elem *));
}

// 比较函数
bool 
mapfile_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  map_file *mfa = hash_entry (a, map_file, elem);
  map_file *mfb = hash_entry (b, map_file, elem);
  return mfa->mapid < mfb->mapid;
}

// 销毁哈希表的元素（UNMAP所有文件）
void 
mapfile_hash_destory_func (struct hash_elem *e, void *aux UNUSED)
{
  // 此时elem对应的map_file已经从表里删除了
  map_file *mf = hash_entry (e, map_file, elem);
  process_munmap_file (mf);
  return;
}

#endif

//END

