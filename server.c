#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <time.h>
#include <direct.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 2525
#define BUF_SIZE 1024

void send_reply(SOCKET sock, const char *msg) {
    send(sock, msg, (int)strlen(msg), 0);
}

// Fixed: Added robustness to recv_line logic
int recv_line(SOCKET sock, char *buf, int size) {
    int i = 0;
    char c = 0;

    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return n; // Propagate error/disconnect immediately

        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }

    buf[i] = '\0';
    return i;
}

int main() {
    WSADATA wsa;
    SOCKET server, client;
    struct sockaddr_in addr;
    char buf[BUF_SIZE];

    _mkdir("mailbox");

    WSAStartup(MAKEWORD(2,2), &wsa);

    server = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 5);

    printf("SMTP server running on port %d...\n", PORT);

    // FIX: Allow the server to handle multiple emails (Loop accept)
    while(1) {
        printf("Waiting for connection...\n");
        client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) break;

        printf("Client connected.\n");
        send_reply(client, "220 localhost Windows SMTP Ready\r\n");

        while (1) {
            memset(buf, 0, BUF_SIZE);
            if (recv_line(client, buf, BUF_SIZE) <= 0) {
                printf("Connection dropped by client.\n");
                break;
            }

            // printf("C: %s\n", buf); // Optional: Print incoming commands

            if (!_strnicmp(buf, "HELO", 4) || !_strnicmp(buf, "EHLO", 4)) {
                send_reply(client, "250 Hello\r\n");

            } else if (!_strnicmp(buf, "MAIL FROM:", 10)) {
                send_reply(client, "250 OK\r\n");

            } else if (!_strnicmp(buf, "RCPT TO:", 8)) {
                send_reply(client, "250 OK\r\n");

            } else if (!_stricmp(buf, "DATA")) {
                send_reply(client, "354 End data with <CR><LF>.<CR><LF>\r\n");

                // --- CRITICAL FIX 1: Check if file opened successfully ---
                FILE *mail = fopen("mailbox\\mail.txt", "a"); // Changed "w" to "a" (append) to prevent overwriting
                if (mail == NULL) {
                    printf("ERROR: Could not open file 'mailbox\\mail.txt'. Is it open in another program?\n");
                    send_reply(client, "451 Local error: cannot write to file\r\n");
                    break; // Break the command loop
                }

                while (1) {
                    // --- CRITICAL FIX 2: Check for disconnection inside DATA loop ---
                    int n = recv_line(client, buf, BUF_SIZE);
                    if (n <= 0) break; 

                    if (!strcmp(buf, "."))
                        break;
                    
                    fprintf(mail, "%s\n", buf);
                }

                fclose(mail);
                send_reply(client, "250 Message accepted\r\n");
                printf("Email saved to mailbox\\mail.txt\n");

            } else if (!_stricmp(buf, "QUIT")) {
                send_reply(client, "221 Bye\r\n");
                break;

            } else {
                send_reply(client, "500 Command not recognized\r\n");
            }
        }
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}