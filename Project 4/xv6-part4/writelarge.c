#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"


int
main(int argc, char **argv)
{
  if(argc < 1){
		printf(2, "Need to provide size\n");
		exit();
	}
  
  struct stat st;
  int fd;
  char buf[BSIZE];
  //int off;
  char *filename = "bigfile.txt";
  int flags = O_CREATE | O_WRONLY;

  fd = open(filename, flags);
  if (fd < 0) {
    printf(2, "error: cannot create file\n");
    exit();
  }
  
  // Write 100 blocks of zeroes to the file
  for (int i = 0; i < atoi(argv[1] ); i++) {
    //off = i * BSIZE;
    if (write(fd, buf, BSIZE) != BSIZE) {
      printf(2, "error: write failed\n");
      exit();
    }
  }
  // Get the file information
  if (stat("bigfile.txt", &st) < 0) {
      printf(2, "error: cannot get file information\n");
      exit();
  }
  // Print the size of the file
  printf(1, "The size of bigfile.txt is %d bytes\n", st.size);

  // Print the size of the file in megabytes
  float mb_size = (float) st.size / (float) (BSIZE * 1024);
  printf(1, "The size of bigfile.txt is %d MB\n", mb_size);
  
  close(fd);

  exit();
  
}