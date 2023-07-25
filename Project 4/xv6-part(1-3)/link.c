#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	if(argc < 2){
		printf(2, "Need to provide target and new path\n");
		exit();
	}
	
	int result = link(argv[1], argv[2]);
	if(result < 0){
		printf(2, "link failed\n");
		exit();
	}
	printf(2, "link successful \n", atoi(argv[1]), result);
	exit();
}
