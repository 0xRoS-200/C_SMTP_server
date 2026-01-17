#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <process.h>
#include <direct.h>

#pragma comment(lib, "Ws2_32.lib")

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

void send_reply(SOCKET sock, const char *msg) {
    send(sock, msg, (int)strlen(msg), 0);
}

int recv_line_buffered(SOCKET sock, char *out_buf, int out_size) {
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

unsigned __stdcall ClientHandler(void* data) {
    SOCKET client = *(SOCKET*)data;
    free(data); 

    char buf[BUF_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    
    getpeername(client, (struct sockaddr*)&addr, &addr_len);
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    log_msg(client_ip, "Connected");
    send_reply(client, "220 MyCustomSMTP Service Ready\r\n");

    FILE *mail_file = NULL;
    int in_data_mode = 0;

    while (1) {
        int n = recv_line_buffered(client, buf, BUF_SIZE);
        if (n < 0) break;

        if (in_data_mode) {
            if (strcmp(buf, ".") == 0) {
                in_data_mode = 0;
                if (mail_file) fclose(mail_file);
                log_msg(client_ip, "Email received and saved.");
                send_reply(client, "250 OK Message accepted\r\n");
            } else {
                if (buf[0] == '.') fprintf(mail_file, "%s\n", buf + 1);
                else fprintf(mail_file, "%s\n", buf);
            }
            continue;
        }

        // Command Logic
        if (!_strnicmp(buf, "HELO", 4) || !_strnicmp(buf, "EHLO", 4)) {
            send_reply(client, "250 Hello\r\n");
        } 
        else if (!_strnicmp(buf, "MAIL FROM:", 10)) {
            log_msg(client_ip, "FROM: %s", buf + 10);
            send_reply(client, "250 OK\r\n");
        } 
        else if (!_strnicmp(buf, "RCPT TO:", 8)) {
            log_msg(client_ip, "TO: %s", buf + 8);
            send_reply(client, "250 OK\r\n");
        } 
        else if (!_stricmp(buf, "DATA")) {
            send_reply(client, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
            

            char filename[64];
            sprintf(filename, "mailbox\\mail_%ld.txt", time(NULL));
            mail_file = fopen(filename, "w");
            
            if (mail_file == NULL) {
                log_msg(client_ip, "ERROR: Storage failed");
                send_reply(client, "451 Local error\r\n");
                break;
            }
            in_data_mode = 1;
        } 
        else if (!_stricmp(buf, "QUIT")) {
            send_reply(client, "221 Bye\r\n");
            break;
        } 
        else {
            send_reply(client, "500 Command not recognized\r\n");
        }
    }

    if (mail_file) fclose(mail_file);
    closesocket(client);
    log_msg(client_ip, "Disconnected");
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET server, client;
    struct sockaddr_in addr;

    _mkdir("mailbox");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 1;

    server = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Bind failed. Run as Administrator? Is port 25 busy?\n");
        return 1;
    }

    listen(server, 100);
    log_msg(NULL, "SMTP Server running on port %d...", PORT);

    while(1) {
        client = accept(server, NULL, NULL);
        if (client != INVALID_SOCKET) {
            SOCKET *new_sock = malloc(sizeof(SOCKET));
            *new_sock = client;
            
            _beginthreadex(NULL, 0, ClientHandler, new_sock, 0, NULL);
        }
    }

    closesocket(server);
    WSACleanup();
    return 0;
}