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
        check_uaddr (cmd_line);
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

static void 
check_uaddr (const uint8_t *uaddr) 
{
  if (get_user (uaddr) == -1) 
    access_invalid_uaddr ();
}

/* [uaddr, uaddr+size)的检查
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

