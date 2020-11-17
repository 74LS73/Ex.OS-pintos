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
static void check_uaddr (const uint8_t *uaddr); 
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
  if (f->esp < 0x08048000) 
    {
      sys_exit (-1);
      return;
    }
  switch (*((int *) f->esp))
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
        printf("%s: exit(%d)\n", thread_current()->name, status);
        thread_exit (); 
        break;
      }
    case SYS_EXEC:
      {
        lock_acquire (&filesys_lock);
        const char *cmd_line;
        get_user_value(f->esp + 4, &cmd_line, sizeof (cmd_line));
        f->eax = process_execute (cmd_line);
        lock_release (&filesys_lock);
        break;
      }
    case SYS_WAIT:
      {
        tid_t child_tid;
        get_user_value(f->esp + 4, &child_tid, sizeof (child_tid));
        process_wait (child_tid);
        break;
      }
    case SYS_CREATE:
      {
        lock_acquire (&filesys_lock);
        char *name;
        off_t initial_size;
        get_user_value(f->esp + 4, &name, sizeof (name));
        get_user_value(f->esp + 8, &initial_size, sizeof (initial_size));
        if (strlen (name) == 0)
          f->eax = -1;
        else 
          f->eax = filesys_create (name, initial_size);
        lock_release (&filesys_lock);
        break;
      }
    case SYS_REMOVE:
      f->eax = filesys_remove (f->esp + 4);
      break;
    case SYS_OPEN:
      {
        const char *name;
        get_user_value (f->esp + 4, &name, sizeof (name));
        struct file *target_file = filesys_open (name);
        if (target_file == NULL) 
          f->eax = -1;
        else 
          f->eax = thread_add_file (target_file);
        break;
      }
    case SYS_FILESIZE:
      {
        struct file *target_file = thread_get_file (*(int *)(f->esp + 4));
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
        if (fd == 0) 
         {
           input_getc ();
           break;
         }
        struct file *target_file = thread_get_file (fd);
        f->eax = file_read (target_file, buffer, size); 
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
        if (fd == 1) 
          {
            putbuf (buffer, size);
          } 
        else
          {
            struct file *target_file = thread_get_file (fd);
            f->eax = file_write (target_file, buffer, size);
          }
        lock_release (&filesys_lock);
        break;
       }
    case SYS_SEEK:
      {
        struct file *target_file = thread_get_file (*(int *)(f->esp + 4));
        file_seek (target_file, *(int *)(f->esp + 8));
        break; 
      }
    case SYS_TELL:
      {
        struct file *target_file = thread_get_file (*(int *)(f->esp + 4));
        f->eax = file_tell (target_file);
        break; 
      }  
    case SYS_CLOSE:
      {
        const char *name;
        get_user_value (f->esp + 4, &name, sizeof (name));
        struct file *target_file = thread_get_file (name);
        thread_remove_file (target_file);
        file_close (target_file);
        break; 
      }
    default:
      {
        sys_exit (-1);
        break;
      }
    }
  // thread_exit ();
}

int 
sys_exit (int status) 
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit (); 
}

static void 
check_uaddr (const uint8_t *uaddr) 
{
  if (!is_user_vaddr (uaddr)) 
  {
    sys_exit (-1);
    return;
  }
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  check_uaddr (uaddr);
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
      char ch = get_user (uaddr + i);
      *(char *)(value + i) = ch;
    }
  return 0;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
