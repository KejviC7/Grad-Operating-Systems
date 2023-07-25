#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	if(argc < 2){
		printf(2, "Need to add the file descriptor and the offset\n");
		exit();
	}
	
	int result = lseek(atoi(argv[1]), atoi(argv[2]));
	if(result < 0){
		printf(2, "lseek: At fd: %d failed to point to offset %d\n", argv[1],argv[2]);
		exit();
	}
	printf(2, "fd %d now has offset at %d\n", atoi(argv[1]), result);
	exit();
}