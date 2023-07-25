#include "types.h"
#include "stat.h"
#include "user.h"

int main()
{
  int count = 5;
  int pid;
  int tempCounter = 100;
  for(int i = 0; i < count; i++) {
	pid = fork();
	for(double a = 0; a < 100000000; a = a + 0.5){
		tempCounter = tempCounter * 100;
		tempCounter = tempCounter / 100;
	}
  	if (pid < 0)
	   break;
	if (pid == 0){
		//Do something to envoke a context switch
		for(double a = 0; a < 100000000; a = a + 0.5){
			tempCounter = tempCounter * 100;
			tempCounter = tempCounter / 100;
		}
	   exit();
	}
  }
  wait();
  exit();   
}
