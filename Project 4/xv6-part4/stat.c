#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

int
main(int argc, char *argv[])
{
    int flags = O_RDONLY;
    int fd;
    struct stat st; 
	if(argc < 1){
		printf(2, "Need to provide path\n");
		exit();
	}

    // Open File
    fd = open(argv[1], flags);
    if (fd < 0) {
        printf(2, "Failed to open file %s\n", argv[1]);
        exit();
    }

    // Get File Stat
    if (fstat(fd, &st) < 0) {
        printf(2, "Failed to get file %s's status. \n");
        exit();
    } 


    // Print the data
    printf(1, "->File name: %s\n", argv[1]);
    printf(1, "->File type: %d\n", st.type);
    if (st.type == 5) {
        printf(1, "->File type is: T_EXTENT\n");
    } else if (st.type == 2) {
        printf(1, "->File type is: T_FILE \n");
    }
    printf(1, "->File size: %d\n", st.size);
    printf(1, "->File block size: %d\n", BSIZE);

    // This information is only relevant for extent-based files so if it not an extent-based file but rather a pointer-based on don't print it
    if (st.type == 5) {
        printf(1, "->File extent count: %d\n", st.n_extents);
        for (int i = 0; i < st.n_extents; i++) {
            printf(1, "-->Extent %d: start addr %d, length %d\n", i, st.extent_addr[i], st.extent_len[i]);
        }
    } else if (st.type == 2) {
        for (int i =0; i < 13; i++) {
            printf(1, "-->Pointer %d's addr: %d\n", i, st.ptr_addrs[i]);
        }
    }
    // Close file
    close(fd);
	exit();
}
