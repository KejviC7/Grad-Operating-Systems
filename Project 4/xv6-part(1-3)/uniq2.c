// Simple program for the uniq command
#include "types.h"
#include "stat.h"
#include "user.h"

#define MAX_LINES 100
#define MAX_LEN 512

void uniq(int fd)
{
    char buf[512];//, *p;//, *p;//, *p;
    int n;

    
    //char lines_array[MAX_LINES][MAX_LEN];
    //char *prev;
    //int i,j;
    
    while((n = read(fd, buf, sizeof(buf))) > 0) {
        
        
        //printf(1, "Buf values: \n\n%s", buf+1);
        // Increment i for the next line to be stored in the next location
        //strcpy(line_array[i], buf);
        
        
        // Write to terminal
        //strcpy(container, buf);
        //printf(1, "Current: %s", p);
        //printf(1, "Next: %s", );    

        //printf(1, "Buf value at 0: %s\n", buf);
        //printf(1, "Pointer value: %s", p);
        /*
        if (write(1, buf, n) != n) {
            printf(2, "Error writing the lines to terminal!\n");
            exit();
        }
        */
    }

    int i, j;
    int new_pos;
    int null_char = 10;
    j = 0;
    int char_pos = 0;
    char lines_array[MAX_LINES][MAX_LEN] = {{0}};
    char temp;
    for(i=0; i <strlen(buf); i++) {

        if ((int)(buf[i]) == 10) {

            printf(1, "New line reached! The current line has been read!\n");
            // NUL termination of C string
            new_pos = char_pos++;
            lines_array[j][new_pos] = (char)null_char;//'\0';
            printf(1, "Line array curr character: %c\n",lines_array[j][new_pos]);
            // Increment lines_array position
            j++;
            // Reset char_pos to 0
            char_pos=0;
        } 

        //printf(1, "Buf value: %c\n",buf[i]);
        temp = (char)buf[i];
        printf(1, "Temp value: %c\n",temp);
        printf(1, "Pair (j,char_pos): %d,%d\n", j, char_pos);
        lines_array[j][char_pos] = temp;
        printf(1, "Line array curr character: %c\n",lines_array[j][char_pos]);
        char_pos++;
    } 

    char k = 'k';
    lines_array[4][5] = k;
    lines_array[4][6] = (char)null_char;
    printf(1, "Value: %c\n", lines_array[0][0]);
    


    //printf(1, "Buf values: %s\n", lines_array);
    
    if (n<0) {
        printf(2, "Error reading the file. Make sure the file is correct!\n");
        exit();
    }
    exit();
}





int main(int argc, char *argv[])
{
    int fd, i;

    if (argc<2) {
        // Error
        printf(2, "Error: You need to provide filename!\n");
        exit();
    }

    for (i=1; i <argc; i++){
        // Condition to check if we can open the file to read
        if((fd = open(argv[i], 0)) < 0) {
            printf(2, "Cannot open file: %s\n", argv[i]);
            exit();
        }
        uniq(fd);
        close(fd);
    }
    exit();
}
