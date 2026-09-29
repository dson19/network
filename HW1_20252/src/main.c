#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "account.h"
#include "logger.h"
#include "session.h"
#include "util.h"

#define STUDENT_ID "20235994"
#define ACCOUNT_FILE "account.txt"

static void printMenu(void){
    printf("\n1. Log in\n2. Post message\n3. Logout\n4. Exit\nEnter your choice: ");
}

static void handleLogin(const char *logPath){
    printf("Username: ");
    char *username = readLine();
    if (username == NULL){
        fprintf(stderr, "Failed to read input.\n");
        return;
    }

    bool ok;
    if (isLoggedIn()){
        printf("You have already logged in\n");
        ok = false;
    } else {
        Account *acc = findAccount(username);
        if (acc == NULL){
            printf("Account is not exist\n");
            ok = false;
        } else if (acc->status == 0){
            printf("Account is banned\n");
            ok = false;
        } else {
            login(acc);
            printf("Hello %s\n", username);
            ok = true;
        }
    }

    if (writeLog(logPath, 1, username, ok) != 0){
        fprintf(stderr, "Warning: could not write to log file '%s'\n", logPath);
    }

    free(username);
}

static void handlePost(const char *logPath){
    printf("Post message: ");
    char *message = readLine();
    if (message == NULL){
        fprintf(stderr, "Failed to read input.\n");
        return;
    }

    bool ok;
    if (isLoggedIn()){
        printf("Successful post\n");
        ok = true;
    } else {
        printf("You have not logged in.\n");
        ok = false;
    }

    if (writeLog(logPath, 2, message, ok) != 0){
        fprintf(stderr, "Warning: could not write to log file '%s'\n", logPath);
    }

    free(message);
}

static void handleLogout(const char *logPath){
    bool ok;
    if (isLoggedIn()){
        logout();
        printf("Successful log out\n");
        ok = true;
    } else {
        printf("You have not logged in.\n");
        ok = false;
    }

    if (writeLog(logPath, 3, "", ok) != 0){
        fprintf(stderr, "Warning: could not write to log file '%s'\n", logPath);
    }
}

static void handleExit(const char *logPath){
    if (writeLog(logPath, 4, "", true) != 0){
        fprintf(stderr, "Warning: could not write to log file '%s'\n", logPath);
    }
}

int main(void){
    int accCount = loadAccounts(ACCOUNT_FILE);
    if (accCount < 0){
        fprintf(stderr, "Error: could not open account file '%s'\n", ACCOUNT_FILE);
        return EXIT_FAILURE;
    }

    char logPath[64];
    snprintf(logPath, sizeof(logPath), "log_%s.txt", STUDENT_ID);

    bool running = true;
    while (running){
        printMenu();

        char *choiceLine = readLine();
        if (choiceLine == NULL){
            break; /* EOF (Ctrl+D) */
        }
        int choice = atoi(choiceLine);
        free(choiceLine);

        switch (choice){
            case 1:
                handleLogin(logPath);
                break;
            case 2:
                handlePost(logPath);
                break;
            case 3:
                handleLogout(logPath);
                break;
            case 4:
                handleExit(logPath);
                running = false;
                break;
            default:
                printf("Invalid choice, please try again.\n");
                break;
        }
    }

    return EXIT_SUCCESS;
}
