/* UDP DNS resolution client: sends a domain name or IP address and prints the server's answer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 2048
#define RECV_TIMEOUT_SEC 5

static void strip_newline(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
}

static void print_response(const char *response) {
    if (response[0] == '+') {
        printf("Result:\n");
        char copy[BUFFER_SIZE];
        strncpy(copy, response + 1, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char *token = strtok(copy, " ");
        while (token != NULL) {
            printf("  %s\n", token);
            token = strtok(NULL, " ");
        }
    } else if (response[0] == '-') {
        printf("Error: %s\n", response + 1);
    } else {
        printf("Unexpected response: %s\n", response);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s IPAddress PortNumber\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = RECV_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP address: %s\n", server_ip);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    while (1) {
        printf("Enter domain name or IP address (empty to quit): ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        strip_newline(input);

        if (strlen(input) == 0) {
            break;
        }

        if (sendto(sockfd, input, strlen(input), 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("sendto");
            continue;
        }

        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        ssize_t n = recvfrom(sockfd, response, sizeof(response) - 1, 0,
                              (struct sockaddr *)&from_addr, &from_len);
        if (n < 0) {
            fprintf(stderr, "No response from server (timeout or error).\n");
            continue;
        }
        response[n] = '\0';

        print_response(response);
    }

    close(sockfd);
    return 0;
}
