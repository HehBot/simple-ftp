#include <arpa/inet.h>
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
            return 0;
        bytes_written += last_bytes_written;
        next_loc += last_bytes_written;
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
            return 0;
        bytes_read += last_bytes_read;
        next_loc += last_bytes_read;
    } while (last_bytes_read != 0 && bytes_read != bytes_to_read);
    return bytes_read;
}
uint64_t send_to_fd(int fd, void const* buf, uint64_t bytes_to_write)
{
    if ((write_to_fd(fd, &bytes_to_write, sizeof bytes_to_write) != sizeof bytes_to_write) || (write_to_fd(fd, buf, bytes_to_write) != bytes_to_write))
        return 0;
    return bytes_to_write;
}

char* create_command(char const* op, char const* file_name)
{
    int op_length = strlen(op);
    int file_name_length = strlen(file_name);
    char* command = malloc(op_length + 1 + file_name_length + 1);
    strncpy(command, op, op_length);
    command[op_length] = ' ';
    strncpy(command + op_length + 1, file_name, file_name_length + 1);
    return command;
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        fprintf(stderr, "ftp-client\nUsage: %s <server_ip>:<server_port> <operation> <file_to_recv>\n", argv[0]);
        exit(1);
    }
    int conn_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn_sock_fd == -1) {
        PERROR("ftp-client: Error in creating socket");
        exit(-1);
    }

    char* server_ip = strtok(argv[1], ":");
    uint16_t server_port = atoi(strtok(NULL, ":"));

    struct sockaddr_in server_addr;
    memset(&server_addr.sin_zero, '\0', sizeof server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(conn_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        PERROR("ftp-client: Error connecting");
        close(conn_sock_fd);
        exit(2);
    }

    printf("ConnectDone: %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    char* command = create_command(argv[2], argv[3]);
    uint64_t command_length = strlen(command) + 1;
    if (send_to_fd(conn_sock_fd, command, command_length) != command_length) {
        PERROR("ftp-client: Error writing to socket");
        free(command);
        exit(-1);
    }
    free(command);

    if (!strcmp(argv[2], "get")) {
        uint64_t file_size = 0;
        if (read_from_fd(conn_sock_fd, &file_size, sizeof file_size) != sizeof file_size) {
            PERROR("ftp-client: Error reading from socket");
            close(conn_sock_fd);
            exit(-1);
        }
        if (file_size == 0) {
            PERROR("ftp-client: Error reading file \'%s\' at server", argv[3]);
            close(conn_sock_fd);
            exit(-1);
        }
        char* file_contents = malloc(file_size);
        if (read_from_fd(conn_sock_fd, file_contents, file_size) != file_size) {
            PERROR("ftp-client: Error reading from socket");
            free(file_contents);
            close(conn_sock_fd);
            exit(-1);
        }

        FILE* output_file = fopen(argv[3], "w");
        if (output_file == NULL) {
            PERROR("ftp-client: Error writing to file \'%s\'", argv[3]);
            free(file_contents);
            close(conn_sock_fd);
            exit(3);
        }
        fwrite(file_contents, file_size, 1, output_file);
        fclose(output_file);

        printf("FileWritten: %lu bytes\n", file_size);

        free(file_contents);
    } else if (!strcmp(argv[2], "put")) {
        FILE* input_file = fopen(argv[3], "r");
        if (input_file == NULL) {
            PERROR("ftp-client: Error reading file \'%s\'", argv[3]);
            close(conn_sock_fd);
            exit(3);
        }

        fseek(input_file, 0, SEEK_END);
        long input_file_size = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);
        char* buffer = malloc(input_file_size);
        fread(buffer, input_file_size, 1, input_file);
        fclose(input_file);
        input_file = NULL;

        if (send_to_fd(conn_sock_fd, buffer, input_file_size) != input_file_size) {
            PERROR("ftp-client: Error writing to socket");
            free(buffer);
            close(conn_sock_fd);
            exit(-1);
        }
        free(buffer);
    } else {
        PERROR("ftp-client: Error, command %s not recognised", argv[2]);
        close(conn_sock_fd);
        exit(-1);
    }
    close(conn_sock_fd);
    return 0;
}
