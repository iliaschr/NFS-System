#include "../include/common.h"
#include "../include/nfs_client_logic.h"

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: %s -p <port_number>\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[2]);
    if (port <= 0) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        return 1;
    }
    
    printf("Starting nfs_client on port %d\n", port);
    
    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        return 1;
    }
    
    printf("nfs_client listening on port %d\n", port);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            fprintf(stderr, "Error accepting connection: %s\n", strerror(errno));
            continue;
        }
        
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Handle client in the same thread for simplicity
        // In a production system, you might want to use separate threads
        handle_client_connection(client_fd);
        
        printf("Client disconnected\n");
    }
    
    close(server_fd);
    return 0;
}