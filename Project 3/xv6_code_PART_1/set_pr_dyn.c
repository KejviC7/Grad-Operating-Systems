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
    priority = atoi(argv[1]);
    set_sched_priority(priority);
    exit();
  } else if (argc == 3) {
    priority = atoi(argv[1]);
    pid = atoi(argv[2]);
    set_priority_dynamic(priority, pid);
  }
  
  exit();
}


