#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>  // Required for logging
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#define PORT 25
#define BUF_SIZE 4096

// --- Logger Helper ---
void log_msg(const char* client_ip, const char* format, ...) {
    char time_buf[64];
    time_t now = time(NULL);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("[%s] [%s] ", time_buf, client_ip ? client_ip : "SERVER");

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void send_reply(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

// --- Buffered Line Reader ---
int recv_line_buffered(int sock, char *out_buf, int out_size) {
    int i = 0;
    char c = 0;
    while (i < out_size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return -1; // Error or disconnect

        if (c == '\n') {
            out_buf[i] = '\0';
            return i;
        }
        if (c != '\r') out_buf[i++] = c;
    }
    out_buf[i] = '\0';
    return i;
}

// --- Client Thread Handler ---
void* ClientHandler(void* socket_desc) {
    int client = *(int*)socket_desc;
    free(socket_desc); // Free the memory allocated in main

    char buf[BUF_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    // Get client IP address
    getpeername(client, (struct sockaddr*)&addr, &addr_len);
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    log_msg(client_ip, "Connected");
    send_reply(client, "220 LinuxSMTP Service Ready\r\n");

    FILE *mail_file = NULL;
    int in_data_mode = 0;

    while (1) {
        int n = recv_line_buffered(client, buf, BUF_SIZE);
        if (n < 0) break; // Client disconnected

        // --- DATA MODE: Receiving the actual Email ---
        if (in_data_mode) {
            // Check for the end of the email (a single dot on a line)
            if (strcmp(buf, ".") == 0) {
                in_data_mode = 0;
                if (mail_file) {
                    fclose(mail_file);
                    mail_file = NULL; // FIX: Prevent double-free crash
                }
                printf("--------------------------------------------------\n");
                log_msg(client_ip, "Email completely received and saved.");
                send_reply(client, "250 OK Message accepted\r\n");
            } else {
                // Remove dot-stuffing (standard SMTP rule)
                char *line_to_write = (buf[0] == '.') ? buf + 1 : buf;

                // 1. Write to File
                if (mail_file) fprintf(mail_file, "%s\n", line_to_write);
                
                // 2. Print to Console (So you can read it live!)
                if (strncasecmp(line_to_write, "Subject:", 8) == 0) {
                    printf("\n>>> INCOMING SUBJECT: %s\n", line_to_write + 8);
                } else {
                    // Print normal body lines slightly indented
                    printf("    %s\n", line_to_write);
                }
            }
            continue;
        }

        // --- COMMAND MODE: Handling HELO, MAIL FROM, RCPT TO ---
        if (strncasecmp(buf, "HELO", 4) == 0 || strncasecmp(buf, "EHLO", 4) == 0) {
            send_reply(client, "250 Hello\r\n");

        } else if (strncasecmp(buf, "MAIL FROM:", 10) == 0) {
            log_msg(client_ip, "FROM: %s", buf + 10);
            send_reply(client, "250 OK\r\n");

        } else if (strncasecmp(buf, "RCPT TO:", 8) == 0) {
            log_msg(client_ip, "TO: %s", buf + 8);
            send_reply(client, "250 OK\r\n");

        } else if (strcasecmp(buf, "DATA") == 0) {
            send_reply(client, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");

            // Create a unique filename
            char filename[64];
            sprintf(filename, "mailbox/mail_%ld.txt", time(NULL));
            mail_file = fopen(filename, "w");

            if (mail_file == NULL) {
                log_msg(client_ip, "ERROR: Storage failed. Check permissions.");
                send_reply(client, "451 Local error\r\n");
                break;
            }
            in_data_mode = 1;
            printf("--- START OF EMAIL CONTENT ---\n");

        } else if (strcasecmp(buf, "QUIT") == 0) {
            send_reply(client, "221 Bye\r\n");
            break;

        } else {
            send_reply(client, "500 Command not recognized\r\n");
        }
    }

    if (mail_file) fclose(mail_file); // Final cleanup if they disconnect early
    close(client);
    log_msg(client_ip, "Disconnected");
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server, client;

    // Create mailbox directory (Linux style permissions)
    mkdir("mailbox", 0777);

    // Create Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Could not create socket");
        return 1;
    }

    // Set socket option to reuse address (Prevents "Address already in use" error after restart)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed. Are you running as root (sudo)?");
        return 1;
    }

    // Listen
    listen(server_fd, 10);
    log_msg(NULL, "Linux SMTP Server running on port %d...", PORT);

    // Accept Incoming Connections
    int c = sizeof(struct sockaddr_in);
    while ((client_fd = accept(server_fd, (struct sockaddr *)&client, (socklen_t*)&c))) {
        
        // Allocate memory for the socket pointer to pass to thread
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_fd;

        pthread_t sniffer_thread;
        if (pthread_create(&sniffer_thread, NULL, ClientHandler, (void*)new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
            return 1;
        }
        
        pthread_detach(sniffer_thread);
    }

    return 0;
}