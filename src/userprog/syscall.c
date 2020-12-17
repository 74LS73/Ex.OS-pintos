#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void syscall_handler (struct intr_frame *);
//ADD
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static void access_invalid_uaddr (void);
static void check_uaddr (const uint8_t *uaddr); 
static void check_uaddr_size (const uint8_t *uaddr, size_t size);

struct lock filesys_lock;
//END

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  //ADD
  lock_init (&filesys_lock);
  //END
}

static void
syscall_handler (struct intr_frame *f) 
{
  // 根据lib/user/syscall.c 此时esp指向的值中储存了
  // 需要调用的系统调用的编号，同时，返回值储存在eax中
  int syscall_number;
  // 内核态，记录esp，以供exception使用
  thread_current ()->esp = f->esp;
  get_user_value(f->esp, &syscall_number, sizeof (syscall_number));
  switch (syscall_number)
    {
    case SYS_HALT:
      {
        f->eax = shutdown_power_off ();
        break;
      }
    case SYS_EXIT:
      {
        int status;
        get_user_value(f->esp + 4, &status, sizeof (status));
        sys_exit (status);
        break;
      }
    case SYS_EXEC:
      {
        lock_acquire (&filesys_lock);
        const char *cmd_line;
        get_user_value(f->esp + 4, &cmd_line, sizeof (cmd_line));
        check_uaddr_size (cmd_line,1);
        f->eax = process_execute (cmd_line);
        lock_release (&filesys_lock);
        break;
      }
    case SYS_WAIT:
      {
        tid_t child_tid;
        get_user_value(f->esp + 4, &child_tid, sizeof (child_tid));
        f->eax = process_wait (child_tid);
        break;
      }
    case SYS_CREATE:
      {
        lock_acquire (&filesys_lock);
        const char *name;
        off_t initial_size;
        get_user_value(f->esp + 4, &name, sizeof (name));
        check_uaddr (name);
        get_user_value(f->esp + 8, &initial_size, sizeof (initial_size));
        f->eax = filesys_create (name, initial_size);
        lock_release (&filesys_lock);
        break;
      }
    case SYS_REMOVE:
      {
        lock_acquire (&filesys_lock);
        const char *name;
        get_user_value(f->esp + 4, &name, sizeof (name));
        check_uaddr (name);
        f->eax = filesys_remove (name);
        lock_release (&filesys_lock);
        break;
      }
    case SYS_OPEN:
      {
        lock_acquire (&filesys_lock);
        const char *name;
        get_user_value (f->esp + 4, &name, sizeof (name));
        check_uaddr (name);
        struct file *target_file = filesys_open (name);
        f->eax = process_add_file (target_file);
        lock_release (&filesys_lock);
        break;
      }
    case SYS_FILESIZE:
      {
        int fd;
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        struct file *target_file = process_get_file (fd);
        f->eax = file_length (target_file); 
        break; 
      }  
    case SYS_READ:
      {
        lock_acquire (&filesys_lock);
        int fd;
        const void *buffer;
        unsigned size;
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        get_user_value (f->esp + 8, &buffer, sizeof(buffer));
        get_user_value (f->esp + 12, &size, sizeof(size));
        check_uaddr_size (buffer, size);
        if (fd == 0) 
         {
           int i;
           for (i = 0; i < size; ++i)
             {
               if (!put_user (buffer + i, input_getc ()))
                 {
                   lock_release (&filesys_lock);
                   sys_exit (-1);
                 }
             }
           f->eax = size;
           break;
         }
        else
          {
            struct file *target_file = process_get_file (fd);
            if (target_file)
              f->eax = file_read (target_file, buffer, size); 
            else
              f->eax = -1;
          }

        lock_release (&filesys_lock);
        break; 
      }
    case SYS_WRITE:
      {
        lock_acquire (&filesys_lock);
        int fd;
        const void *buffer;
        unsigned size;
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        get_user_value (f->esp + 8, &buffer, sizeof(buffer));
        get_user_value (f->esp + 12, &size, sizeof(size));
        check_uaddr_size (buffer, size);
        if (fd == 1) 
          {
            putbuf (buffer, size);
            f->eax = size;
          } 
        else
          {
            struct file *target_file = process_get_file (fd);
            if (target_file)
              f->eax = file_write (target_file, buffer, size);
            else
              f->eax = -1;
          }
        lock_release (&filesys_lock);
        break;
       }
    case SYS_SEEK:
      {
        int fd;
        off_t new_pos;
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        get_user_value (f->esp + 8, &new_pos, sizeof (new_pos));
        struct file *target_file = process_get_file (fd);
        file_seek (target_file, new_pos);
        break; 
      }
    case SYS_TELL:
      {
        int fd;
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        struct file *target_file = process_get_file (fd);
        f->eax = file_tell (target_file);
        break; 
      }  
    case SYS_CLOSE:
      {
        int fd;
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        struct file *target_file = process_get_file (fd);
        process_remove_file (target_file);
        file_close (target_file);
        break; 
      }
    case SYS_MMAP:
      {
        int fd; 
        get_user_value (f->esp + 4, &fd, sizeof (fd));
        void *upage; 
        get_user_value (f->esp + 8, &upage, sizeof (upage));
        f->eax = sys_mmap (fd, upage);
        break;
      }
    case SYS_MUNMAP:
      {
        mapid_t mapid; 
        get_user_value (f->esp + 4, &mapid, sizeof (mapid));
        sys_mummap (mapid);
        break;
      }
    default:
      {
        sys_exit (-1);
        break;
      }
    }
  // sys_exit (-1);
}

int 
sys_exit (int status) 
{
  struct thread* cur = thread_current ();
  cur->process->exit_status = status;
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit (); 
}

int
sys_mmap (int fd, void *upage)
{
  lock_acquire (&filesys_lock);
  if (fd == 0 || fd == 1) { goto SYS_MMAP_FAIL; } // fd不能为0/1
  if (pg_ofs (upage) != 0) { goto SYS_MMAP_FAIL; } // upage必须是页面起始地址
  void *start_upage = upage;
  struct file *target_file = process_get_file (fd);
  if (target_file == NULL) { goto SYS_MMAP_FAIL; } 
  struct thread *cur_thread = thread_current ();
  off_t ofs = 0;
  uint32_t read_bytes = file_length (target_file);
  if (read_bytes == 0) { goto SYS_MMAP_FAIL; } // 文件长度不能为0
  file_seek (target_file, ofs);
  while (read_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      // Lazy load
      vm_spte *spte = vm_spte_create_for_file 
        (upage, target_file, ofs, page_read_bytes, page_zero_bytes, true);
      if (!vm_spt_insert (cur_thread->spt, spte)) 
        { free (spte); goto SYS_MMAP_FAIL; }
      read_bytes -= page_read_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }
  struct process *cur_process = cur_thread->process;
  map_file *mfile = malloc (sizeof (map_file));
  mfile->file = target_file;
  mfile->start_upage = start_upage;
  mfile->mapid = (mapid_t) (&mfile->elem);
  hash_insert (cur_process->map_files, &mfile->elem);
  
  lock_release (&filesys_lock);
  return mfile->mapid;
SYS_MMAP_FAIL:
  lock_release (&filesys_lock);
  return -1;
}

void
sys_munmap (mapid_t mapid)
{
  lock_acquire (&filesys_lock);
  struct thread *cur_thread = thread_current ();
  struct process *cur_process = cur_thread->process;
  struct hash_elem *e = hash_find 
            (cur_process->map_files, (struct hash_elem *) mapid);
  if (e == NULL) goto SYS_MUNMAP_END;
  map_file *mf = hash_entry (e, map_file, elem);
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
SYS_MUNMAP_END:
  lock_release (&filesys_lock);
  return;
}


static void 
check_uaddr (const uint8_t *uaddr) 
{
  if (get_user (uaddr) == -1) 
    access_invalid_uaddr ();
}

/* [uaddr, uaddr+size]的检查
*/
static void 
check_uaddr_size (const uint8_t *uaddr, size_t size) 
{
  check_uaddr (uaddr);
  check_uaddr (uaddr + size);
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  if (!is_user_vaddr (uaddr)) 
    return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

int
get_user_value (const uint8_t *uaddr, const void* value, size_t size)
{
  int i;
  for (i = 0; i < size; i++)
    {
      // 这里ch必须是int, 才能特判0xffffffff的情况
      // char就不行. 
      int ch = get_user (uaddr + i);
      if (ch == -1)
        access_invalid_uaddr ();
      *(char *)(value + i) = ch & 0xff;
    }
  return 0;
}

static void
access_invalid_uaddr (void)
{
  if (lock_held_by_current_thread(&filesys_lock))
    lock_release (&filesys_lock);
  sys_exit (-1);
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  if (!is_user_vaddr (udst)) 
    return false;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

