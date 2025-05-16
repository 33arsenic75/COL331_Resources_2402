#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "syscall.h"
// #include <stddef.h>  // For size_t
// #include <stdlib.h>  // For qsort


int sys_gethistory(void) {

    // qsort(history, history_count, sizeof(history[0]), compare_by_pid);
    for(int i = 0; i < history_count  ; i++){
      // if(history[i].done){
        cprintf("%d %s %d\n", history[i].pid, history[i].name  ,history[i].memory);
      // }
    }
    return 0;
}

int sys_block(void) {
    int syscall_id;
    if (argint(0, &syscall_id) < 0)
    return -1; // Invalid argument
    
    // cprintf("Inside sysblock with id %d\n", syscall_id);
    if (syscall_id < 0 || syscall_id >= MAX_SYSCALLS || syscall_id == SYS_fork || syscall_id == SYS_exit)
        return -1; // Prevent blocking essential syscalls
    
    struct proc *p = myproc();
    if(strncmp(p->name, "sh", 2) !=0 ){
      return -1;
    }
    p->blocked_syscalls_child[syscall_id] = 1;
    cprintf("syscall %d is blocked\n", syscall_id);
    return 0;
}

int sys_unblock(void) {
    int syscall_id;
    if (argint(0, &syscall_id) < 0)
        return -1;

    if (syscall_id < 0 || syscall_id >= MAX_SYSCALLS)
        return -1;

    struct proc *p = myproc();
        if(strncmp(p->name, "sh", 0) !=0 ){
      return -1;
    }
    p->blocked_syscalls_child[syscall_id] = 0;
    cprintf("syscall %d is unblocked\n", syscall_id);
    return 0;
}


int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
