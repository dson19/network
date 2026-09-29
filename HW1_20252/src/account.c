#include "account.h"
#include <stdio.h>
#include <string.h>

#define MAX_ACCOUNT 20000

static Account accounts[MAX_ACCOUNT];

static int accNum =0;

int loadAccounts(const char *path){
    accNum = 0;
    char buffer[1007];
    FILE *fptr;
    char username[10000];
    int status;
    fptr = fopen(path, "r");
    if (fptr == NULL){
        return -1;
    }
    else{
        while (fgets(buffer, sizeof(buffer), fptr) != NULL){
            int matched = sscanf(buffer, "%s %d", username, &status);
            if (matched != 2){
                continue;
            }
            strcpy(accounts[accNum].username, username);
            accounts[accNum].status = status;
            accNum++;
        }
        fclose(fptr);
    }
    return accNum;
}

Account *findAccount(const char *username){
    for(int i =0; i < accNum; i++){
        if (strcmp(username, accounts[i].username) == 0){
            return &accounts[i];
        }
    }
    return NULL;
}

