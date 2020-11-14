#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // 根据lib/user/syscall.c 此时esp指向的值中储存了
  // 需要调用的系统调用的编号，同时，返回值储存在eax中
  struct file * target_file;
  switch (*((int *) f->esp))
    {
    case SYS_HALT:
      f->eax = shutdown_power_off ();
      break;
    case SYS_EXIT:
      {
        int status;
        get_user_value(f->esp + 4, &status, sizeof (status));
        printf("%s: exit(%d)\n", thread_current()->name, status);
        thread_exit (); 
        break;
      }
    case SYS_EXEC:
      f->eax = process_execute (f->esp + 4);
      break;
    case SYS_WAIT:
      /* code */
      break;
    case SYS_CREATE:
      f->eax = filesys_create (f->esp + 4, *((int *)(f->esp + 8)));
      break;
    case SYS_REMOVE:
      f->eax = filesys_remove (f->esp + 4);
      break;
    case SYS_OPEN:
      target_file = filesys_open (f->esp + 4);
      f->eax = thread_add_file (target_file);
      break;
    case SYS_FILESIZE:
      target_file = thread_get_file (*(int *)(f->esp + 4));
      f->eax = file_length (target_file); 
      break; 
    case SYS_READ:
      {
        int fd;
        fd = *(int *)(f->esp + 4);
        if (fd == 0) 
         {
           input_getc ();
           break;
         }
        target_file = thread_get_file (fd);
        f->eax = file_read (target_file, f->esp + 8, *(int *)(f->esp + 12)); 
        break; 
      }
    case SYS_WRITE:
      {
        int fd;
        const void *buffer;
        unsigned size;
        get_user_value(f->esp + 20, &fd, sizeof (fd));
        get_user_value(f->esp + 24, &buffer, sizeof(buffer));
        get_user_value(f->esp + 28, &size, sizeof(size));
        if (fd == 1) 
          {
            putbuf (buffer, size);
          } 
        else
          {
            target_file = thread_get_file (fd);
            f->eax = file_write (target_file, f->esp + 6, *(int *)(f->esp + 12));
            
          }
        break;
       }
    case SYS_SEEK:
      target_file = thread_get_file (*(int *)(f->esp + 4));
      file_seek (target_file, *(int *)(f->esp + 8));
      break; 
    case SYS_TELL:
      target_file = thread_get_file (*(int *)(f->esp + 4));
      f->eax = file_tell (target_file);
      break; 
    case SYS_CLOSE:
      target_file = thread_get_file (*(int *)(f->esp + 4));
      thread_remove_file (target_file);
      file_close (target_file);
      break; 
    default:
      break;
    }
  // thread_exit ();
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
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
      *(char *)(value + i) = get_user (uaddr + i) & 0xff;
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