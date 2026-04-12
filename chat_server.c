#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    char name[50];
    int is_authenticated;
} Client;

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(8888), INADDR_ANY};
    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 5);

    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    fd_set master_fds;
    FD_ZERO(&master_fds);
    FD_SET(listener, &master_fds);
    int max_fd = listener;

    while (1) {
        fd_set read_fds = master_fds;
        select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    int client_fd = accept(listener, NULL, NULL);
                    FD_SET(client_fd, &master_fds);
                    if (client_fd > max_fd) max_fd = client_fd;
                    
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == -1) {
                            clients[j].fd = client_fd;
                            clients[j].is_authenticated = 0;
                            break;
                        }
                    }
                    send(client_fd, "Nhap ten theo cu phap 'client_id: client_name':\n", 48, 0);
                } else {
                    char buf[BUFFER_SIZE];
                    int ret = recv(i, buf, sizeof(buf) - 1, 0);
                    if (ret <= 0) {
                        close(i);
                        FD_CLR(i, &master_fds);
                        for (int j = 0; j < MAX_CLIENTS; j++) if (clients[j].fd == i) clients[j].fd = -1;
                    } else {
                        buf[ret] = '\0';
                        int idx = -1;
                        for (int j = 0; j < MAX_CLIENTS; j++) if (clients[j].fd == i) idx = j;

                        if (!clients[idx].is_authenticated) {
                            char id[50], name[50];
                            if (sscanf(buf, "client_id: %s", name) == 1) {
                                strcpy(clients[idx].name, name);
                                clients[idx].is_authenticated = 1;
                                send(i, "Dang nhap thanh cong!\n", 22, 0);
                            } else {
                                send(i, "Sai cu phap. Nhap lai 'client_id: client_name':\n", 48, 0);
                            }
                        } else {
                            time_t t = time(NULL);
                            struct tm *tm_info = localtime(&t);
                            char time_str[26], send_buf[BUFFER_SIZE + 100];
                            strftime(time_str, 26, "%Y/%m/%d %I:%M:%S%p", tm_info);
                            sprintf(send_buf, "%s %s: %s", time_str, clients[idx].name, buf);

                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients[j].fd != -1 && clients[j].fd != listener && clients[j].fd != i && clients[j].is_authenticated) {
                                    send(clients[j].fd, send_buf, strlen(send_buf), 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}