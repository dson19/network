#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_LEN 1000

char *readLine(void){
    char tmp[MAX_LEN];

    if (fgets(tmp, MAX_LEN, stdin) == NULL){
        return NULL;
    }

    size_t len = strlen(tmp);

    while (len >0 && (tmp[len-1] == '\n' || tmp[len-1] == '\r')){
        tmp[--len] = '\0';
    }

    char *result = malloc( (len+1)* sizeof(char));
    if (result != NULL){
        strcpy(result, tmp);
    }
    return result;
}