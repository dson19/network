/* UDP DNS resolution server: forward (domain -> IP list) and reverse (IP -> names) lookup. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define STUDENT_ID "StudentID"
#define BUFFER_SIZE 2048
#define RESPONSE_SIZE 2048

static void get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(buf, len, "%d/%m/%Y %H:%M:%S", &tm_info);
}

static void log_request(FILE *log_fp, const char *request, const char *result) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_fp, "[%s]$%s$%s\n", timestamp, request, result);
    fflush(log_fp);
}

static int is_ip_address(const char *str) {
    struct in_addr addr4;
    struct in6_addr addr6;
    return inet_pton(AF_INET, str, &addr4) == 1 || inet_pton(AF_INET6, str, &addr6) == 1;
}

static void resolve_forward(const char *domain, char *response, size_t resp_size) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(domain, NULL, &hints, &res);
    if (status != 0) {
        snprintf(response, resp_size, "-Not found information");
        return;
    }

    response[0] = '+';
    response[1] = '\0';
    size_t len = 1;
    for (p = res; p != NULL; p = p->ai_next) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipv4->sin_addr, ip_str, sizeof(ip_str));

        if (strstr(response, ip_str) != NULL) {
            continue; /* avoid duplicate entries */
        }

        size_t needed = strlen(ip_str) + 2; /* space + ip */
        if (len + needed >= resp_size) {
            break;
        }
        if (len > 1) {
            strcat(response, " ");
            len += 1;
        }
        strcat(response, ip_str);
        len += strlen(ip_str);
    }
    freeaddrinfo(res);

    if (len <= 1) {
        snprintf(response, resp_size, "-Not found information");
    }
}

static void resolve_reverse(const char *ip_str, char *response, size_t resp_size) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        snprintf(response, resp_size, "-Not found information");
        return;
    }

    struct hostent *he = gethostbyaddr(&addr, sizeof(addr), AF_INET);
    if (he == NULL || he->h_name == NULL) {
        snprintf(response, resp_size, "-Not found information");
        return;
    }

    response[0] = '+';
    response[1] = '\0';
    strncat(response, he->h_name, resp_size - strlen(response) - 1);

    for (char **alias = he->h_aliases; alias != NULL && *alias != NULL; alias++) {
        if (strstr(response, *alias) != NULL) {
            continue;
        }
        size_t remaining = resp_size - strlen(response) - 1;
        if (remaining < strlen(*alias) + 2) {
            break;
        }
        strcat(response, " ");
        strcat(response, *alias);
    }
}

static void process_request(const char *request, char *response, size_t resp_size) {
    if (is_ip_address(request)) {
        resolve_reverse(request, response, resp_size);
    } else {
        resolve_forward(request, response, resp_size);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s PortNumber\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char log_filename[64];
    snprintf(log_filename, sizeof(log_filename), "log_%s.txt", STUDENT_ID);
    FILE *log_fp = fopen(log_filename, "a");
    if (log_fp == NULL) {
        perror("fopen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    char buffer[BUFFER_SIZE];
    char response[RESPONSE_SIZE];
    struct sockaddr_in client_addr;

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                              (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recvfrom");
            continue;
        }
        buffer[n] = '\0';

        /* strip trailing newline/whitespace, if any */
        while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) {
            buffer[--n] = '\0';
        }

        if (n == 0) {
            continue;
        }

        process_request(buffer, response, sizeof(response));

        if (sendto(sockfd, response, strlen(response), 0,
                   (struct sockaddr *)&client_addr, client_len) < 0) {
            perror("sendto");
        }

        log_request(log_fp, buffer, response);
        printf("Request: %s -> Response: %s\n", buffer, response);
    }

    fclose(log_fp);
    close(sockfd);
    return 0;
}
