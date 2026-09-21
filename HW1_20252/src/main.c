#include <stdio.h>
#include <stdlib.h>

#include "account.h"
#include "logger.h"
#include "session.h"
#include "util.h"

/* TODO: replace with your own student ID before submitting (used to name
 * the log file, e.g. STUDENT_ID "20201234" -> log_20201234.txt). */
#define STUDENT_ID "20235994"

/* account.txt must sit next to the executable, per the assignment spec. */
#define ACCOUNT_FILE "account.txt"

static void print_menu(void) {
    printf("\n1. Log in\n");
    printf("2. Post message\n");
    printf("3. Logout\n");
    printf("4. Exit\n");
    printf("Enter your choice: ");
}

/*
 * Function 1: Log in.
 * Fails if a user is already logged in, if the account does not exist, or
 * if the account is banned. Otherwise records the account as logged in.
 */
static void handle_login(AccountList *accounts, Session *session, const char *log_path) {
    printf("Username: ");
    char *username = read_line();
    if (!username) {
        fprintf(stderr, "Failed to read input.\n");
        return;
    }

    LogStatus result;

    if (session_is_logged_in(session)) {
        /* Already logged in takes priority over the entered username,
         * per the spec: a second login attempt always fails. */
        printf("You have already logged in\n");
        result = LOG_ERR;
    } else {
        Account *account = account_find(accounts, username);
        if (!account) {
            printf("Account is not exist\n");
            result = LOG_ERR;
        } else if (account->status == 0) {
            printf("Account is banned\n");
            result = LOG_ERR;
        } else {
            session_login(session, account);
            printf("Hello %s\n", username);
            result = LOG_OK;
        }
    }

    if (logger_write(log_path, 1, username, result) != 0) {
        fprintf(stderr, "Warning: could not write to log file '%s'\n", log_path);
    }

    free(username);
}

/*
 * Function 2: Post message.
 * Only succeeds while a user is logged in.
 */
static void handle_post(Session *session, const char *log_path) {
    printf("Post message: ");
    char *message = read_line();
    if (!message) {
        fprintf(stderr, "Failed to read input.\n");
        return;
    }

    LogStatus result;
    if (session_is_logged_in(session)) {
        printf("Successful post\n");
        result = LOG_OK;
    } else {
        printf("You have not logged in.\n");
        result = LOG_ERR;
    }

    if (logger_write(log_path, 2, message, result) != 0) {
        fprintf(stderr, "Warning: could not write to log file '%s'\n", log_path);
    }

    free(message);
}

/*
 * Function 3: Log out.
 * Only succeeds while a user is logged in.
 */
static void handle_logout(Session *session, const char *log_path) {
    LogStatus result;

    if (session_is_logged_in(session)) {
        session_logout(session);
        printf("Successful log out\n");
        result = LOG_OK;
    } else {
        printf("You have not logged in.\n");
        result = LOG_ERR;
    }

    /* Logout takes no user-supplied value, so the log's value field is empty. */
    if (logger_write(log_path, 3, "", result) != 0) {
        fprintf(stderr, "Warning: could not write to log file '%s'\n", log_path);
    }
}

/*
 * Function 4: Terminate the program.
 * Always logged as successful, then the main loop is stopped by the caller.
 */
static void handle_exit(const char *log_path) {
    if (logger_write(log_path, 4, "", LOG_OK) != 0) {
        fprintf(stderr, "Warning: could not write to log file '%s'\n", log_path);
    }
}

int main(void) {
    AccountList accounts;
    if (account_load(ACCOUNT_FILE, &accounts) != 0) {
        fprintf(stderr, "Error: could not open account file '%s'\n", ACCOUNT_FILE);
        return EXIT_FAILURE;
    }

    Session session;
    session_init(&session);

    char log_path[64];
    snprintf(log_path, sizeof(log_path), "log_%s.txt", STUDENT_ID);

    int running = 1;
    while (running) {
        print_menu();

        char *choice_line = read_line();
        if (!choice_line) {
            break; /* EOF on stdin (e.g. Ctrl+D) */
        }

        int choice = atoi(choice_line);
        free(choice_line);

        switch (choice) {
            case 1:
                handle_login(&accounts, &session, log_path);
                break;
            case 2:
                handle_post(&session, log_path);
                break;
            case 3:
                handle_logout(&session, log_path);
                break;
            case 4:
                handle_exit(log_path);
                running = 0;
                break;
            default:
                printf("Invalid choice, please try again.\n");
                break;
        }
    }

    account_free(&accounts);
    return EXIT_SUCCESS;
}
