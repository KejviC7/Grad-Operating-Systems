#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

int processCounterID = 1;
int pidMap[10000] = {-1};

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
	if(processCounterID == 1){
		for(int i = 0; i < 10000; i++){
			pidMap[i] = -1;
		}
	}
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // NEW CODE
  // Allocate current ticks to ctime as creation time when process is allocated. 
  // Set rtime (running time to 0)
  p->ctime = ticks;
  p->rtime = 0;

  #ifdef PRIORITY
    p->priority = 8;
  #endif

	cprintf("Process (pid:%d) created\n",processCounterID);
	if(pidMap[processCounterID] != -1){
		p->priority = pidMap[processCounterID];
	}
	processCounterID++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  // NEW CODE
  // Allocate current ticks to ctime as creation time when process is allocated. 
  // Set rtime (running time to 0)
  //p->ctime = ticks;
  //p->rtime = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0;  // beginning of initcode.S

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
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
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

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{		
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;
  
  //Temp Code
	//cprintf("exiting a process\n");
	//uint xticks;
	//acquire(&tickslock);
	//xticks = ticks;
	//release(&tickslock);
	//cprintf("Time terminated: (pid:%d) %d\n",curproc->pid,xticks);
	//cprintf("Time created: (pid:%d) %d\n",curproc->pid,curproc->ctime);
	//cprintf("Time running: (pid:%d) %d\n",curproc->pid,curproc->rtime);
	/////Temp code ends

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
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
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
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
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

void
scheduler(void)
{
  struct proc *p;
  struct proc *tempPointer; //NEW
  struct cpu *c = mycpu();
  c->proc = 0;
  int min_ctime; //NEW
  int max_priority; //NEW
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    //while(1){ // Loop until we have accessed all the RUNNABLE processes. //NEW
    tempPointer = ptable.proc; // Only to prevent compile errors //NEW
    min_ctime = -1;//NEW
    max_priority = -1; //NEW
    
    // Just to use the variables. Was throwing that warning of unuse variables before. 
    if (min_ctime) {
      min_ctime = min_ctime;
    } 
    if (tempPointer) {
      tempPointer = ptable.proc;
    }
    if (max_priority) {
      max_priority = -1;
    }

      
    #ifdef DEFAULT
      // DEFAULT SCHEDULER WILL BE USED (RR)
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE)
          continue;
          
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    #else
    #ifdef FIFO
		//cli(); // Stop interrupts
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->state == RUNNABLE){
				if(p->ctime < min_ctime || min_ctime == -1){ // Check to see if it arrived before the current temp proc. //NEW
					min_ctime = p->ctime; //NEW
					tempPointer = p; //NEW
				} 
			}
		}
			
		p = tempPointer; // Execute the proc that arrived first
		
		// Switch to chosen process.  It is the process's job
		// to release ptable.lock and then reacquire it
		// before jumping back to us.
		c->proc = p;
		switchuvm(p);
		p->state = RUNNING;
		
		swtch(&(c->scheduler), p->context);
		switchkvm();
		// Process is done running for now.
		// It should have changed its p->state before coming back.
		c->proc = 0;
    #else
    #ifdef PRIORITY
      // PRIORITY passed to SCHEDULER flag. PRIORITY Scheduling will be used
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == RUNNABLE) {
			if(p->priority < max_priority || max_priority == -1){ // A priority of a higher level was found.
				min_ctime = p->ctime;
				max_priority = p->priority;
				tempPointer = p;
			}
			else if(p->priority == max_priority) { // A priority of the same level was found.
				if(p->ctime < min_ctime || min_ctime == -1){ // Theoredically don't need the the "min_ctime == -1" part.
					max_priority = p->priority;
					tempPointer = p;
				}
			}
        }
      }
      p = tempPointer; // Execute the proc that arrived first
      
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0; 
  
    #endif
    #endif
    #endif
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
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
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
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// The function will set a new priority for the current process.
int
set_sched_priority(int priority)
{
	struct proc *curproc = myproc();
	curproc->priority = priority;
	cprintf("Priority of PID: %d is now set to new priority: %d\n",curproc->pid, priority);
	return 0;
}

// The function will set a new priority for the current process.
int
set_priority_dynamic(int priority, int pid)
{
	pidMap[pid] = priority;
	struct proc *p;
  acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) { // First we find the process by PID 
		if(p->pid == pid) {
			// Change the priority of that process
			p->priority = priority;
      cprintf("Priority of PID: %d is now set to new priority: %d\n",pid, priority);
	  }
  }
  release(&ptable.lock);
  return 0;
	
}

// The function will return the current priority of a given PID.
int
get_sched_priority(int pid)
{
	struct proc *p;
	//sti();
	int priority = -999; // DEFAULT value 
	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) { // First we find the process by PID and make sure it's in the queue, meaning it's Running, runnable, or sleeping.
		if(p->pid == pid) {
			//cprintf("PID: %d State: %d ctime: %d rtime: %d\n", p->pid, p->state, p->ctime, p->rtime);
			priority = p->priority;
	}
	 }
	release(&ptable.lock);
  if (priority == -999) {
    cprintf("Process doesn't exist or is not in the scheduling queue!\n");
    return -1;
  } else {
	  cprintf("Priority of PID: %d is %d\n",pid, priority);
  }
  return priority;
}

// The function requested in the assingment for a FIFO scheduler will return the position in queue for a given pid.
int
fifo_position(int pid)
{
	struct proc *p;
	struct proc *tempP = ptable.proc;
	int foundCondition = 0;
	//int counter =0;
	acquire(&ptable.lock);
	//cprintf("name \t pid \t state \t ctime \t rtime \t \n");
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) { // First we find the process by PID and make sure it's in the queue, meaning it's Running, runnable, or sleeping.
		if(p->pid == pid) {
			// We have found a match of the provided PID in the process table
			if(p->state == RUNNABLE) {
				//cprintf("PID: %d \n", p->pid);
				foundCondition = 1;
				break;
			}
		} 
	}
	if(foundCondition == 0){
		release(&ptable.lock);
		return -1; // Not found. Or does not exist in a runnable state.
	}
	tempP = p;
	int position_count = 1; // 1 means it's next to run, meaning the next in line to execute. Last being number of processes.
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) { // Now assuming we made it here we have found the process and it's running. We find it's position in the FIFO.
		if(p->pid != pid && p->ctime < tempP->ctime) {
			// We have found another process in the queue.
			if(p->state == RUNNABLE) {
				//cprintf("PID: %d \n", p->pid);
				position_count++;
			}
		} 
	}
	release(&ptable.lock);
	cprintf("Position in FIFO: %d\n",position_count);
	return position_count;
}

//
int
time_scheduled(int pid)
{
	struct proc *p;
	//int counter =0;
	acquire(&ptable.lock);
	//cprintf("name \t pid \t state \t ctime \t rtime \t \n");
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
		if(p->pid == pid) {
			cprintf("PID: %d State: %d ctime: %d rtime: %d\n", p->pid, p->state, p->ctime, p->rtime);
			// We have found a match of the provided PID in the process table
			if(p->state == RUNNING) {
				//cprintf("PID: %d \n", p->pid);
				release(&ptable.lock);
				return p->rtime;
			}
			else if (p->state == RUNNABLE || p->state == SLEEPING) {
				release(&ptable.lock);
				return 0;
			} 
		} 
	}
	release(&ptable.lock);
	return -1; 
}

//
int
process_table()
{
  struct proc *p;
  //char *state_name = " ";
  //int counter =0;
  acquire(&ptable.lock);
  cprintf("priority pid \t ctime \t rtime \t state \t\t name\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED) {
      continue;
    } else {
      if (p->state == 2) {
        //safestrcpy(state_name, "SLEEPING", sizeof(state_name));
        cprintf("%d \t %d \t %d \t %d \t SLEEPING \t %s\n", p->priority, p->pid, p->ctime, p->rtime, p->name);
      } else if (p->state == 3) {
        //safestrcpy(state_name, "RUNNABLE", sizeof(state_name));
        cprintf("%d \t %d \t %d \t %d \t RUNNABLE \t %s\n", p->priority, p->pid, p->ctime, p->rtime, p->name);
      } else if (p->state == 4) {
        //safestrcpy (state_name, "RUNNING", sizeof(state_name));
        cprintf("%d \t %d \t %d \t %d \t RUNNING \t %s\n", p->priority, p->pid, p->ctime, p->rtime, p->name);
      } else if (p->state == 5) {
        //safestrcpy(state_name, "ZOMBIE", sizeof(state_name));
        cprintf("%d \t %d \t %d \t %d \t ZOMBIE \t %s\n", p->priority, p->pid, p->ctime, p->rtime, p->name);
      } 
      //cprintf("%d \t %s \t %d \t %d \t $s\n", p->pid, state_name, p->ctime, p->rtime, p->name);
    }
  }
  release(&ptable.lock);
  return 0;
}

void 
increment_rtime() {
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == RUNNING) {
      p->rtime++;
    } 
  } 
  release(&ptable.lock);
}


// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
