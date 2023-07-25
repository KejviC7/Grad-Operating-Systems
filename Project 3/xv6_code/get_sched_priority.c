#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  int i;

  if(argc < 2){
    printf(2, "Provide a PID to get the priority for the that PID.\n");
    exit();
  }
  for(i=1; i<argc; i++)
    get_sched_priority(atoi(argv[i]));
  exit();
}

