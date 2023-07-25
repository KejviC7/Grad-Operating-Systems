#include "types.h"
#include "stat.h"
#include "user.h"

char buf[512];

int
casecmp(const char *p, const char *q)
{
    // If element is a character from the alphabet then we need to convert
    // To not overcomplicate things let us convert uppercase to lowercase when possible
    // Lower case range ASCII: 97-122
    // Upper case range ASCII: 65-90
    // Offset to go from uppercase to lowercase is: 32
    int ascii_offset = 32;
    int temp_p = 0;
    int temp_q = 0;
    int comparison_flag = 0;
    while(*p != '\0' && *q != '\0' && comparison_flag == 0) {
        temp_p = (int)*p;
        temp_q = (int)*q;
        
        if((int)*p >= 65 && (int)*p <= 90) {
            // Convert to lowercase
            temp_p = (int)*p + ascii_offset;
        }
        if((int)*q >= 65 && (int)*q <= 90) {
            // Convert to lowercase
            temp_q = (int)*q + ascii_offset;
        }
        //printf(1, "Temp p: %d\n", temp_p);
        //printf(1, "Temp q: %d\n", temp_q);
        if (temp_p == temp_q) {
            // Character of p is equal to character of q
            comparison_flag = 0;
        } else {
            // We just found characters that don't match so set to 0 and return
            comparison_flag = 1;
            break;
        }

        p++;
        q++;
    }
    return comparison_flag;
}

void
uniq(int fd, int cFlag, int dFlag, int iFlag)
{
  int n;
  int condition = 1;
  int offset = 0;
  int offsetIndex = 0;
  char *bufArray[512]; //512 max number of lines
  char *tempBuf = (char*)malloc(512 * sizeof(char));
  int *countArray = (int*)malloc(512 * sizeof(int));//Keeps track of repeats.
  int *uniqArray = (int*)malloc(512 * sizeof(int)); //Keeps track of unique indexes
  // Initialize array elements to 1
  for(int i = 0; i < 512; i++)
		countArray[i] = 1;
  // Pass empty string to tempBuf
  strcpy(tempBuf, "");
  // Initialize bufArray[0] to the empty string 
  bufArray[0] = tempBuf;

  // Does nothing
  if(bufArray[0] == bufArray[0]){
	  //Yeah so cool!
  }
  // Reading the file and performing population of bufArray
	while((n = read(fd, buf, sizeof(buf))) > 0) {
        // Condition is used to determine if we can keep reading in.
		while(condition == 1){
            // We use for loop to populate our bufArray by reading each character from the lines
			for(int y = 0; y < 512; y++){
              // We have reached the end of the file. Set condition to 0 to stop reading  
			  if(buf[y] == '\0'){
				  condition = 0;
				  break;
			  }
              // We reached a new-line character. We need to move on to next line
			  if(buf[y] == '\n'){
                  // We pass the new-line to the sequence of chars we were passing to bufArray[offset]
				  bufArray[offset][offsetIndex]  = '\n';
				  // Now we increment offset to move to the next location to store the next line and reset offsetIndex to 0.
                  offset += 1;
				  offsetIndex = 0;
                  
                  // Allocate memory for the the new string location in the bufArray. And then use it to initialize that next position with an empty string which we can populate character by character. 
				  char *otherTempBuf = (char*)malloc(512 * sizeof(char));
				  strcpy(otherTempBuf, "");
				  bufArray[offset] = otherTempBuf;
			  }
			  else{
                  // Populate the bufArray by character
				  bufArray[offset][offsetIndex] = buf[y];
				  offsetIndex++;
			  }
			}
		}
        int uniq_index = 0;
        int comparison_res;
        // Output Manipulation in accordance to the passed flags
		int skipCondition = 0;
        // We now have the populated bufArray and by accessing bufArray[] we can get strings
        // So we search the bufArray by keeping track of the current and the next strings and compare them
		for(int p = 0; p < offset + 1; p++){
			for(int q = p + 1; q < offset; q++){
				
                //Set conditions when to use strcmp and when to use casecmp based on the -i Flag passed
                if (iFlag == 1) {
                    comparison_res = casecmp(bufArray[p], bufArray[q]);
                } else {
                    comparison_res = strcmp(bufArray[p], bufArray[q]);
                }

                if(comparison_res == 0){
					// Duplicate lines found. Set skipCondition to 1 and increment the countArray count for that line
                    skipCondition = 1;
					countArray[q]++;
				}
			}
            // Track the indexes where there is no duplicates.
            if(skipCondition != 1){
                uniqArray[uniq_index] = p;
                uniq_index++;
            }

            if (dFlag != 1) {
                if(skipCondition != 1){
                    //printf(1, "P index in the buf: %d\n", p);
                    if(cFlag == 1){ // When passing in -c parameter
                        //char *TempThing = (char*)malloc(512 * sizeof(char));
                        //TempThing[0] = (int)countArray[p];
                        //TempThing[1] = '\t';
                        int val = countArray[p];
                        printf(1, "%d  ", val);
                        //uniqArray[uniq_index] = p;
                        //uniq_index++;
                    }

                    if (write(1, bufArray[p], n) != n) {
                        printf(1, "cat: write error\n");
                        exit();
                    }

                
                }
            } 
            
			skipCondition = 0;
	}
    /* FOR DEBUGGING PURPOSES
    if (iFlag == 1) {
        char *el1 = "line1";
        char *el2 = "Line2";

        if (casecmp(el1, el2) == 0) {
            printf(1, "The strings are the same!");
        } else {
            printf(1, "The string not the same!");
        }

    }*/

    if (dFlag == 1) {
        for (int k=0; k < uniq_index; k++) {
            if(countArray[uniqArray[k]] > 1) {
                //Grab count
                //printf(1, "Count per line %s in the array buf is: %d\n", bufArray[uniqArray[k]], countArray[uniqArray[k]]);
                write(1, bufArray[uniqArray[k]], n);
                
            }
        }
    }

  if(n < 0){
    printf(1, "cat: read error\n");
    exit();
  }
}
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    uniq(0, 0, 0, 0);
    exit();
  }
	if(strcmp(argv[argc - 1], "-c") == 0){
		for(i = 1; i < argc - 1; i++){
			if((fd = open(argv[i], 0)) < 0){
				printf(1, "cat: cannot open %s\n", argv[i]);
				exit();
			}
			uniq(fd, 1, 0, 0);
			close(fd);
		}
	} else if (strcmp(argv[argc - 1], "-d") == 0){
		for(i = 1; i < argc - 1; i++){
			if((fd = open(argv[i], 0)) < 0){
				printf(1, "cat: cannot open %s\n", argv[i]);
				exit();
			}
			uniq(fd, 0, 1, 0);
			close(fd);
		}
	} else if (strcmp(argv[argc - 1], "-i") == 0){
		for(i = 1; i < argc - 1; i++){
			if((fd = open(argv[i], 0)) < 0){
				printf(1, "cat: cannot open %s\n", argv[i]);
				exit();
			}
			uniq(fd, 1, 0, 1);
			close(fd);
		}
    } else {
		for(i = 1; i < argc; i++){
			if((fd = open(argv[i], 0)) < 0){
				printf(1, "cat: cannot open %s\n", argv[i]);
				exit();
			}
			uniq(fd, 0, 0, 0);
			close(fd);
		}
	}
  exit();
}
