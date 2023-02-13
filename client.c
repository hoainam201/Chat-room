#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>

#define BUF_SIZE 2048

char line[3][512] = {0};
int line_count = 0;

void print(char *msg) {
    system("clear");

    if (line_count == 3) {
        for (int i = 0; i < 2; i++) {
            strcpy(line[i], line[i + 1]);
        }
        strcpy(line[2], msg);
    } else {
        strcpy(line[line_count], msg);
        line_count++;
    }

    for (int i = 0; i < line_count; i++) {
        printf("%s", line[i]);
    }

    printf("Enter message <Type @leave to leave, @exit to exit>: ");
    fflush(stdout);
}

// thread nhận thông báo từ server
void *recv_thread(void *arg) {
    int sfd = *(int *) arg;
    char *buffer = malloc(BUF_SIZE);
    while (1) {
        int bytes = recv(sfd, buffer, BUF_SIZE, 0);
        buffer[bytes] = '\0';
        print(buffer);
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char buf[512] = {0}, command[BUF_SIZE] = {0};

    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8888);
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("inet_pton");
        return 1;
    }

    if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("connect");
        return 1;
    }

    // Cấu trúc tin nhắn dạng: <command> <data>
    // login abc : đăng nhập với tên abc
    // room abc : chuyển vào room abc nếu đã tồn tại, chưa tồn tại thì tạo room abc
    // msg abc : gửi tin nhắn "abc" đến room hiện tại
    // leave : rời room hiện tại
    // list : danh sách các room

    input_name:
    printf("Enter your name: ");
    fgets(buf, 512, stdin);

    sprintf(command, "login %s", buf);
    send(client_socket, command, strlen(command), 0);
    int bytes = recv(client_socket, buf, BUF_SIZE, 0);
    buf[bytes] = '\0';
    printf("%s", buf);

    if (strstr(buf, "Server: Error") != NULL)
        goto input_name;

    input_room:
    send(client_socket, "list\r\n", 6, 0);
    bytes = recv(client_socket, buf, BUF_SIZE, 0);
    buf[bytes] = '\0';
    printf("%s", buf);

    printf("Enter room name <Type @exit to exit>: ");
    fgets(buf, 512, stdin);

    if (strncmp(buf, "@exit",5) == 0) {
        return 0;
    }

    sprintf(command, "room %s", buf);
    send(client_socket, command, strlen(command), 0);
    bytes = recv(client_socket, buf, BUF_SIZE, 0);
    buf[bytes] = '\0';
    printf("%s", buf);

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, &client_socket);


    while (1) {
        printf("Enter message <Type @leave to leave, @exit to exit>: ");
        fgets(buf, 512, stdin);

        // nếu nhập @leave thì rời room
        if (strncmp(buf, "@leave",6) == 0) {
            send(client_socket, "leave\r\n", 7, 0);
            pthread_cancel(tid);
            goto input_room;
        }

        if (strncmp(buf, "@exit",5) == 0) {
            return 0;
        }

        sprintf(command, "msg %s", buf);
        send(client_socket, command, strlen(command), 0);
    }
}
