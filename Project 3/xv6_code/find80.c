#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int compareFile(char *A, char*B)
{
	int i;
	int j = 0;
	for(i=strlen(A) - 1; i > 0 && A[i] != '/'; i--); //Might not need the -1
	i++;
	while(j < strlen(B) && i < strlen(A)){
		if(A[i] != B[j]){
			return 0;
		}
		i++;
		j++;
	}
	if(i == strlen(A) && j == strlen(B)){
		return 1;
	}
	return 0;
}

void
find(char *path, char *FileToBeFound, int compass, int type_f, int type_d)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    printf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(compareFile(buf, FileToBeFound)){
		if(type_f == 1 || type_f + type_d == 0){
			printf(1, "%s\n", buf);
		}
	}
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      //printf(1, "find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
	while(read(fd, &de, sizeof(de)) == sizeof(de)){
		if(de.inum == 0)
			continue;
		memmove(p, de.name, DIRSIZ);
		p[DIRSIZ] = 0;
		if(stat(buf, &st) < 0){
			printf(1, "ls: cannot stat %s\n", buf);
			continue;
		}
		if(st.type == T_DIR){
			if(compass < 2){
				compass++;
			}
			else{
				if(compareFile(buf, FileToBeFound)){
					if(type_d == 1 || type_f + type_d == 0){
						printf(1, "%s\n", buf);
					}
				}
				//printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
				char *tempPath = (char*)malloc(512 * sizeof(char));
				char *tempFileToBeFound = (char*)malloc(512 * sizeof(char));
				strcpy(tempPath, buf);
				strcpy(tempFileToBeFound, FileToBeFound);
				find(tempPath, tempFileToBeFound, 0, type_f, type_d);
				//printf(1, "-%s- -%s-\n", tempFileToBeFound, tempPath);
			}
		}
		else{
			//printf(1, "found a file -%s-\n", fmtname(buf));
			if(compareFile(buf, FileToBeFound)){
				if(type_f == 1 || type_f + type_d == 0){
					printf(1, "%s\n", buf);
				}
			}
		}
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
	int fd;
	if(argc == 4){
		find(argv[1], argv[3], 0, 0, 0);
		exit();
	}
	int type_f = 0;// Establish some booleans for our flags.
	int  type_d = 0;
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "f") == 0){
			type_f = 1;
		}
		else if (strcmp(argv[i], "d") == 0){
			type_d = 1;
		}
	}
	if((fd = open(argv[1], 0)) < 0){
		printf(1, "cat: cannot open %s\n", argv[1]);
		exit();
	}
	find(argv[1], argv[3], 0,type_f, type_d); //Make our call to uniq based on the flags.
	close(fd);
	exit();
}