#include <stdio.h>
#include <stdlib.h> //hàm exit, atoi, malloc
#include <string.h> //tvien xử lý chuỗi: strlen, strcmp
#include <sys/socket.h> //các hàm socket chính
#include <arpa/inet.h> //htons, inet_ntoa
#include <unistd.h> //dùng cho hàm close
#include <sys/select.h> //tvien quan trọng cho hàm select

#define BUFFER_SIZE 1024

// cấu trúc để lưu trạng thái tưng client
typedef struct {
    int fd;
    int is_authenticated; // 0: chưa đăng nhập, 1: rồi
} ClientInfo;

int main(int argc, char *argv[]) {
    // 1. Check tham số đầu vào
    if (argc != 2) {
        printf("Sử dụng: %s <Cổng>\n", argv[0]);
        exit(1);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("Lỗi tạo socket"); exit(1); }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Lỗi bind");
        exit(1);
    }
    listen(server_fd, 5);

    printf("Telnet Server đang đợi kết nối ở cổng %s...\n", argv[1]);

    // Khởi tạo tập fd cho select
    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    int max_fd = server_fd;

    ClientInfo clients[FD_SETSIZE];
    for (int i = 0; i < FD_SETSIZE; i++) clients[i].fd = -1;

    while (1) {
        read_fds = master_fds;
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Lỗi select");
            break;
        }

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    // Có kết nối mới
                    int client_fd = accept(server_fd, NULL, NULL);
                    FD_SET(client_fd, &master_fds);
                    if (client_fd > max_fd) max_fd = client_fd;
                    
                    clients[client_fd].fd = client_fd;
                    clients[client_fd].is_authenticated = 0;
                    
                    char *msg = "Hay gui user va pass (vi du: admin admin):\n";
                    send(client_fd, msg, strlen(msg), 0);
                } else {
                    // Có dữ liệu từ client
                    char buf[BUFFER_SIZE];
                    int bytes = recv(i, buf, sizeof(buf) - 1, 0);
                    
                    if (bytes <= 0) {
                        printf("Client ngắt kết nối.\n");
                        close(i);
                        FD_CLR(i, &master_fds);
                        clients[i].fd = -1;
                    } else {
                        buf[bytes] = '\0';
                        // xóa kí tự xuống dòng nếu có
                        if (buf[bytes-1] == '\n') buf[bytes-1] = '\0';
                        if (buf[bytes-2] == '\r') buf[bytes-2] = '\0';

                        if (clients[i].is_authenticated == 0) {
                            // Xử lý đăng nhập, so sánh với file text
                            FILE *f = fopen("accounts.txt", "r");
                            if (f == NULL) {
                                char *err = "Lỗi sever ko có file acc.\n";
                                send(i, err, strlen(err), 0);
                                continue;
                            }

                            char db_user[50], db_pass[50];
                            int found = 0;
                            while (fscanf(f, "%s %s", db_user, db_pass) != EOF) {
                                char input_auth[100];
                                sprintf(input_auth, "%s %s", db_user, db_pass);
                                if (strcmp(buf, input_auth) == 0) {
                                    found = 1;
                                    break;
                                }
                            }
                            fclose(f);

                            if (found) {
                                clients[i].is_authenticated = 1;
                                char *ok = "Dang nhap ok. Hay nhap lenh:\n";
                                send(i, ok, strlen(ok), 0);
                            } else {
                                char *no = "Sai rùi, nhap lai di:\n";
                                send(i, no, strlen(no), 0);
                            }
                        } else {
                            // Đã login, thực hiện lệnh hệ thống
                            char cmd[BUFFER_SIZE + 20];
                            sprintf(cmd, "%s > out.txt", buf);
                            system(cmd); // Chạy lệnh thui

                            // Đọc file out.txt rùi gửi lại
                            FILE *f_out = fopen("out.txt", "r");
                            if (f_out) {
                                char res[BUFFER_SIZE];
                                while (fgets(res, sizeof(res), f_out)) {
                                    send(i, res, strlen(res), 0);
                                }
                                fclose(f_out);
                            }
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}