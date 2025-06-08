#include "../include/nfs_manager_logic.h"

// Global manager instance for signal handling
nfs_manager_t *global_manager = NULL;

// Global log file for worker threads
FILE *g_worker_logfile = NULL;

// Signal handling flag
volatile sig_atomic_t shutdown_flag = 0;

void signal_handler(int sig) {
    shutdown_flag = 1;
    if (global_manager) {
        global_manager->shutdown_requested = 1;
    }
    
    // Don't use printf in signal handler - not async-safe
    // Just set the flag and let main loop handle it
    signal(sig, SIG_DFL);  // Reset to default handler
}

int parse_arguments(int argc, char *argv[], nfs_manager_t *manager) {
    memset(manager, 0, sizeof(nfs_manager_t));
    manager->worker_limit = DEFAULT_WORKERS;
    
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing argument for %s\n", argv[i]);
            return -1;
        }
        
        if (strcmp(argv[i], "-l") == 0) {
            manager->logfile_path = strdup(argv[i + 1]);
        } else if (strcmp(argv[i], "-c") == 0) {
            manager->config_file_path = strdup(argv[i + 1]);
        } else if (strcmp(argv[i], "-n") == 0) {
            manager->worker_limit = atoi(argv[i + 1]);
            if (manager->worker_limit <= 0) {
                fprintf(stderr, "Invalid worker limit: %s\n", argv[i + 1]);
                return -1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            manager->port = atoi(argv[i + 1]);
            if (manager->port <= 0) {
                fprintf(stderr, "Invalid port: %s\n", argv[i + 1]);
                return -1;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            manager->buffer_size = atoi(argv[i + 1]);
            if (manager->buffer_size <= 0) {
                fprintf(stderr, "Invalid buffer size: %s\n", argv[i + 1]);
                return -1;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }
    
    if (!manager->logfile_path || !manager->config_file_path || 
        manager->port == 0 || manager->buffer_size == 0) {
        fprintf(stderr, "Missing required arguments\n");
        return -1;
    }
    
    return 0;
}

int initialize_manager(nfs_manager_t *manager) {
    // Open log file
    manager->logfile = fopen(manager->logfile_path, "w");
    if (!manager->logfile) {
        fprintf(stderr, "Error opening log file %s: %s\n", manager->logfile_path, strerror(errno));
        return -1;
    }
    
    // Set global log file for worker threads
    g_worker_logfile = manager->logfile;
    
    // Create server socket
    manager->server_sockfd = create_server_socket(manager->port);
    if (manager->server_sockfd < 0) {
        return -1;
    }
    
    // Create sync info store
    manager->sync_store = create_sync_info_store();
    if (!manager->sync_store) {
        fprintf(stderr, "Failed to create sync info store\n");
        return -1;
    }
    
    // Create thread pool
    manager->thread_pool = create_thread_pool(manager->worker_limit, manager->buffer_size);
    if (!manager->thread_pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        destroy_sync_info_store(manager->sync_store);
        return -1;
    }
    
    log_message(manager->logfile, "nfs_manager initialized on port %d with %d workers", 
                manager->port, manager->worker_limit);
    
    return 0;
}

int start_directory_sync(nfs_manager_t *manager, sync_info_t *sync_info) {
    // Validate inputs
    if (!manager || !sync_info) {
        fprintf(stderr, "Invalid parameters to start_directory_sync\n");
        return -1;
    }
    
    // Check that all string fields are valid
    if (!sync_info->source_host[0] || !sync_info->source_dir[0] || 
        !sync_info->target_host[0] || !sync_info->target_dir[0]) {
        fprintf(stderr, "Invalid sync_info: empty host or directory fields\n");
        return -1;
    }
    
    // Connect to source to get file list
    int source_fd = connect_to_server(sync_info->source_host, sync_info->source_port);
    if (source_fd < 0) {
        // Safe logging with validated strings
        if (manager->logfile) {
            log_message(manager->logfile, "Failed to connect to source %s:%d", 
                       sync_info->source_host, sync_info->source_port);
        }
        return -1;
    }
    
    // Send LIST command
    char command[MAX_COMMAND_SIZE];
    snprintf(command, sizeof(command), "LIST %s\n", sync_info->source_dir);
    
    if (send_command(source_fd, command) != 0) {
        close(source_fd);
        return -1;
    }
    
    // Read file list
    char buffer[MAX_BUFFER_SIZE];
    ssize_t received = recv(source_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        close(source_fd);
        return -1;
    }
    
    buffer[received] = '\0';
    
    // Parse filenames (one per line, ending with ".")
    char *line = strtok(buffer, "\n");
    while (line && strcmp(line, ".") != 0) {
        // Create sync job for this file
        sync_job_t *job = create_sync_job(
            sync_info->source_host, sync_info->source_port, sync_info->source_dir,
            sync_info->target_host, sync_info->target_port, sync_info->target_dir,
            line
        );
        
        if (job) {
            if (enqueue_sync_job(manager->thread_pool, job) == 0) {
                if (manager->logfile) {
                    log_message(manager->logfile, "Added file: %s/%s@%s:%d -> %s/%s@%s:%d",
                               sync_info->source_dir, line, sync_info->source_host, sync_info->source_port,
                               sync_info->target_dir, line, sync_info->target_host, sync_info->target_port);
                }
            } else {
                if (manager->logfile) {
                    log_message(manager->logfile, "Failed to enqueue job for file: %s", line);
                }
                free_sync_job(job);
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    close(source_fd);
    return 0;
}

int load_config_file(nfs_manager_t *manager) {
    printf("DEBUG: load_config_file called\n");
    
    if (!manager || !manager->config_file_path) {
        fprintf(stderr, "DEBUG: Invalid manager or config file path\n");
        return -1;
    }
    
    printf("DEBUG: Opening config file: %s\n", manager->config_file_path);
    FILE *config_file = fopen(manager->config_file_path, "r");
    if (!config_file) {
        fprintf(stderr, "Error opening config file %s: %s\n", manager->config_file_path, strerror(errno));
        return -1;
    }
    printf("DEBUG: Config file opened successfully\n");
    
    char line[MAX_COMMAND_SIZE];
    int line_number = 0;
    
    while (fgets(line, sizeof(line), config_file)) {
        line_number++;
        printf("DEBUG: Read line %d: %s", line_number, line);
        
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#') {
            printf("DEBUG: Skipping empty/comment line\n");
            continue;
        }
        
        char source_spec[MAX_PATH * 2], target_spec[MAX_PATH * 2];
        int scan_result = sscanf(line, "%s %s", source_spec, target_spec);
        printf("DEBUG: Parsed line - source='%s' target='%s' scan_result=%d\n", 
               source_spec, target_spec, scan_result);
        
        if (scan_result != 2) {
            fprintf(stderr, "Error parsing config line %d: %s", line_number, line);
            continue;
        }
        
        printf("DEBUG: Calling handle_add_command for: %s -> %s\n", source_spec, target_spec);
        
        // Add this sync pair
        int result = handle_add_command(manager, source_spec, target_spec);
        printf("DEBUG: handle_add_command returned: %d\n", result);
        
        if (result == 0) {
            printf("Loaded and started sync: %s -> %s\n", source_spec, target_spec);
        } else if (result == 1) {
            printf("Sync pair already exists: %s\n", source_spec);
        } else {
            printf("Failed to load sync pair: %s -> %s\n", source_spec, target_spec);
        }
    }
    
    fclose(config_file);
    printf("DEBUG: Config file processing complete\n");
    
    // Show current sync configuration only if we have a store
    if (manager->sync_store) {
        printf("\nCurrent sync configuration:\n");
        print_sync_info_store(manager->sync_store);
    } else {
        printf("DEBUG: No sync store available\n");
    }
    
    return 0;
}

int handle_add_command(nfs_manager_t *manager, const char *source_spec, const char *target_spec) {
    printf("DEBUG: handle_add_command called with source='%s' target='%s'\n", 
           source_spec ? source_spec : "NULL", target_spec ? target_spec : "NULL");
    
    if (!manager || !source_spec || !target_spec) {
        fprintf(stderr, "Invalid parameters to handle_add_command\n");
        return -1;
    }
    
    char source_host[MAX_HOST_SIZE], target_host[MAX_HOST_SIZE];
    char source_dir[MAX_PATH], target_dir[MAX_PATH];
    int source_port, target_port;
    
    printf("DEBUG: Parsing source spec: %s\n", source_spec);
    if (parse_directory_spec(source_spec, source_host, &source_port, source_dir) != 0) {
        printf("DEBUG: Failed to parse source spec\n");
        return -1;
    }
    printf("DEBUG: Source parsed - host=%s port=%d dir=%s\n", source_host, source_port, source_dir);
    
    printf("DEBUG: Parsing target spec: %s\n", target_spec);
    if (parse_directory_spec(target_spec, target_host, &target_port, target_dir) != 0) {
        printf("DEBUG: Failed to parse target spec\n");
        return -1;
    }
    printf("DEBUG: Target parsed - host=%s port=%d dir=%s\n", target_host, target_port, target_dir);
    
    // Check if already exists
    printf("DEBUG: Checking if sync info already exists...\n");
    if (find_sync_info(manager->sync_store, source_host, source_port, source_dir)) {
        printf("DEBUG: Sync info already exists\n");
        if (manager->logfile) {
            log_message(manager->logfile, "Already in queue: %s@%s:%d", 
                       source_dir, source_host, source_port);
        }
        return 1;
    }
    printf("DEBUG: Sync info doesn't exist, creating new one...\n");
    
    // Create and add sync info
    sync_info_t *info = create_sync_info(source_host, source_port, source_dir,
                                       target_host, target_port, target_dir);
    if (!info) {
        fprintf(stderr, "Failed to create sync info\n");
        return -1;
    }
    printf("DEBUG: Created sync info successfully\n");
    
    printf("DEBUG: Adding sync info to store...\n");
    if (add_sync_info(manager->sync_store, info) != 0) {
        printf("DEBUG: Failed to add sync info to store\n");
        free_sync_info(info);
        return -1;
    }
    printf("DEBUG: Added sync info to store successfully\n");
    
    // Start synchronization
    printf("DEBUG: Starting directory sync...\n");
    if (start_directory_sync(manager, info) != 0) {
        printf("DEBUG: Failed to start directory sync\n");
        if (manager->logfile) {
            log_message(manager->logfile, "Failed to start sync for %s@%s:%d", 
                       source_dir, source_host, source_port);
        }
        return -1;
    }
    printf("DEBUG: Directory sync started successfully\n");
    
    if (manager->logfile) {
        log_message(manager->logfile, "Started sync: %s@%s:%d -> %s@%s:%d",
                   source_dir, source_host, source_port,
                   target_dir, target_host, target_port);
    }
    
    return 0;
}

int handle_cancel_command(nfs_manager_t *manager, const char *source_spec) {
    if (!manager || !source_spec) {
        return -1;
    }
    
    char source_host[MAX_HOST_SIZE], source_dir[MAX_PATH];
    int source_port;
    
    if (parse_directory_spec(source_spec, source_host, &source_port, source_dir) != 0) {
        return -1;
    }
    
    if (deactivate_sync_info(manager->sync_store, source_host, source_port, source_dir) == 0) {
        if (manager->logfile) {
            log_message(manager->logfile, "Synchronization stopped for %s@%s:%d",
                       source_dir, source_host, source_port);
        }
        return 0;
    } else {
        if (manager->logfile) {
            log_message(manager->logfile, "Directory not being synchronized: %s@%s:%d",
                       source_dir, source_host, source_port);
        }
        return 1;
    }
}

int handle_shutdown_command(nfs_manager_t *manager) {
    if (!manager) return -1;
    
    manager->shutdown_requested = 1;
    shutdown_flag = 1;
    
    if (manager->logfile) {
        log_message(manager->logfile, "Shutting down manager...");
        log_message(manager->logfile, "Waiting for all active workers to finish.");
    }
    
    if (manager->thread_pool) {
        signal_shutdown(manager->thread_pool);
    }
    
    if (manager->logfile) {
        log_message(manager->logfile, "Processing remaining queued tasks.");
        log_message(manager->logfile, "Manager shutdown complete.");
    }
    
    return 0;
}

void handle_console_connection(nfs_manager_t *manager, int client_fd) {
    if (!manager || client_fd < 0) {
        return;
    }
    
    char buffer[MAX_COMMAND_SIZE];
    
    while (!manager->shutdown_requested && !shutdown_flag) {
        // Use select for timeout to check shutdown flag periodically
        fd_set read_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        timeout.tv_sec = 1;  // 1 second timeout
        timeout.tv_usec = 0;
        
        int select_result = select(client_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            if (errno == EINTR) continue;  // Interrupted by signal
            break;
        }
        if (select_result == 0) continue;  // Timeout, check shutdown flag
        
        ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break;
        }
        
        buffer[received] = '\0';
        
        // Parse command
        char command[64], arg1[MAX_PATH], arg2[MAX_PATH];
        int args = sscanf(buffer, "%s %s %s", command, arg1, arg2);
        
        char response[MAX_BUFFER_SIZE];
        
        if (strcmp(command, CMD_ADD) == 0 && args == 3) {
            int result = handle_add_command(manager, arg1, arg2);
            if (result == 0) {
                snprintf(response, sizeof(response), "Added sync pair successfully\n");
            } else if (result == 1) {
                snprintf(response, sizeof(response), "Already in queue: %s\n", arg1);
            } else {
                snprintf(response, sizeof(response), "Error adding sync pair\n");
            }
        } else if (strcmp(command, CMD_CANCEL) == 0 && args == 2) {
            int result = handle_cancel_command(manager, arg1);
            if (result == 0) {
                snprintf(response, sizeof(response), "Synchronization stopped for %s\n", arg1);
            } else if (result == 1) {
                snprintf(response, sizeof(response), "Directory not being synchronized: %s\n", arg1);
            } else {
                snprintf(response, sizeof(response), "Error canceling synchronization\n");
            }
        } else if (strcmp(command, CMD_SHUTDOWN) == 0) {
            handle_shutdown_command(manager);
            snprintf(response, sizeof(response), "Shutting down manager...\n");
            send(client_fd, response, strlen(response), 0);
            break;
        } else {
            snprintf(response, sizeof(response), "Invalid command: %s\n", buffer);
        }
        
        send(client_fd, response, strlen(response), 0);
    }
    
    close(client_fd);
}

void cleanup_manager(nfs_manager_t *manager) {
    if (!manager) return;
    
    printf("Cleaning up manager...\n");
    
    if (manager->thread_pool) {
        printf("Shutting down thread pool...\n");
        destroy_thread_pool(manager->thread_pool);
        manager->thread_pool = NULL;
    }
    
    if (manager->sync_store) {
        destroy_sync_info_store(manager->sync_store);
        manager->sync_store = NULL;
    }
    
    if (manager->logfile) {
        fclose(manager->logfile);
        manager->logfile = NULL;
        g_worker_logfile = NULL;  // Clear global reference
    }
    
    if (manager->server_sockfd >= 0) {
        close(manager->server_sockfd);
        manager->server_sockfd = -1;
    }
    
    if (manager->logfile_path) {
        free(manager->logfile_path);
        manager->logfile_path = NULL;
    }
    
    if (manager->config_file_path) {
        free(manager->config_file_path);
        manager->config_file_path = NULL;
    }
    
    printf("Manager cleanup complete.\n");
}