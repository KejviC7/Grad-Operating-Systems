#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int priority, pid;
  if(argc < 2) {
    printf(2, "You need to provide the new Priority value you want to set. \n");
    exit();
  } else if (argc == 2) {
    // Perform regular functionality and set the new priority of the current process with set_priority() system call
    priority = atoi(argv[1]);
    set_sched_priority(priority);
  } else if (argc == 3) {
    // Extra functionality which allows you to set the new priority for a given PID with set_priority_dynamic() system call
    priority = atoi(argv[1]);
    pid = atoi(argv[2]);
    set_priority_dynamic(priority, pid);
  }
  
  exit();
}

