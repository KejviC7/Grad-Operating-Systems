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
		int First_Instance_And_Duplicate = 0; // A special boolean to insure correct functionality of -d flag.
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
					if(countArray[p] == 1){
						First_Instance_And_Duplicate  = 1;
					}
					else if(First_Instance_And_Duplicate != 1){
						skipCondition = 1;
					}
					countArray[q]++;
					countArray[p]++;
				}
				else{
					break;
				}
			}
			//Check if we ended and there was a duplicated before the last line. (Only needed for a specific cirsumstance that occurs when the loop ends and the last element is part of a series of parallel elements)
			//if(p == 0){
			//	printf(1, "%d", countArray[p]);
			//}
			if(countArray[p] > 1 && p > 0){ //This code basically just prevents the last of a line of parallel duplicates from printing.
				if (iFlag == 1) {
					if(casecmp(bufArray[p-1], bufArray[p]) == 0){
						skipCondition = 1;
					}
				}
				else {
					if(strcmp(bufArray[p-1], bufArray[p]) == 0){
						skipCondition = 1;
					}
				}
			}
            // Take the first of every index that has a duplicate somewhere.
            if(First_Instance_And_Duplicate  == 1){
                uniqArray[uniq_index] = p;
                uniq_index++;
            }
			First_Instance_And_Duplicate  = 0;
			//printf(1, "debugging %s value =%d \n", bufArray[p], skipCondition);
            if (dFlag != 1) {
                if(skipCondition != 1){
                    if(cFlag == 1 && strcmp(bufArray[p], "") != 0){ // When passing in -c parameter
                        int val = countArray[p];
                        printf(1, "%d  ", val);
                    }
                    if (write(1, bufArray[p], n) != n) {
                        printf(1, "cat: write error\n");
                        exit();
                    }
                }
            } 
            
			skipCondition = 0;
		}
		// Used to print the duplicate pairs.
		if (dFlag == 1) {
			for (int k=0; k < uniq_index; k++) {
                //Grab count
                //printf(1, "Count per line %s in the array buf is: %d\n", bufArray[uniqArray[k]], countArray[uniqArray[k]]);
				if(cFlag == 1 && strcmp(bufArray[uniqArray[k]], "") != 0){ // When passing in -c parameter
                    int val = countArray[uniqArray[k]];
                    printf(1, "%d  ", val);
                }
                write(1, bufArray[uniqArray[k]], n);
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

	if(argc <= 1){ // Event we need to pass a pipe instead of a file discriptor.
		uniq(0, 0, 0, 0);
		exit();
	}
	int cFlag = 0;// Establish some booleans for our flags.
	int dFlag = 0;
	int iFlag = 0;
	int *flagMap = (int*)malloc(512 * sizeof(int));// Used to ensure we don't call uniq on a flag.
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "-c") == 0){
			cFlag = 1;
			flagMap[i] = 0;
		}
		else if (strcmp(argv[i], "-d") == 0){
			dFlag = 1;
			flagMap[i] = 0;
		}
		else if (strcmp(argv[i], "-i") == 0){
			iFlag = 1;
			flagMap[i] = 0;
		}
		else{
			flagMap[i] = 1;
		}
	}
	for(i = 1; i < argc; i++){// Call uniq on all the inputs that aren't flags.
		if(flagMap[i] == 1){
			if((fd = open(argv[i], 0)) < 0){
				printf(1, "cat: cannot open %s\n", argv[i]);
				exit();
			}
			uniq(fd, cFlag, dFlag, iFlag); //Make our call to uniq based on the flags.
			close(fd);
		}
	}
	exit();
}
