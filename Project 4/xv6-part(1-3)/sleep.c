// This is a simple sleep function. 
#include "types.h"
#include "stat.h"
#include "user.h"


void sleep_c(int n)
{
  // Perform sleep system call
  //printf(2, "n val: %d", n);
  printf(2, "Nothing happens for a little while...\n");
  sleep(n);
}


int main(int argc, char *argv[])
{
  int tick_count;

  if (argc<2) {
    printf(2, "Error: You need to provide the tick amount!\n");
    exit();
  }

 
  // Get the tick amount (It will be the second parameter passed)
  tick_count = atoi(argv[1]);
  //printf(2, "Tick count provide: %d", tick_count);
  // Call the sleep_c func and pass the tick_count parameter
  sleep_c(tick_count);

  exit();
  
}
	
