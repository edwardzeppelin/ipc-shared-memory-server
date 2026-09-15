#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define PORT 8081
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define SHARED_MEM_SIZE 4096

typedef struct {
    int socket;
    struct sockaddr_in address;
} client_data_t;

int shmid;
char *shared_memory;
pthread_mutex_t memory_mutex = PTHREAD_MUTEX_INITIALIZER;

void cleanup(int sig) {
    printf("\nCleaning up resources...\n");
    
    if (shared_memory != NULL) {
        shmdt(shared_memory);
    }
    
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
        printf("Shared memory destroyed\n");
    }
    
    pthread_mutex_destroy(&memory_mutex);
    exit(0);
}

void *handle_client(void *arg) {
    client_data_t *client_data = (client_data_t *)arg;
    int client_socket = client_data->socket;
    char buffer[BUFFER_SIZE] = {0};
    int read_size;
    
    printf("New connection from %s:%d\n", 
           inet_ntoa(client_data->address.sin_addr), 
           ntohs(client_data->address.sin_port));
    
    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received from client %s:%d: %s\n", 
               inet_ntoa(client_data->address.sin_addr),
               ntohs(client_data->address.sin_port),
               buffer);

        if (strncmp(buffer, "READ", 4) == 0) {

            pthread_mutex_lock(&memory_mutex);
            
            if (strlen(shared_memory) == 0) {
                char *empty_msg = "Shared memory is empty";
                if (send(client_socket, empty_msg, strlen(empty_msg), 0) == -1) {
                    perror("send failed");
                } else {
                    printf("Sent empty memory notification to client %s:%d\n", 
                           inet_ntoa(client_data->address.sin_addr),
                           ntohs(client_data->address.sin_port));
                }
            } else {
                if (send(client_socket, shared_memory, strlen(shared_memory), 0) == -1) {
                    perror("send failed");
                } else {
                    printf("Sent shared memory content to client %s:%d\n", 
                           inet_ntoa(client_data->address.sin_addr),
                           ntohs(client_data->address.sin_port));
                }
            }
            pthread_mutex_unlock(&memory_mutex);
            
        } else if (strncmp(buffer, "WRITE ", 6) == 0) {

            pthread_mutex_lock(&memory_mutex);
            char *data = buffer + 6;
            
            size_t current_len = strlen(shared_memory);
            size_t data_len = strlen(data);
            size_t available_space = SHARED_MEM_SIZE - current_len - 1; // -1 для нуль-терминатора
            
            if (data_len > available_space) {

                char *error_msg = "ERROR: Not enough space in shared memory";
                if (send(client_socket, error_msg, strlen(error_msg), 0) == -1) {
                    perror("send failed");
                } else {
                    printf("Not enough space for client %s:%d write request\n", 
                           inet_ntoa(client_data->address.sin_addr),
                           ntohs(client_data->address.sin_port));
                }
            } else {

                if (current_len > 0) {

                    if (current_len + data_len + 1 < SHARED_MEM_SIZE) {
                        strcat(shared_memory, " ");
                        strncat(shared_memory, data, available_space - 1);
                    }
                } else {

                    strncpy(shared_memory, data, SHARED_MEM_SIZE - 1);
                }
                shared_memory[SHARED_MEM_SIZE - 1] = '\0';
                
                if (send(client_socket, "OK", 2, 0) == -1) {
                    perror("send failed");
                } else {
                    printf("Client %s:%d appended to shared memory: %s\n", 
                           inet_ntoa(client_data->address.sin_addr),
                           ntohs(client_data->address.sin_port),
                           data);
                }
            }
            pthread_mutex_unlock(&memory_mutex);
            
        } else if (strncmp(buffer, "CLEAR", 5) == 0) {

            pthread_mutex_lock(&memory_mutex);
            shared_memory[0] = '\0';
            if (send(client_socket, "CLEARED", 7, 0) == -1) {
                perror("send failed");
                pthread_mutex_unlock(&memory_mutex);
                break;
            }
            pthread_mutex_unlock(&memory_mutex);
            printf("Client %s:%d cleared shared memory\n", 
                   inet_ntoa(client_data->address.sin_addr),
                   ntohs(client_data->address.sin_port));
            
        } else {

            char *error_msg = "ERROR: Unknown command. Use READ, WRITE <data>, or CLEAR";
            if (send(client_socket, error_msg, strlen(error_msg), 0) == -1) {
                perror("send failed");
                break;
            }
        }
        
        memset(buffer, 0, BUFFER_SIZE);
    }
    
    if (read_size == 0) {
        printf("Client %s:%d disconnected\n", 
               inet_ntoa(client_data->address.sin_addr),
               ntohs(client_data->address.sin_port));
    } else if (read_size == -1) {
        perror("recv failed");
    }
    
    close(client_socket);
    free(client_data);
    pthread_exit(NULL);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    key_t key;

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if ((key = ftok("LAB1serverSHM.c", 'R')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    if ((shmid = shmget(key, SHARED_MEM_SIZE, IPC_CREAT | 0666)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    if ((shared_memory = shmat(shmid, NULL, 0)) == (char *)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    shared_memory[0] = '\0';

    printf("Shared memory created with key: %d, shmid: %d\n", key, shmid);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        cleanup(0);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        cleanup(0);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        cleanup(0);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        cleanup(0);
    }
    
    printf("Distributed Memory Server listening on port %d...\n", PORT);
    printf("Available commands:\n");
    printf("  READ - read shared memory content\n");
    printf("  WRITE <data> - write data to shared memory\n");
    printf("  CLEAR - clear shared memory\n");
    printf("Press Ctrl+C to stop server and cleanup resources\n");
    
    while (1) {
        client_data_t *client_data = malloc(sizeof(client_data_t));
        if (client_data == NULL) {
            perror("malloc failed");
            continue;
        }
        
        if ((client_data->socket = accept(server_fd, (struct sockaddr *)&client_data->address, &addrlen)) < 0) {
            perror("accept");
            free(client_data);
            continue;
        }
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_data) != 0) {
            perror("could not create thread");
            close(client_data->socket);
            free(client_data);
            continue;
        }

        pthread_detach(thread_id);
    }
    
    close(server_fd);
    cleanup(0);
    return 0;
}