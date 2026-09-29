#include "session.h"
#include <stddef.h>


static const Account *currentUser = NULL;

bool isLoggedIn(void){
    return currentUser != NULL;
}

void login(const Account *account){
    currentUser = account;
}

void logout(void){
    currentUser = NULL;
}

const char *getCurrentUsername(void){
    return currentUser ? currentUser->username : NULL;
}
