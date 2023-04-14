#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PERROR(S, ...) fprintf(stderr, S "\n", ##__VA_ARGS__)

uint64_t write_to_fd(int fd, void const* buf, uint64_t bytes_to_write)
{
    uint64_t last_bytes_written = 0, bytes_written = 0;
    char const* next_loc = (char const*)buf;
    do {
        last_bytes_written = write(fd, next_loc, bytes_to_write - bytes_written);
        if (last_bytes_written == -1)
            break;
        bytes_written += last_bytes_written;
    } while (last_bytes_written != 0 && bytes_written != bytes_to_write);
    return bytes_written;
}
uint64_t read_from_fd(int fd, void* buf, uint64_t bytes_to_read)
{
    uint64_t last_bytes_read = 0, bytes_read = 0;
    char* next_loc = (char*)buf;
    do {
        last_bytes_read = read(fd, next_loc, bytes_to_read - bytes_read);
        if (last_bytes_read == -1)
            break;
        bytes_read += last_bytes_read;
    } while (last_bytes_read != 0 && bytes_read != bytes_to_read);
    return bytes_read;
}
uint64_t send_to_fd(int fd, void const* buf, uint64_t bytes_to_write)
{
    if ((write_to_fd(fd, &bytes_to_write, sizeof bytes_to_write) != sizeof bytes_to_write) || (write_to_fd(fd, buf, bytes_to_write) != bytes_to_write))
        return 0;
    return bytes_to_write;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        PERROR("ftp-server\nUsage: %s <port_num>", argv[0]);
        exit(1);
    }
    uint16_t port_num = atoi(argv[1]);
    int accepting_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (accepting_sock_fd == -1) {
        PERROR("ftp-server: Error in creating socket");
        exit(-1);
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(accepting_sock_fd, (struct sockaddr*)&server_addr, sizeof server_addr) == -1) {
        close(accepting_sock_fd);
        PERROR("ftp-server: Error in binding socket to port %d", port_num);
        exit(2);
    }
    printf("BindDone: %d\n", port_num);
    if (listen(accepting_sock_fd, 3) == -1) {
        close(accepting_sock_fd);
        PERROR("ftp-server: Error in listening on socket");
        exit(-1);
    }
    printf("ListenDone: %d\n", port_num);

#define PCAP_INC 10
    int pcap = 3;
    struct pollfd* pfd = (struct pollfd*)malloc(pcap * (sizeof *pfd));
    pfd[0].fd = accepting_sock_fd;
    pfd[0].events = POLLIN;
    pfd[0].revents = 0;
    for (int i = 1; i < pcap; ++i) {
        pfd[i].fd = -1;
        pfd[i].events = 0;
        pfd[i].revents = 0;
    }

    while (poll(pfd, pcap, 0) >= 0) {
        if (pfd[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_size = sizeof client_addr;
            int conn_sock_fd = accept(accepting_sock_fd, (struct sockaddr*)&client_addr, &client_addr_size);
            if (conn_sock_fd == -1) {
                PERROR("ftp-server: Error in accepting connection: %s", strerror(errno));
                continue;
            }
            int i = 1;
            for (; i < pcap; ++i) {
                if (pfd[i].fd == -1)
                    break;
            }
            if (i == pcap) {
                pcap += PCAP_INC;
                pfd = (struct pollfd*)realloc(pfd, pcap * (sizeof *pfd));
                for (int j = i; j < pcap; ++j) {
                    pfd[j].fd = -1;
                    pfd[j].events = 0;
                    pfd[j].revents = 0;
                }
            }
            pfd[i].fd = conn_sock_fd;
            pfd[i].events = POLLIN | POLLOUT;
            printf("Client: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
        for (int i = 1; i < pcap; ++i) {
            if (pfd[i].fd >= 0 && (pfd[i].revents & (POLLIN | POLLOUT))) {
                uint64_t command_length = 0;
                if (read_from_fd(pfd[i].fd, &command_length, sizeof command_length) != sizeof command_length) {
                    PERROR("ftp-server: Error reading from socket");
                    close(pfd[i].fd);
                    continue;
                }
                char* command = (char*)malloc(command_length);
                if (read_from_fd(pfd[i].fd, command, command_length) != command_length) {
                    PERROR("ftp-server: Error reading from socket");
                    free(command);
                    close(pfd[i].fd);
                    pfd[i].fd = -1;
                    continue;
                }
                char* op = strtok(command, " ");
                if (!strcmp(op, "get")) {
                    char* file_name = strtok(NULL, " ");
                    FILE* input_file = fopen(file_name, "r");
                    if (input_file == NULL) {
                        printf("FileTransferFail\n");
                        PERROR("ftp-server: Error reading file \'%s\'", file_name);
                        uint64_t zero = 0;
                        if (write_to_fd(pfd[i].fd, &zero, sizeof zero) != sizeof zero) {
                            free(command);
                            close(pfd[i].fd);
                            pfd[i].fd = -1;
                            PERROR("ftp-server: Error writing to socket");
                            continue;
                        }
                        free(command);
                        close(pfd[i].fd);
                        pfd[i].fd = -1;
                        continue;
                    }
                    free(command);

                    fseek(input_file, 0, SEEK_END);
                    long input_file_size = ftell(input_file);
                    fseek(input_file, 0, SEEK_SET);
                    char* buffer = (char*)malloc(input_file_size);
                    fread(buffer, input_file_size, 1, input_file);
                    fclose(input_file);
                    input_file = NULL;

                    if (send_to_fd(pfd[i].fd, buffer, input_file_size) != input_file_size) {
                        free(buffer);
                        close(pfd[i].fd);
                        pfd[i].fd = -1;
                        PERROR("ftp-server: Error writing to socket");
                        continue;
                    }
                    printf("TransferDone: %lu bytes\n", input_file_size);
                    free(buffer);
                    close(pfd[i].fd);
                    pfd[i].fd = -1;
                } else if (!strcmp(op, "put")) {
                    char* file_name = strtok(NULL, " ");
                    uint64_t output_file_size;
                    if (read_from_fd(pfd[i].fd, &output_file_size, sizeof output_file_size) != sizeof output_file_size) {
                        free(command);
                        close(pfd[i].fd);
                        pfd[i].fd = -1;
                        PERROR("ftp-server: Error reading from socket");
                        continue;
                    }
                    char* output_file_contents = (char*)malloc(output_file_size);
                    if (read_from_fd(pfd[i].fd, output_file_contents, output_file_size) != output_file_size) {
                        free(output_file_contents);
                        free(command);
                        close(pfd[i].fd);
                        pfd[i].fd = -1;
                        PERROR("ftp-server: Error reading from socket");
                        continue;
                    }
                    FILE* output_file = fopen(file_name, "w");
                    if (output_file == NULL) {
                        free(output_file_contents);
                        free(command);
                        close(pfd[i].fd);
                        pfd[i].fd = -1;
                        PERROR("ftp-server: Error writing to file \'%s\'", file_name);
                        continue;
                    }
                    free(command);
                    fwrite(output_file_contents, output_file_size, 1, output_file);
                    fclose(output_file);
                    free(output_file_contents);
                    close(pfd[i].fd);
                    pfd[i].fd = -1;
                } else {
                    free(command);
                    close(pfd[i].fd);
                    pfd[i].fd = -1;
                    printf("UnknownCmd\n");
                    PERROR("ftp-server: Error, command %s not recognised", op);
                    continue;
                }
            }
        }
    }
    return 0;
}