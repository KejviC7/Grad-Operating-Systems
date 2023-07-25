#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

// A special string compare function that works but ignoring anything before the last '/'
int compareFile(char *A, char*B)
{
	int i;
	int j = 0;
	for(i=strlen(A) - 1; i > 0 && A[i] != '/'; i--);
	i++;
	while(j < strlen(B) && i < strlen(A)){
		if(A[i] != B[j]){
			return 0;
		}
		i++;
		j++;
	}
	if(i == strlen(A) && j == strlen(B)){
		return 1; // We have a matching name reguardless of the directory.
	}
	return 0;
}

// Helper function that checks if a size is valid based on the user defined flags.
int checkSize(int sizeFlag, int minusFlag, int plusFlag, int sizeToCheck, int size){
	if(sizeFlag == 0){
		return 1;
	}
	else{
		if(minusFlag == 1){ // User enteres the - flag.
			if(sizeToCheck < size){
				return 1;
			}
		}
		else if(plusFlag == 1){ // User enteres the + flag.
			if(sizeToCheck > size){
				return 1;
			}
		}
		else{
			if(size == sizeToCheck){ // User enteres just a size.
				return 1;
			}
		}
	}
	return 0;
}

void
find(char *path, char *FileToBeFound, int compass, int type_f, int type_d, int sizeFlag, int minusFlag, int plusFlag, int size)
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
  case T_FILE: // This is used in the even we call ls on a file not a directory.
    if(compareFile(buf, FileToBeFound)){ // We have found a matching name.
		if(type_f == 1 || type_f + type_d == 0){ // Established the correct use of flags.
			if(type_f != 1){
				if(checkSize(sizeFlag, minusFlag, plusFlag, st.size, size)){
					printf(1, "%s\n", buf); // Finally print the result to the user.
				}
			}
			else if(st.type == T_FILE){
				if(checkSize(sizeFlag, minusFlag, plusFlag, st.size, size)){
					printf(1, "%s\n", buf);
				}
			}
		}
	}
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
	while(read(fd, &de, sizeof(de)) == sizeof(de)){ // Pull everything from the current directory.
		if(de.inum == 0)
			continue;
		memmove(p, de.name, DIRSIZ);
		p[DIRSIZ] = 0;
		if(stat(buf, &st) < 0){
			printf(1, "ls: cannot stat %s\n", buf);
			continue;
		}
		if(st.type == T_DIR){
			if(compass < 2){ // Compass is used to ignore the "." and ".." that are present at the start of every file.
				compass++;
			}
			else{
				if(compareFile(buf, FileToBeFound)){ // We have found a matching name.
					if(type_d == 1 || type_f + type_d == 0){ // Established the correct use of flags.
						if(type_d != 1){
							if(checkSize(sizeFlag, minusFlag, plusFlag, st.size, size)){
								printf(1, "%s\n", buf); // Finally print the result to the user.
							}
					}
						else if(st.type == T_DIR){
							if(checkSize(sizeFlag, minusFlag, plusFlag, st.size, size)){
								printf(1, "%s\n", buf);
							}
						}
					}
				}
				char *tempPath = (char*)malloc(512 * sizeof(char)); // The act of clonning the variable is probably not needed. Although this is what this does.
				char *tempFileToBeFound = (char*)malloc(512 * sizeof(char));
				strcpy(tempPath, buf); // Populate the new variables.
				strcpy(tempFileToBeFound, FileToBeFound);
				find(tempPath, tempFileToBeFound, 0, type_f, type_d, sizeFlag, minusFlag, plusFlag, size); // Recure on this fucntion. Be sure to set the compass to 0 to ignore the current and last directory from another recursive call.
			}
		}
		else{
			if(compareFile(buf, FileToBeFound)){ // We have found a matching name.
				if(type_f == 1 || type_f + type_d == 0){ // Established the correct use of flags.
					if(type_f != 1){
						if(checkSize(sizeFlag, minusFlag, plusFlag, st.size, size)){
							printf(1, "%s\n", buf); // Finally print the result to the user.
						}
					}
					else if(st.type == T_FILE){
						if(checkSize(sizeFlag, minusFlag, plusFlag, st.size, size)){
							printf(1, "%s\n", buf);
						}
					}
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
		find(argv[1], argv[3], 0, 0, 0, 0, 0, 0, 0); // Perform the defult call.
		exit();
	}
	int type_f = 0;// Establish some booleans for our flags.
	int  type_d = 0;
	int sizeFlag = 0;
	int plusFlag = 0;
	int minusFlag = 0;
	for(int i = 1; i < argc; i++){ // Check for the flags.
		if(strcmp(argv[i], "f") == 0){
			type_f = 1;
		}
		else if (strcmp(argv[i], "d") == 0){
			type_d = 1;
		}
		else if(strcmp(argv[i], "-size") == 0){
			sizeFlag = 1;
		}
	}
	if((fd = open(argv[1], 0)) < 0){
		printf(1, "cat: cannot open %s\n", argv[1]);
		exit();
	}
	if(argv[argc - 1][0] == '+'){
		plusFlag = 1;
	}
	else if(argv[argc - 1][0] == '-'){
		minusFlag = 1;
	}
	if(sizeFlag == 0){ // Set of if statements establishes the correct way to make the call based on the flags.
		find(argv[1], argv[3], 0,type_f, type_d, sizeFlag, minusFlag, plusFlag, 0); // Make out call based on flags.
	}
	else if(minusFlag == 0 && plusFlag == 0){
		find(argv[1], argv[3], 0,type_f, type_d, sizeFlag, minusFlag, plusFlag, atoi(argv[argc - 1]));
	}
	else{
		char *temp = (char*)malloc(512 * sizeof(char));
		temp = argv[argc - 1];
		temp++; // A trick to remove the + or - in front of the integer.
		find(argv[1], argv[3], 0,type_f, type_d, sizeFlag, minusFlag, plusFlag, atoi(temp));
	}
	close(fd); // Close the file.
	exit();
}