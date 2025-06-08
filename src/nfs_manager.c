#include "../include/nfs_manager_logic.h"

extern volatile sig_atomic_t shutdown_flag;

int main(int argc, char *argv[]) {
    nfs_manager_t manager;
    
    printf("DEBUG: Manager starting...\n");
    
    if (argc < 9) {
        fprintf(stderr, "Usage: %s -l <manager_logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>\n", argv[0]);
        return 1;
    }
    
    printf("DEBUG: Parsing arguments...\n");
    if (parse_arguments(argc, argv, &manager) != 0) {
        printf("DEBUG: Argument parsing failed\n");
        return 1;
    }
    printf("DEBUG: Arguments parsed successfully\n");
    
    global_manager = &manager;
    
    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    printf("DEBUG: Signal handlers set up\n");
    
    printf("DEBUG: Initializing manager...\n");
    if (initialize_manager(&manager) != 0) {
        printf("DEBUG: Manager initialization failed\n");
        cleanup_manager(&manager);
        return 1;
    }
    printf("DEBUG: Manager initialized successfully\n");
    
    printf("DEBUG: Loading config file: %s\n", manager.config_file_path);
    if (load_config_file(&manager) != 0) {
        printf("DEBUG: Config file loading failed\n");
        cleanup_manager(&manager);
        return 1;
    }
    printf("DEBUG: Config file loaded successfully\n");
    
    printf("nfs_manager started on port %d\n", manager.port);
    printf("DEBUG: Entering main server loop...\n");
    
    // Main server loop
    while (!manager.shutdown_requested && !shutdown_flag) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        printf("DEBUG: Waiting for connection on port %d...\n", manager.port);
        
        // Use select to timeout and check shutdown flag
        fd_set read_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(manager.server_sockfd, &read_fds);
        timeout.tv_sec = 5;  // 5 second timeout for easier debugging
        timeout.tv_usec = 0;
        
        int select_result = select(manager.server_sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, check shutdown flag
                printf("DEBUG: Select interrupted by signal\n");
                continue;
            }
            if (!manager.shutdown_requested && !shutdown_flag) {
                fprintf(stderr, "Error in select: %s\n", strerror(errno));
            }
            break;
        }
        
        if (select_result == 0) {
            // Timeout, just continue to check shutdown flag
            printf("DEBUG: Select timeout, checking for shutdown...\n");
            continue;
        }
        
        printf("DEBUG: Connection available, accepting...\n");
        int client_fd = accept(manager.server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!manager.shutdown_requested && !shutdown_flag) {
                fprintf(stderr, "Error accepting connection: %s\n", strerror(errno));
            }
            continue;
        }
        
        printf("Console connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("DEBUG: Starting console connection handler...\n");
        
        handle_console_connection(&manager, client_fd);
        printf("DEBUG: Console connection handler finished\n");
    }
    
    printf("Manager shutting down...\n");
    cleanup_manager(&manager);
    printf("Manager shutdown complete.\n");
    return 0;
}