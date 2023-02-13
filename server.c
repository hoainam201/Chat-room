#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>

#define MAX_CLIENTS 100
#define MAX_ROOMS 10
#define BUF_SIZE 2048

struct client {
    int socket;
    char name[50];
    int room;
};

struct room {
    int clients[MAX_CLIENTS];
    char name[50];
    int count;
};

// gửi tin nhắn đến tất cả client trong room
void send_to_room(struct client *clients, struct room *rooms, int room_id, int sender, char *msg) {
    for (int i = 0; i < rooms[room_id].count; i++) {
        int client_id = rooms[room_id].clients[i];

        // nếu không phải là người gửi thì gửi
        if (client_id != sender) {
            send(clients[client_id].socket, msg, strlen(msg), 0);
        }
    }
}

// lấy room id từ tên room
int get_roomid_by_name(struct room *rooms, int room_count, char *name) {
    for (int i = 0; i < room_count; i++) {
        if (strcmp(rooms[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void logger(char *msg) {
    printf("%s", msg);
    fflush(stdout);
}

int main() {
    logger("Starting server...\n");
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_len;
    char buf[BUF_SIZE] = {0};
    struct client clients[MAX_CLIENTS];
    struct room rooms[MAX_ROOMS];
    int client_count = 0, room_count = 0;
    fd_set readfds;

    // tạo socket cho server
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    // cấu hình địa chỉ cho server
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(8888);

    // bind socket với địa chỉ
    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("bind");
        return 1;
    }

    // listen
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        return 1;
    }

    logger("Server started\n");
    // Main server loop
    while (1) {
        // xóa các socket trong set
        FD_ZERO(&readfds);

        // thêm server socket vào set
        FD_SET(server_socket, &readfds);

        // thêm các client socket vào set
        for (int i = 0; i < client_count; i++) {
            client_socket = clients[i].socket;
            if (client_socket > 0) {
                FD_SET(client_socket, &readfds);
            }
        }


        int activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
        if ((activity < 0)) {
            printf("select error");
        }

        // handle kết nối mới
        if (FD_ISSET(server_socket, &readfds)) {
            // chấp nhận kết nối mới
            client_len = sizeof(client_address);
            client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_len);
            if (client_socket < 0) {
                perror("accept");
                return 1;
            }

            // thêm client vào danh sách
            clients[client_count].socket = client_socket;
            clients[client_count].room = -1;
            client_count++;
        }

        // handle tin nhắn từ client
        for (int i = 0; i < client_count; i++) {
            client_socket = clients[i].socket;

            // nếu có tin nhắn từ client
            if (FD_ISSET(client_socket, &readfds)) {
                memset(buf, 0, BUF_SIZE);

                // nhận tin nhắn
                int bytes = recv(client_socket, buf, BUF_SIZE, 0);
                if (bytes == 0) {
                    // Connection closed by client
                    if (clients[i].room != -1) {
                        char noti[BUF_SIZE] = {0};
                        send_to_room(clients, rooms, clients[i].room, i, noti);
                        logger(noti);
                        int is_remove = 0;
                        for (int j = 0; j < rooms[clients[i].room].count; ++j) {
                            if (rooms[clients[i].room].clients[j] == i) {
                                is_remove = 1;
                            }
                            if (is_remove) {
                                rooms[clients[i].room].clients[j] = rooms[clients[i].room].clients[j + 1];
                            }
                        }
                        rooms[clients[i].room].count--;
                        clients[i].room = -1;
                    }
                    close(client_socket);
                } else {
                    // Cấu trúc tin nhắn dạng: <command> <data>
                    // login abc : đăng nhập với tên abc
                    // room abc : chuyển vào room abc nếu đã tồn tại, chưa tồn tại thì tạo room abc
                    // msg abc : gửi tin nhắn "abc" đến room hiện tại
                    // leave : rời room hiện tại
                    // list : danh sách các room

                    char command[10] = {0}, noti[BUF_SIZE] = {0};
                    char *data = malloc(BUF_SIZE);
                    memset(data, 0, BUF_SIZE);

                    sscanf(strdup(buf), "%s %s", command, data);

                    
                    if (strncmp(data, buf, strlen(data)) == 0) {
                        data = strstr(buf + 1, data);
                    } else
                        data = strstr(buf, data);


                    if (data != NULL) {
                        data[strlen(data) - 1] = '\0';
                    }

                    if (strcmp(command, "login") == 0) {
                        // check username đã tồn tại chưa
                        int existed = 0;
                        for (int j = 0; j < client_count; j++) {
                            if (strcmp(clients[j].name, data) == 0) {
                                existed = 1;
                                break;
                            }
                        }
                        if (existed) {
                            sprintf(noti, "Server: Error Tên %s đã tồn tại, vui lòng chọn tên khác\n", data);
                        } else {

                            strcpy(clients[i].name, data);
                            sprintf(noti, "Server: %s đã tham gia phòng chat\n", clients[i].name);
                        }

                        send(client_socket, noti, strlen(noti), 0);
                        logger(noti);
                    } else if (strcmp(command, "room") == 0) {
                        int room = get_roomid_by_name(rooms, room_count, data);
                        if (room < 0) {
                            strcpy(rooms[room_count].name, data);
                            rooms[room_count].count = 0;
                            room = room_count;
                            room_count++;
                            sprintf(noti, "Server: %s đã tạo phòng %s\n", clients[i].name, data);
                        } else {
                            sprintf(noti, "Server: %s đã tham gia phòng %s\n", clients[i].name, data);
                            send_to_room(clients, rooms, room, -1, noti);
                        }
                        clients[i].room = room;
                        rooms[room].clients[rooms[room].count] = i;
                        rooms[room].count++;
                        send(client_socket, noti, strlen(noti), 0);
                        logger(noti);
                    } else if (strcmp(command, "msg") == 0) {
                        if (clients[i].room < 0) {
                            sprintf(noti, "Server: %s chưa tham gia phòng chat nào\n", clients[i].name);
                            send(client_socket, noti, strlen(noti), 0);
                        } else {
                            sprintf(noti, "%s| %s: %s\n", rooms[clients[i].room].name, clients[i].name, data);
                            send_to_room(clients, rooms, clients[i].room, i, noti);
                        }
                        logger(noti);
                    } else if (strcmp(command, "leave") == 0) {
                        sprintf(noti, "Server: %s đã thoát phòng chat\n", clients[i].name);
                        send_to_room(clients, rooms, clients[i].room, i, noti);
                        logger(noti);

                        int is_remove = 0;
                        for (int j = 0; j < rooms[clients[i].room].count; ++j) {
                            if (rooms[clients[i].room].clients[j] == i) {
                                is_remove = 1;
                            }
                            if (is_remove) {
                                rooms[clients[i].room].clients[j] = rooms[clients[i].room].clients[j + 1];
                            }
                        }
                        rooms[clients[i].room].count--;
                        clients[i].room = -1;

                    } else if (strcmp(command, "list") == 0) {
                        sprintf(noti, "Server: Danh sách phòng chat\n");
                        for (int j = 0; j < room_count; ++j) {
                            sprintf(noti + strlen(noti), "%s (%d)\n", rooms[j].name, rooms[j].count);
                        }
                        if (room_count == 0) {
                            sprintf(noti + strlen(noti), "Không có phòng chat nào\n");
                        }
                        send(client_socket, noti, strlen(noti), 0);
                    } else {
                        sprintf(noti, "Server: Lệnh không hợp lệ\n");
                        send(client_socket, noti, strlen(noti), 0);
                        logger(noti);
                    }

                }
            }
        }
    }
}

