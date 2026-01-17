#include <stdio.h>
#include <stdlib.h>
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

int recv_line_buffered(int sock, char *out_buf, int out_size) {
    int i = 0;
    char c = 0;
    while (i < out_size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return -1;
        if (c == '\n') {
            out_buf[i] = '\0';
            return i;
        }
        if (c != '\r') out_buf[i++] = c;
    }
    out_buf[i] = '\0';
    return i;
}

void* ClientHandler(void* socket_desc) {
    int client = *(int*)socket_desc;
    free(socket_desc);
    
    char buf[BUF_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    getpeername(client, (struct sockaddr*)&addr, &addr_len);
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    log_msg(client_ip, "Connected");
    send_reply(client, "220 LinuxSMTP Service Ready\r\n");
    
    FILE *mail_file = NULL;
    int in_data_mode = 0;
    
    while (1) {
        int n = recv_line_buffered(client, buf, BUF_SIZE);
        if (n < 0) break;
        
        if (in_data_mode) {
            if (strcmp(buf, ".") == 0) {
                in_data_mode = 0;
                if (mail_file) fclose(mail_file);
                log_msg(client_ip, "Email saved.");
                send_reply(client, "250 OK Message accepted\r\n");
            } else {
                if (buf[0] == '.') fprintf(mail_file, "%s\n", buf + 1);
                else fprintf(mail_file, "%s\n", buf);
            }
            continue;
        }
        
        if (strncasecmp(buf, "HELO", 4) == 0 || strncasecmp(buf, "EHLO", 4) == 0) {
            send_reply(client, "250 Hello\r\n");
        } else if (strncasecmp(buf, "MAIL FROM:", 10) == 0) {
            log_msg(client_ip, "FROM: %s", buf + 10);
            send_reply(client, "250 OK\r\n");
        } else if (strncasecmp(buf, "RCPT TO:", 8) == 0) {
            log_msg(client_ip, "TO: %s", buf + 8);
            send_reply(client, "250 OK\r\n");
        } else if (strcasecmp(buf, "DATA") == 0) {
            send_reply(client, "354 End with <CRLF>.<CRLF>\r\n");
            char filename[64];
            sprintf(filename, "mailbox/mail_%ld.txt", time(NULL));
            mail_file = fopen(filename, "w");
            if (!mail_file) {
                log_msg(client_ip, "ERROR: File write failed");
                break;
            }
            in_data_mode = 1;
        } else if (strcasecmp(buf, "QUIT") == 0) {
            send_reply(client, "221 Bye\r\n");
            break;
        } else {
            send_reply(client, "500 Command not recognized\r\n");
        }
    }
    if (mail_file) fclose(mail_file);
    close(client);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server, client;
    
    mkdir("mailbox", 0777);
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed. Are you root?");
        return 1;
    }
    
    listen(server_fd, 3);
    log_msg(NULL, "Linux SMTP Server running on port %d...", PORT);
    
    int c = sizeof(struct sockaddr_in);
    while ((client_fd = accept(server_fd, (struct sockaddr *)&client, (socklen_t*)&c))) {
        int *new_sock = malloc(1);
        *new_sock = client_fd;
        pthread_t sniffer_thread;
        if (pthread_create(&sniffer_thread, NULL, ClientHandler, (void*)new_sock) < 0) {
            perror("Could not create thread");
            return 1;
        }
    }
    return 0;
}