#ifndef UTIL_H
#define UTIL_H

#include<stdio.h>
#include<string.h>
#include<time.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef power_2
#define power_2(a)           1 << a
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

//returns number of elements after splitting
int parse_Filename(char* filename,char array[][30]){
    if(filename == NULL)
        return 0;
    int i = 0;
    if(filename[0] == '/'){
        strcpy(array[i],"/");
        i++;
    }
    char * token = strtok(filename, "/");

    while( token != NULL ) {
      strcpy(array[i],token);
      token = strtok(NULL, "/");
      i++;
   }

   return i;
}

struct tm get_Time(){
    time_t t = time(NULL);
    return *localtime(&t);
}

#endif
