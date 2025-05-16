#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#ifndef ALPHA
#define ALPHA 1
#endif

#ifndef BETA
#define BETA 13
#endif

int last_running = -1;

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p->arrival_time = ticks;
  p->completion_time = -1;
  p->running_time = 0;
  p->last_start_running = -1;
  p->response_time = -1;
  p->context_switch = 0;
  p->sighandler = 0;
  p->sig_handled = 0;
  p->forced_sleep = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  // p->sighandler = 0;

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

int custom_fork(int start_later_flag, int exec_time)
{
  // if (argint(0, &start_later) < 0 || argint(1, &exec_time) < 0)
  //     return -1;  // Invalid arguments

  if (start_later_flag < 0 || exec_time < 0)
  {
    return -1;
  }

  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    // np->start_later = start_later_flag;
    // np->exec_time = exec_time;
    // np->start_ticks = 0;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  np->start_later = start_later_flag;
  np->exec_time = exec_time;
  np->start_ticks = ticks;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  if (start_later_flag == 0)
  {
    np->state = RUNNABLE;
  }

  release(&ptable.lock);
  // cprintf("Inside custom fork with %d %d %d\n",np->pid, np->start_later, np->exec_time);
  return pid;
}

int scheduler_start(void)
{
  struct proc *p;
  // cprintf("Inside scheduler_start\n");
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->start_later && p->state == EMBRYO)
    {
      p->state = RUNNABLE;
      p->start_ticks = ticks; // Set the start time
    }
  }
  release(&ptable.lock);
  return 0;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  curproc->completion_time = ticks;

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  print_metrics(curproc);
  sched();
  panic("zombie exit");
}

int print_metrics(struct proc *p)
{
  cprintf("PID: %d\n", p->pid);
  cprintf("TAT: %d\n", p->completion_time - p->arrival_time);
  cprintf("WT: %d\n", p->completion_time - p->arrival_time - p->running_time);
  cprintf("RT: %d\n", p->response_time);
  if (p->context_switch - 1 < 0)
  {
    cprintf("#CS: %d\n", 0);
  }
  else
  {
    cprintf("#CS: %d\n", p->context_switch - 1);
  }

  cprintf("$ ");

  // cprintf("PID: %d\n", p->pid);
  // cprintf("Arrival Time: %d\n", p->arrival_time);
  // cprintf("Completion Time: %d\n", p->completion_time);
  // cprintf("Running Time: %d\n", p->running_time);
  // cprintf("Last Start Running: %d\n", p->last_start_running);
  // cprintf("Response Time: %d\n", p->response_time);
  // cprintf("Context Switches: %d\n", p->context_switch);

  return 0;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  int all_frozen = 1;
  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;

      if (p->state == RUNNABLE && p->forced_sleep == 0)
        all_frozen = 0;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    if (havekids && all_frozen)
    {
      // If we have children but they're all frozen,
      // allow the wait call to return to the shell
      release(&ptable.lock);
      return -2; // Special return code to indicate frozen children
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.

// void scheduler(void)
// {
//   struct proc *p;
//   // struct proc *bestP = 0;
//   struct cpu *c = mycpu();
//   c->proc = 0;
//   for (;;)
//   {

//     sti();

//     // Loop over process table looking for process to run.
//     acquire(&ptable.lock);
//     for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
//     {
//       if (p->state != RUNNABLE)
//         continue;

//       if (p->forced_sleep)
//       {
//         continue;
//       }

//       // Switch to chosen process.  It is the process's job
//       // to release ptable.lock and then reacquire it
//       // before jumping back to us.
//       c->proc = p;
//       switchuvm(p);
//       p->state = RUNNING;

//       swtch(&(c->scheduler), p->context);
//       switchkvm();

//       // Process is done running for now.
//       // It should have changed its p->state before coming back.
//       c->proc = 0;
//     }
//     release(&ptable.lock);

//     // sti();
//     // acquire(&ptable.lock);
//     // int best_score = -(1<<10);
//     // for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//     //   if(p->state != RUNNABLE){
//     //     continue;
//     //   }

//     //   if(p->forced_sleep){
//     //     continue;
//     //   }

//     //   if (p->start_later && p->exec_time > 0 && ticks - p->start_ticks >= p->exec_time) {
//     //       p->start_later = 0;
//     //       continue;
//     //   }
//     //   if(bestP != 0){
//     //     int score_p = -ALPHA*(p->running_time) + BETA*(ticks - p->arrival_time - p->running_time);
//     //     if(score_p > best_score){
//     //       bestP = p;
//     //       best_score = score_p;
//     //     }
//     //   }
//     //   else{
//     //     bestP = p;
//     //     best_score = -ALPHA*(bestP->running_time) + BETA*(ticks - bestP->arrival_time - bestP->running_time);
//     //   }
//     // }
//     // if(bestP != 0){
//     //     if(bestP->response_time == -1){
//     //       bestP->response_time = ticks - bestP->arrival_time;
//     //     }

//     //     bestP->last_start_running = ticks;
//     //     bestP->context_switch++;

//     //     c->proc = bestP;
//     // 		switchuvm(bestP);
//     // 		bestP->state = RUNNING;

//     // 		swtch(&(c->scheduler), bestP->context);
//     // 		switchkvm();

//     //     bestP->running_time += ticks - bestP->last_start_running;

//     // 		c->proc = 0;
//     // }
//     // release(&ptable.lock);
//   }
// }

void scheduler(void)
{
  struct proc *p;
  struct proc *bestP = 0;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    sti(); // Enable interrupts
    acquire(&ptable.lock);
    int best_score = -(1 << 10);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
      {
        continue;
      }

      if (p->forced_sleep)
      { // Skip frozen processes
        continue;
      }

      if (p->start_later && p->exec_time > 0 && ticks - p->start_ticks >= p->exec_time)
      {
        p->start_later = 0;
        continue;
      }

      int score_p = -ALPHA * (p->running_time) + BETA * (ticks - p->arrival_time - p->running_time);
      if (!bestP || score_p > best_score)
      {
        bestP = p;
        best_score = score_p;
      }

      if (bestP && bestP->state == RUNNABLE)
      {
        if (bestP->response_time == -1)
        {
          bestP->response_time = ticks - bestP->arrival_time;
        }

        bestP->last_start_running = ticks;
        if (bestP->pid != last_running)
        {
          bestP->context_switch++;
        }
        if ((bestP->pid != 1) & (bestP->pid != 2))
        {
          last_running = bestP->pid;
        }
        c->proc = bestP;
        switchuvm(bestP);
        bestP->state = RUNNING;

        swtch(&(c->scheduler), bestP->context);
        switchkvm();

        bestP->running_time += ticks - bestP->last_start_running;

        c->proc = 0;
      }
    }

    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void ctrlkill(void)
{
  struct proc *p;
  cprintf("\nCtrl-C is detected by xv6\n");
  acquire(&ptable.lock);

  struct proc *console_proc = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == 2)
    {
      console_proc = p;
      break;
    }
  }

  if (!console_proc)
  {
    release(&ptable.lock);
    cprintf("Console process not found!\n");
    return;
  }
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid > 2 && p->killed == 0)
    {
      // cprintf("Killing: %d %d\n", p->pid, p->killed);
      p->killed = 1;
      p->forced_sleep = 0;
      p->parent = console_proc;
      // cprintf("Killed: %d %d\n", p->pid, p->killed);
      release(&ptable.lock);
      wakeup(p);
      kill(p->pid);
      acquire(&ptable.lock);
    }
  }

  release(&ptable.lock);
  // procdump();
}

void ctrlfreeze(void)
{
  struct proc *p;

  cprintf("\nCtrl-B is detected by xv6\n");
  acquire(&ptable.lock); // Lock the process table

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid > 2) // Freeze processes with pid > 2
    {
      p->forced_sleep = 1;
      // cprintf("Freezing: %d %d\n", p->pid, p->forced_sleep);
      // p->state = SLEEPING; // Put process to sleep (freeze)
    }
  }
  release(&ptable.lock); // Unlock after modifying process states
}

void ctrlrestart(void)
{
  struct proc *p;
  cprintf("\nCtrl-F is detected by xv6\n");
  acquire(&ptable.lock); // Lock before modifying state
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    p->forced_sleep = 0;
  }
  release(&ptable.lock); // Unlock after chang
}

void registerctrl(void)
{
  struct proc *p = myproc(); // Get the current process

  // cprintf("%d %p\n", p->pid, p->sighandler);
  cprintf("\nCtrl-G is detected by xv6\n");
  if (p->sighandler) // If a handler is registered
  {
    // cprintf("Executing custom signal handler for process %d\n", p->pid);
    p->sig_handled = 1;
    p->tf->eip = (uint)p->sighandler;
  }
  else
  {
    cprintf("No signal handler registered. Ignoring SIGCUSTOM.\n");
  }
}

// int signal(void (*handler)(void)){
//   cprintf("Inside signal and %s\n", handler);
//   return 0;
// }