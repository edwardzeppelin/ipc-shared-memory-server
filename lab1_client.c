#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8081
#define BUFFER_SIZE 1024


int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error: %s\n", strerror(errno));
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    
    printf("Connected to Distributed Memory Server.\n");
    printf("Available commands:\n");
    printf("  READ - read shared memory content\n");
    printf("  WRITE <data> - write data to shared memory\n");
    printf("  CLEAR - clear shared memory\n");
    printf("  exit - quit client\n\n");
    
    while (1) {
        printf("Enter command: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("Input error: %s\n", strerror(errno));
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        if (send(sock, buffer, strlen(buffer), 0) == -1) {
            printf("send failed: %s\n", strerror(errno));
            break;
        }

        int read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (read_size < 0) {
            printf("recv failed: %s\n", strerror(errno));
            break;
        } else if (read_size == 0) {
            printf("Server disconnected\n");
            break;
        }
        
        buffer[read_size] = '\0';
        printf("Server response: %s\n", buffer);
    }
    
    close(sock);
    printf("Client terminated.\n");
    return 0;
}