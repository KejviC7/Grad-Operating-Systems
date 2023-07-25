#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
	if(argc < 1){
		printf(2, "Need to provide path\n");
		exit();
	}
	
	int result = createExtent(argv[1]);
	if(result < 0){
		printf(2, "extent based file failed\n");
		exit();
	}
	printf(2, "extent based file %s created\n", argv[1]);
	exit();
}
