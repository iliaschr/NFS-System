#include "../include/common.h"
#include "../include/nfs_client_logic.h"

void handle_list_command(int client_fd, const char *dir_path) {
    // Strip leading '/' to make path relative
    const char *relative_path = dir_path;
    if (dir_path[0] == '/') {
        relative_path = dir_path + 1;  // Skip the leading '/'
    }
    
    DIR *dir = opendir(relative_path);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", relative_path, strerror(errno));
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {  // Skip hidden files and . .. entries
            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s/%s", relative_path, entry->d_name);  // Use relative_path here
            
            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                char response[MAX_FILENAME + 2];
                snprintf(response, sizeof(response), "%s\n", entry->d_name);
                send(client_fd, response, strlen(response), 0);
            }
        }
    }
    
    // Send end marker
    send(client_fd, ".\n", 2, 0);
    closedir(dir);
}

void handle_pull_command(int client_fd, const char *file_path) {
    const char *relative_path = file_path;
    if (file_path[0] == '/') {
        relative_path = file_path + 1;  // Skip the leading '/'
    }
    
    int fd = open(relative_path, O_RDONLY);  // Use relative_path here
    if (fd < 0) {
        // Send error response
        char error_response[64];
        snprintf(error_response, sizeof(error_response), "-1 %s", strerror(errno));
        send(client_fd, error_response, strlen(error_response), 0);
        return;
    }
    
    // Get file size
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        char error_response[64];
        snprintf(error_response, sizeof(error_response), "-1 %s", strerror(errno));
        send(client_fd, error_response, strlen(error_response), 0);
        close(fd);
        return;
    }
    
    // Send file size first
    char size_header[32];
    snprintf(size_header, sizeof(size_header), "%ld ", file_stat.st_size);
    send(client_fd, size_header, strlen(size_header), 0);
    
    // Send file content
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_sent = 0;
        while (bytes_sent < bytes_read) {
            ssize_t sent = send(client_fd, buffer + bytes_sent, bytes_read - bytes_sent, 0);
            if (sent < 0) {
                fprintf(stderr, "Error sending file data: %s\n", strerror(errno));
                close(fd);
                return;
            }
            bytes_sent += sent;
        }
    }
    
    close(fd);
}

void handle_push_command(int client_fd, const char *file_path, int chunk_size) {
    static int current_fd = -1;
    
    // Strip leading '/' to make path relative  
    const char *relative_path = file_path;
    if (file_path[0] == '/') {
        relative_path = file_path + 1;  // Skip the leading '/'
    }

    if (chunk_size == -1) {
        // Start new file - truncate if exists
        if (current_fd >= 0) {
            close(current_fd);
        }
        current_fd = open(relative_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (current_fd < 0) {
            fprintf(stderr, "Error opening file %s for writing: %s\n", file_path, strerror(errno));
            return;
        }
        return;
    }
    
    if (chunk_size == 0) {
        // End of file
        if (current_fd >= 0) {
            close(current_fd);
            current_fd = -1;
        }
        return;
    }
    
    if (current_fd < 0) {
        fprintf(stderr, "Error: No file open for writing\n");
        return;
    }
    
    // Read chunk data from socket and write to file
    char buffer[MAX_BUFFER_SIZE];
    int total_received = 0;
    
    while (total_received < chunk_size) {
        int to_receive = (chunk_size - total_received < MAX_BUFFER_SIZE) ? 
                        chunk_size - total_received : MAX_BUFFER_SIZE;
        
        ssize_t received = recv(client_fd, buffer, to_receive, 0);
        if (received <= 0) {
            fprintf(stderr, "Error receiving chunk data: %s\n", strerror(errno));
            return;
        }
        
        ssize_t written = write(current_fd, buffer, received);
        if (written != received) {
            fprintf(stderr, "Error writing to file: %s\n", strerror(errno));
            return;
        }
        
        total_received += received;
    }
}

void handle_client_connection(int client_fd) {
    char buffer[MAX_COMMAND_SIZE];
    
    while (1) {
        ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break; // Client disconnected or error
        }
        
        buffer[received] = '\0';
        
        // Remove trailing newline
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        printf("Received command: %s\n", buffer);
        
        if (strncmp(buffer, CMD_LIST, strlen(CMD_LIST)) == 0) {
            char *dir_path = buffer + strlen(CMD_LIST);
            while (*dir_path == ' ') dir_path++; // Skip spaces
            handle_list_command(client_fd, dir_path);
            
        } else if (strncmp(buffer, CMD_PULL, strlen(CMD_PULL)) == 0) {
            char *file_path = buffer + strlen(CMD_PULL);
            while (*file_path == ' ') file_path++; // Skip spaces
            handle_pull_command(client_fd, file_path);
            
        } else if (strncmp(buffer, CMD_PUSH, strlen(CMD_PUSH)) == 0) {
            char file_path[MAX_PATH];
            int chunk_size;
            
            if (sscanf(buffer, "PUSH %s %d", file_path, &chunk_size) >= 2) {
                handle_push_command(client_fd, file_path, chunk_size);
            } else {
                fprintf(stderr, "Invalid PUSH command format: %s\n", buffer);
            }
        } else {
            fprintf(stderr, "Unknown command: %s\n", buffer);
        }
    }
    
    close(client_fd);
}