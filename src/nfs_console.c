#include "../include/nfs_console.h"

void console_log_command(FILE *logfile, const char *command) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    if (logfile) {
        fprintf(logfile, "[%s] Command %s\n", timestamp, command);
        fflush(logfile);
    }
}

int process_user_command(const char *input, char *command, char *args) {
    // Remove trailing newline
    char *input_copy = strdup(input);
    char *newline = strchr(input_copy, '\n');
    if (newline) *newline = '\0';
    
    // Parse command and arguments
    char *space = strchr(input_copy, ' ');
    if (space) {
        *space = '\0';
        strcpy(command, input_copy);
        strcpy(args, space + 1);
    } else {
        strcpy(command, input_copy);
        args[0] = '\0';
    }
    
    free(input_copy);
    
    // Validate command
    if (strcmp(command, CMD_ADD) == 0) {
        // args should contain "source target"
        char source[MAX_PATH], target[MAX_PATH];
        if (sscanf(args, "%s %s", source, target) != 2) {
            fprintf(stderr, "Invalid add command format. Use: add <source> <target>\n");
            return -1;
        }
    } else if (strcmp(command, CMD_CANCEL) == 0) {
        // args should contain source directory
        if (strlen(args) == 0) {
            fprintf(stderr, "Invalid cancel command format. Use: cancel <source>\n");
            return -1;
        }
    } else if (strcmp(command, CMD_SHUTDOWN) == 0) {
        // No arguments needed
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        fprintf(stderr, "Available commands: add, cancel, shutdown\n");
        return -1;
    }
    
    return 0;
}

int send_command_to_manager(int sockfd, const char *command, FILE *logfile) {
    // Send command
    if (send_command(sockfd, command) != 0) {
        return -1;
    }
    
    // Receive response
    char response[MAX_BUFFER_SIZE];
    int received = receive_response(sockfd, response, sizeof(response));
    if (received < 0) {
        return -1;
    }
    
    // Display response to user
    printf("%s", response);
    
    // Log response to console logfile
    if (logfile) {
        char timestamp[32];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(logfile, "[%s] Response: %s", timestamp, response);
        fflush(logfile);
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    char *logfile_path = NULL;
    char *host = NULL;
    int port = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing argument for %s\n", argv[i]);
            goto usage;
        }
        
        if (strcmp(argv[i], "-l") == 0) {
            logfile_path = argv[i + 1];
        } else if (strcmp(argv[i], "-h") == 0) {
            host = argv[i + 1];
        } else if (strcmp(argv[i], "-p") == 0) {
            port = atoi(argv[i + 1]);
            if (port <= 0) {
                fprintf(stderr, "Invalid port number: %s\n", argv[i + 1]);
                goto usage;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            goto usage;
        }
    }
    
    if (!logfile_path || !host || port == 0) {
        goto usage;
    }
    
    // Open log file
    FILE *logfile = fopen(logfile_path, "w");
    if (!logfile) {
        fprintf(stderr, "Error opening log file %s: %s\n", logfile_path, strerror(errno));
        return 1;
    }
    
    printf("nfs_console started. Connecting to %s:%d\n", host, port);
    printf("Type 'help' for available commands or 'shutdown' to exit.\n");
    
    // Connect to manager
    int sockfd = connect_to_server(host, port);
    if (sockfd < 0) {
        fclose(logfile);
        return 1;
    }
    
    printf("Connected to nfs_manager\n");
    printf("> ");
    fflush(stdout);
    
    char input[MAX_COMMAND_SIZE];
    while (fgets(input, sizeof(input), stdin)) {
        char command[64], args[MAX_PATH * 2];
        
        // Skip empty lines
        if (input[0] == '\n') {
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        // Handle help command locally
        if (strncmp(input, "help", 4) == 0) {
            printf("Available commands:\n");
            printf("  add <source> <target>  - Add directory pair for synchronization\n");
            printf("  cancel <source>        - Cancel synchronization for source directory\n");
            printf("  shutdown               - Shutdown the manager\n");
            printf("  help                   - Show this help message\n");
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        // Process command
        if (process_user_command(input, command, args) != 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        // Log command
        char full_command[MAX_COMMAND_SIZE];
        if (strlen(args) > 0) {
            snprintf(full_command, sizeof(full_command), "%s %s", command, args);
        } else {
            strcpy(full_command, command);
        }
        console_log_command(logfile, full_command);
        
        // Send to manager
        if (send_command_to_manager(sockfd, full_command, logfile) != 0) {
            fprintf(stderr, "Error communicating with manager\n");
            break;
        }
        
        // Exit if shutdown command
        if (strcmp(command, CMD_SHUTDOWN) == 0) {
            printf("Shutting down console...\n");
            break;
        }
        
        printf("> ");
        fflush(stdout);
    }
    
    cleanup_socket(sockfd);
    fclose(logfile);
    return 0;

usage:
    fprintf(stderr, "Usage: %s -l <console-logfile> -h <host_IP> -p <host_port>\n", argv[0]);
    return 1;
}