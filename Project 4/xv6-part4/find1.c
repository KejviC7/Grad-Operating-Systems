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

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), '\0', DIRSIZ-strlen(p));
  //printf(1, "%s\n", buf);

  return buf;
}

char* fmtname_directory(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = '/';
  memset(buf+strlen(p)+1, ' ', DIRSIZ-strlen(p)-1);
  return buf;
}

void
find(char *path, char *filename, char *type, int size, int size_special)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  //int skip = 0;
 
  
  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    //printf(2, "Error!");
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    //printf(2, "Error!");
    close(fd);
    return;
  } 
  
  //printf(1, "DEBUGGING: st.size is: %d\n", st.size);

  switch(st.type){
  case T_FILE:
   // Check if the file matches our filename and if yes print it
   if (strcmp(type, "d")!=0) {
        // Means we are not using the -size flag. Default parameter for size: -1
        if (size == -1) {
            if (compareFile(buf, filename)) {
              printf(1, "%s\n", buf);
            }
            //if ((strcmp(fmtname(path),filename)==0)) {
                //printf(1, "%s\n",path); 
           // }
        } else {
            // We are using the -size flag so we have 3 cases
            if (size_special == 0) {
                // SIZE EQUAL TO
                if (compareFile(buf, filename) && (st.size == size)){
                    printf(1, "%s\n",buf);
                }
            } else if (size_special == 1) {
                // SIZE GREATER THAN
                if (compareFile(buf, filename) && (st.size > size)){
                    printf(1, "%s\n",buf);
                }
            } else if (size_special == 2) {
                // SIZE LESS THAN
                if (compareFile(buf, filename) && (st.size < size)){
                    printf(1, "%s\n",buf);
                }
            }
    
        }
   } 
    //printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // Modification to original ls. Skip '.' and '..'
      //if(de.inum == 0 || strcmp(de.name,".") == 0 || strcmp(de.name,"..") == 0) {
        //continue;
      //}
      if (de.inum == 0) {
        continue;
      }
      //if(de.inum == 0)
			  //continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
			  printf(1, "ls: cannot stat %s\n", buf);
			  continue;
		  }
      if(st.type == T_DIR) {
        if (compareFile(buf, ".") || compareFile(buf, "..")) {
          continue;
        } else {

          //printf(1, " Buf value: %s\n", buf);
          // If inside a directory search the files inside. Call find() again
          if (strcmp(type, "d")==0) {
            // TYPE FLAG IS -d
            if (compareFile(buf, filename)){
                printf(1, "%s\n",buf);
            }
            find(buf, filename, type, size, size_special);
          } else if (strcmp(type, "n")==0) {
            // NORMAL FUNCTIONALITY
            if (size == -1) {
                //if ((strcmp(fmtname(path),filename)==0)){
                    //printf(1, "%s\n",path);
                //}
                if (compareFile(buf, filename)) {
                  printf(1, "%s\n", buf);
                }

            } else { 
                // We are using the -size flag so we have 3 cases
                if (size_special == 0) {
                    // SIZE EQUAL TO
                    if (compareFile(buf, filename) && (st.size == size)){
                        printf(1, "%s\n",buf);
                    }
                } else if (size_special == 1) {
                    // SIZE GREATER THAN
                    if (compareFile(buf, filename) && (st.size > size)){
                        printf(1, "%s\n",buf);
                    }
                } else if (size_special == 2) {
                    // SIZE LESS THAN
                    if (compareFile(buf, filename) && (st.size < size)){
                        printf(1, "%s\n",buf);
                    }
                }
            }
            find(buf, filename, type, size, size_special);

        } else if (strcmp(type, "f")==0) {
          if (compareFile(buf, filename)) {
                  printf(1, "%s\n", buf);
                }
          find(buf, filename, type, size, size_special);
        } 
     }
    } else {
      if (strcmp(type, "d")){
        if (compareFile(buf, filename)) {
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
  char *flag_def = "n"; // default
  char *flag_type;
  char *flag_1 = "-type";
  char *flag_2 = "-size";
  char *byte_size_notation;
  if (argc < 4) {
    printf(2, "Error! You need to provide name of the file/directory you are looking for!\n");
  } else if (argc == 4) {
    // Default find functionality
    printf(1, "Default Case!\n");
    find(argv[1], argv[3], flag_def, -1, 0);
  } else if (argc > 4) {
    // Flag type
    flag_type = argv[4];
    if (strcmp(flag_type,flag_1)==0) {
        // -type case
        printf(1, "Type filtering case!\n");
        find(argv[1], argv[3], argv[argc-1], -1, 0);
    } else if (strcmp(flag_type, flag_2)==0) {
        // -size case. We have 3 sub-cases
        byte_size_notation = argv[argc-1];

        if (byte_size_notation[0] == '+') {
            // -size +(Greater than) CASE
            // Skip the special element to simply get the number
            byte_size_notation++;
            //printf(1, "Greater than filter size: %d\n", atoi(byte_size_notation));
            printf(1, "Size (greater than) filtering case!\n");
            find(argv[1], argv[3], flag_def, atoi(byte_size_notation), 1);
        } else if (byte_size_notation[0] == '-') {
            // -size -(Less than) Case
            // Skip the special element to simply get the number
            byte_size_notation++;
            //printf(1, "Greater less filter size: %d\n", atoi(byte_size_notation));
            printf(1, "Size (less than) filtering case!\n");
            find(argv[1], argv[3], flag_def, atoi(byte_size_notation), 2);
        } else {
            // -size Equals Case
            printf(1, "Size (equals) filtering case!\n");
            find(argv[1], argv[3], flag_def, atoi(argv[argc-1]), 0);

        }
    }

  }
  exit();
}
