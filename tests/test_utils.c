#include "acutest.h"
#include "../include/common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

// Test timestamp function
void test_get_timestamp(void) {
    char buffer[32];
    get_timestamp(buffer, sizeof(buffer));
    
    // Check format: YYYY-MM-DD HH:MM:SS
    TEST_CHECK(strlen(buffer) == 19);
    TEST_CHECK(buffer[4] == '-');
    TEST_CHECK(buffer[7] == '-');
    TEST_CHECK(buffer[10] == ' ');
    TEST_CHECK(buffer[13] == ':');
    TEST_CHECK(buffer[16] == ':');
    
    // Test that buffer is null-terminated
    TEST_CHECK(buffer[19] == '\0');
}

// Test directory spec parsing
void test_parse_directory_spec(void) {
    char host[MAX_HOST_SIZE];
    int port;
    char dir[MAX_PATH];
    
    // Valid case
    int result = parse_directory_spec("/home/test@192.168.1.1:8080", host, &port, dir);
    TEST_CHECK(result == 0);
    TEST_CHECK(strcmp(host, "192.168.1.1") == 0);
    TEST_CHECK(port == 8080);
    TEST_CHECK(strcmp(dir, "/home/test") == 0);
    
    // Test localhost
    result = parse_directory_spec("/data@127.0.0.1:9000", host, &port, dir);
    TEST_CHECK(result == 0);
    TEST_CHECK(strcmp(host, "127.0.0.1") == 0);
    TEST_CHECK(port == 9000);
    TEST_CHECK(strcmp(dir, "/data") == 0);
    
    // Invalid cases
    result = parse_directory_spec("/home/test@192.168.1.1", host, &port, dir);
    TEST_CHECK(result == -1);  // Missing port
    
    result = parse_directory_spec("/home/test:8080", host, &port, dir);
    TEST_CHECK(result == -1);  // Missing @
    
    result = parse_directory_spec("/home/test@192.168.1.1:abc", host, &port, dir);
    TEST_CHECK(result == -1);  // Invalid port
}

// Test config line parsing
void test_parse_config_line(void) {
    char source_host[MAX_HOST_SIZE], target_host[MAX_HOST_SIZE];
    char source_dir[MAX_PATH], target_dir[MAX_PATH];
    int source_port, target_port;
    
    // Valid config line
    const char *line = "/source@192.168.1.1:8080 /target@192.168.1.2:9090";
    int result = parse_config_line(line, source_host, &source_port, source_dir,
                                   target_host, &target_port, target_dir);
    
    TEST_CHECK(result == 0);
    TEST_CHECK(strcmp(source_host, "192.168.1.1") == 0);
    TEST_CHECK(source_port == 8080);
    TEST_CHECK(strcmp(source_dir, "/source") == 0);
    TEST_CHECK(strcmp(target_host, "192.168.1.2") == 0);
    TEST_CHECK(target_port == 9090);
    TEST_CHECK(strcmp(target_dir, "/target") == 0);
    
    // Invalid config line (missing target)
    result = parse_config_line("/source@192.168.1.1:8080", source_host, &source_port, source_dir,
                               target_host, &target_port, target_dir);
    TEST_CHECK(result == -1);
}

// Test socket creation (basic functionality)
void test_socket_creation(void) {
    // Test create_server_socket with a high port number
    int sockfd = create_server_socket(0);  // Let OS choose port
    TEST_CHECK(sockfd >= 0);
    
    if (sockfd >= 0) {
        // Get actual port assigned
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int result = getsockname(sockfd, (struct sockaddr*)&addr, &len);
        TEST_CHECK(result == 0);
        TEST_CHECK(ntohs(addr.sin_port) > 0);
        
        cleanup_socket(sockfd);
    }
}

// Test command validation
void test_command_validation(void) {
    // Test valid commands
    TEST_CHECK(strcmp(CMD_ADD, "add") == 0);
    TEST_CHECK(strcmp(CMD_CANCEL, "cancel") == 0);
    TEST_CHECK(strcmp(CMD_SHUTDOWN, "shutdown") == 0);
    TEST_CHECK(strcmp(CMD_LIST, "LIST") == 0);
    TEST_CHECK(strcmp(CMD_PULL, "PULL") == 0);
    TEST_CHECK(strcmp(CMD_PUSH, "PUSH") == 0);
}

// Test memory allocation patterns
void test_memory_allocation(void) {
    // Test sync_job allocation
    sync_job_t *job = malloc(sizeof(sync_job_t));
    TEST_CHECK(job != NULL);
    
    if (job) {
        memset(job, 0, sizeof(sync_job_t));
        strcpy(job->source_host, "127.0.0.1");
        job->source_port = 8080;
        strcpy(job->filename, "test.txt");
        
        TEST_CHECK(strcmp(job->source_host, "127.0.0.1") == 0);
        TEST_CHECK(job->source_port == 8080);
        TEST_CHECK(strcmp(job->filename, "test.txt") == 0);
        
        free(job);
    }
    
    // Test sync_info allocation
    sync_info_t *info = malloc(sizeof(sync_info_t));
    TEST_CHECK(info != NULL);
    
    if (info) {
        memset(info, 0, sizeof(sync_info_t));
        strcpy(info->source_dir, "/test");
        info->active = 1;
        info->error_count = 0;
        
        TEST_CHECK(strcmp(info->source_dir, "/test") == 0);
        TEST_CHECK(info->active == 1);
        TEST_CHECK(info->error_count == 0);
        
        free(info);
    }
}

// Test file operations safety
void test_file_operations_safety(void) {
    // Test opening non-existent file
    FILE *file = fopen("non_existent_file_12345.txt", "r");
    TEST_CHECK(file == NULL);
    
    if (file) {
        fclose(file);
    }
    
    // Test creating and writing to a temporary file
    FILE *temp_file = fopen("test_temp.txt", "w");
    TEST_CHECK(temp_file != NULL);
    
    if (temp_file) {
        fprintf(temp_file, "Test content");
        fclose(temp_file);
        
        // Read it back
        temp_file = fopen("test_temp.txt", "r");
        TEST_CHECK(temp_file != NULL);
        
        if (temp_file) {
            char buffer[32];
            char *result = fgets(buffer, sizeof(buffer), temp_file);
            TEST_CHECK(result != NULL);
            TEST_CHECK(strcmp(buffer, "Test content") == 0);
            fclose(temp_file);
        }
        
        // Clean up
        unlink("test_temp.txt");
    }
}

// Test buffer boundaries
void test_buffer_boundaries(void) {
    char buffer[MAX_BUFFER_SIZE];
    
    // Test maximum buffer size
    TEST_CHECK(sizeof(buffer) == MAX_BUFFER_SIZE);
    
    // Test that we can fill the buffer safely
    memset(buffer, 'A', MAX_BUFFER_SIZE - 1);
    buffer[MAX_BUFFER_SIZE - 1] = '\0';
    
    TEST_CHECK(strlen(buffer) == MAX_BUFFER_SIZE - 1);
    TEST_CHECK(buffer[0] == 'A');
    TEST_CHECK(buffer[MAX_BUFFER_SIZE - 1] == '\0');
}

// Test list to run
TEST_LIST = {
    { "get_timestamp", test_get_timestamp },
    { "parse_directory_spec", test_parse_directory_spec },
    { "parse_config_line", test_parse_config_line },
    { "socket_creation", test_socket_creation },
    { "command_validation", test_command_validation },
    { "memory_allocation", test_memory_allocation },
    { "file_operations_safety", test_file_operations_safety },
    { "buffer_boundaries", test_buffer_boundaries },
    { NULL, NULL }
};