#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "stream.h"
#include "util.h"

#define INPUT_BUFFER_SIZE (MAX_MESSAGE_LEN + 2)

/* Menu choices. */
enum {
    MENU_LOGIN = 1,
    MENU_POST,
    MENU_LOGOUT,
    MENU_EXIT
};

/* What the client shows about the session. The server keeps the real login
 * state and checks every request; this copy is only used for display. */
typedef struct {
    Connection conn;
    bool loggedIn;
    char username[INPUT_BUFFER_SIZE];
} ClientState;

/* Parse a port number in [1, 65535]. Returns 0 and sets *port if valid, -1 otherwise. */
static int parsePort(const char *text, int *port){
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 65535){
        return -1;
    }
    *port = (int)value;
    return 0;
}

static void printResponse(const char *response){
    printf(">> [%s] %s\n", response, describeResponse(response));
}

static void printMenu(const ClientState *state){
    printf("\n========== MESSAGE BOARD ==========\n");
    if (state->loggedIn){
        printf("Logged in as: %s\n", state->username);
    } else {
        printf("Not logged in\n");
    }
    printf("-----------------------------------\n");
    printf("  1. Log in\n");
    printf("  2. Post a message\n");
    printf("  3. Log out\n");
    printf("  4. Exit\n");
    printf("===================================\n");
    printf("Your choice: ");
    fflush(stdout);
}

/* Show a prompt and read one line from the user.
 * Returns READ_OK with the text in `input`, READ_TOO_LONG, or READ_EOF. */
static int prompt(const char *label, char *input, size_t size){
    printf("%s", label);
    fflush(stdout);
    return readLine(input, size);
}

/* Send a request and show the server's answer.
 * Returns 0 with the code in `response`, or -1 if the connection is lost. */
static int exchange(ClientState *state, const char *command, const char *argument,
                    char *response, size_t size){
    if (sendRequest(&state->conn, command, argument, response, size) != 0){
        printf(">> Lost connection to the server\n");
        return -1;
    }
    printResponse(response);
    return 0;
}

/* Each handler below returns 1 to keep going, 0 to end the program. */

static int handleLogin(ClientState *state){
    char input[INPUT_BUFFER_SIZE];
    int status = prompt("Username: ", input, sizeof(input));
    if (status == READ_EOF){
        return 0;
    }
    char *username = trimSpaces(input);
    if (status == READ_TOO_LONG || strlen(username) > MAX_USERNAME_LEN){
        printf(">> Username is too long (maximum %d characters)\n", MAX_USERNAME_LEN);
        return 1;
    }
    if (username[0] == '\0'){
        printf(">> Username must not be empty\n");
        return 1;
    }

    char response[MAX_RESPONSE_LEN];
    if (exchange(state, LOGIN_COMMAND, username, response, sizeof(response)) != 0){
        return 0;
    }
    if (strcmp(response, REPLY_LOGIN_OK) == 0){
        state->loggedIn = true;
        snprintf(state->username, sizeof(state->username), "%s", username);
    }
    return 1;
}

static int handlePost(ClientState *state){
    char input[INPUT_BUFFER_SIZE];
    int status = prompt("Message: ", input, sizeof(input));
    if (status == READ_EOF){
        return 0;
    }
    if (status == READ_TOO_LONG || strlen(input) > MAX_ARTICLE_LEN){
        printf(">> Message is too long (maximum %d characters)\n", MAX_ARTICLE_LEN);
        return 1;
    }
    char copy[INPUT_BUFFER_SIZE];
    snprintf(copy, sizeof(copy), "%s", input);
    if (trimSpaces(copy)[0] == '\0'){
        printf(">> Message must not be empty\n");
        return 1;
    }

    char response[MAX_RESPONSE_LEN];
    return exchange(state, POST_COMMAND, input, response, sizeof(response)) == 0;
}

static int handleLogout(ClientState *state){
    char response[MAX_RESPONSE_LEN];
    if (exchange(state, LOGOUT_COMMAND, NULL, response, sizeof(response)) != 0){
        return 0;
    }
    if (strcmp(response, REPLY_LOGOUT_OK) == 0){
        state->loggedIn = false;
        state->username[0] = '\0';
    }
    return 1;
}

/* Read a menu choice. Returns MENU_* , 0 for an invalid choice, -1 on EOF. */
static int readChoice(void){
    char input[16];
    int status = readLine(input, sizeof(input));
    if (status == READ_EOF){
        return -1;
    }
    char *choice = trimSpaces(input);
    if (status != READ_OK || strlen(choice) != 1 || choice[0] < '1' || choice[0] > '4'){
        return 0;
    }
    return choice[0] - '0';
}

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s IP_Addr Port_Number\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port;
    if (parsePort(argv[2], &port) != 0){
        fprintf(stderr, "Error: invalid port number '%s'\n", argv[2]);
        return EXIT_FAILURE;
    }

    /* A server that disconnects while we write must not kill the client. */
    signal(SIGPIPE, SIG_IGN);

    int socketFd = connectToServer(argv[1], port);
    if (socketFd < 0){
        return EXIT_FAILURE;
    }

    ClientState state;
    initConnection(&state.conn, socketFd);
    state.loggedIn = false;
    state.username[0] = '\0';

    char welcome[MAX_RESPONSE_LEN];
    if (recvLine(&state.conn, welcome, sizeof(welcome)) < 0){
        printf("The server closed the connection\n");
        close(socketFd);
        return EXIT_FAILURE;
    }
    printResponse(welcome);
    if (strcmp(welcome, REPLY_WELCOME) != 0){
        close(socketFd);
        return EXIT_FAILURE;
    }

    int running = 1;
    while (running){
        printMenu(&state);
        switch (readChoice()){
            case MENU_LOGIN:
                running = handleLogin(&state);
                break;
            case MENU_POST:
                running = handlePost(&state);
                break;
            case MENU_LOGOUT:
                running = handleLogout(&state);
                break;
            case MENU_EXIT:
            case -1:
                running = 0;
                break;
            default:
                printf(">> Invalid choice, please enter a number from 1 to 4\n");
                break;
        }
    }

    printf("Goodbye!\n");
    close(socketFd);
    return EXIT_SUCCESS;
}
