#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#ifndef USERPROG
#define USERPROG
#endif

#include "threads/thread.h"

typedef int pid_t;

//ADD 在为用户进程时，为struct thread添加进程信息
struct process
{
  pid_t pid;                          /* Process identifier. */
  struct semaphore init_process_sema;   //用于父进程等待子进程创建完毕
  char *cmd_line;
  struct list_elem child_elem;
  struct file *file_descriptor_table[128]; /* 需要储存每个线程的file */
  struct semaphore exit_sema;         /* 程序退出型号量，用于父子进程同步 */
  int exit_status;                    /* 程序退出状态码 */
  struct file *executing_file;        /* 本进程正在使用的文件 */
  struct lock ensure_once_wait;
};

#define FD_START 3                  //fd起始值
#define FD_END   127                //fd终止值

//END


pid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

//ADD
struct file *process_get_file (int);
int process_add_file (struct file * file);
void process_remove_file (struct file * file);
//END

#endif /* userprog/process.h */

