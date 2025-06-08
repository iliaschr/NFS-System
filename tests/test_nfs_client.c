#include "acutest.h"
#include "../include/nfs_client_logic.h"
#include "../include/common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>

// Test directory setup
static void setup_test_directory(void) {
    system("mkdir -p test_client_data");
    system("echo 'Test file 1 content' > test_client_data/file1.txt");
    system("echo 'Test file 2 content' > test_client_data/file2.txt");
    system("echo 'Sample data for testing' > test_client_data/sample.txt");
}

static void cleanup_test_directory(void) {
    system("rm -rf test_client_data");
    system("rm -rf test_client_output");
}

// Test LIST command functionality
void test_list_command_functionality(void) {
    setup_test_directory();
    
    int sockpair[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
    
    // Fork to test client handling
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - simulate client
        close(sockpair[0]);
        handle_list_command(sockpair[1], "test_client_data");
        close(sockpair[1]);
        exit(0);
    } else if (pid > 0) {
        // Parent process - read response
        close(sockpair[1]);
        
        char buffer[1024];
        ssize_t total_read = 0;
        ssize_t bytes_read;
        
        while ((bytes_read = read(sockpair[0], buffer + total_read, 
                                sizeof(buffer) - total_read - 1)) > 0) {
            total_read += bytes_read;
            if (total_read > 0 && buffer[total_read - 1] == '.' && 
                (total_read == 1 || buffer[total_read - 2] == '\n')) {
                break; // Found end marker
            }
        }
        
        buffer[total_read] = '\0';
        close(sockpair[0]);
        
        // Verify response contains files
        TEST_CHECK(strstr(buffer, "file1.txt") != NULL);
        TEST_CHECK(strstr(buffer, "file2.txt") != NULL);
        TEST_CHECK(strstr(buffer, "sample.txt") != NULL);
        TEST_CHECK(strstr(buffer, ".\n") != NULL);  // End marker
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        TEST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    
    cleanup_test_directory();
}

// Test PULL command functionality
void test_pull_command_functionality(void) {
    setup_test_directory();
    
    int sockpair[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - simulate client
        close(sockpair[0]);
        handle_pull_command(sockpair[1], "test_client_data/file1.txt");
        close(sockpair[1]);
        exit(0);
    } else if (pid > 0) {
        // Parent process - read response
        close(sockpair[1]);
        
        // First read the size header
        char size_buffer[32];
        ssize_t header_read = 0;
        ssize_t bytes_read;
        
        // Read until we find a space (end of size header)
        while (header_read < sizeof(size_buffer) - 1) {
            bytes_read = read(sockpair[0], size_buffer + header_read, 1);
            if (bytes_read <= 0) break;
            header_read++;
            if (size_buffer[header_read - 1] == ' ') break;
        }
        
        size_buffer[header_read] = '\0';
        
        if (header_read > 0) {
            long file_size = atol(size_buffer);
            TEST_CHECK(file_size > 0);
            
            // Now read the file content
            char content_buffer[1024];
            ssize_t content_read = read(sockpair[0], content_buffer, sizeof(content_buffer) - 1);
            TEST_CHECK(content_read > 0);
            
            if (content_read > 0) {
                content_buffer[content_read] = '\0';
                TEST_CHECK(content_read >= file_size);
                TEST_CHECK(strncmp(content_buffer, "Test file 1 content", 19) == 0);
            }
        }
        
        close(sockpair[0]);
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        TEST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    
    cleanup_test_directory();
}

// Test PULL command with non-existent file
void test_pull_command_error(void) {
    int sockpair[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - simulate client
        close(sockpair[0]);
        handle_pull_command(sockpair[1], "non_existent_file.txt");
        close(sockpair[1]);
        exit(0);
    } else if (pid > 0) {
        // Parent process - read response
        close(sockpair[1]);
        
        char buffer[1024];
        ssize_t bytes_read = read(sockpair[0], buffer, sizeof(buffer) - 1);
        TEST_CHECK(bytes_read > 0);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            
            // Should start with "-1" indicating error
            TEST_CHECK(strncmp(buffer, "-1", 2) == 0);
        }
        
        close(sockpair[0]);
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        TEST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}

// Test PUSH command functionality - FIXED VERSION
void test_push_command_functionality(void) {
    system("mkdir -p test_client_output");
    
    int sockpair[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
    
    const char *test_data = "This is test data for PUSH command";
    size_t data_length = strlen(test_data);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - simulate receiving data
        close(sockpair[0]);
        
        // Start new file (chunk_size = -1)
        handle_push_command(sockpair[1], "test_client_output/pushed_file.txt", -1);
        
        // Read and write the data chunk
        handle_push_command(sockpair[1], "test_client_output/pushed_file.txt", data_length);
        
        // End file (chunk_size = 0)
        handle_push_command(sockpair[1], "test_client_output/pushed_file.txt", 0);
        
        close(sockpair[1]);
        exit(0);
    } else if (pid > 0) {
        // Parent process - send data
        close(sockpair[1]);
        
        // Send the test data that the child will read
        ssize_t sent = write(sockpair[0], test_data, data_length);
        TEST_CHECK(sent == (ssize_t)data_length);
        
        close(sockpair[0]);
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        TEST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        
        // Verify file was created and has correct content
        FILE *file = fopen("test_client_output/pushed_file.txt", "r");
        TEST_CHECK(file != NULL);
        
        if (file) {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));  // Initialize buffer
            size_t read_bytes = fread(buffer, 1, sizeof(buffer) - 1, file);
            TEST_CHECK(read_bytes > 0);
            
            if (read_bytes > 0) {
                buffer[read_bytes] = '\0';  // Ensure null termination
                TEST_CHECK(strcmp(buffer, test_data) == 0);
            }
            fclose(file);
        }
    }
    
    system("rm -rf test_client_output");
}

// Test client connection handling
void test_client_connection_handling(void) {
    setup_test_directory();
    
    int sockpair[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - simulate client handler
        close(sockpair[0]);
        
        // This would normally run in a loop, but we'll test one command
        char buffer[MAX_COMMAND_SIZE];
        ssize_t received = recv(sockpair[1], buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            
            // Remove trailing newline
            char *newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            
            if (strncmp(buffer, CMD_LIST, strlen(CMD_LIST)) == 0) {
                char *dir_path = buffer + strlen(CMD_LIST);
                while (*dir_path == ' ') dir_path++;
                handle_list_command(sockpair[1], dir_path);
            }
        }
        
        close(sockpair[1]);
        exit(0);
    } else if (pid > 0) {
        // Parent process - send command
        close(sockpair[1]);
        
        const char *command = "LIST test_client_data\n";
        write(sockpair[0], command, strlen(command));
        
        // Read response
        char buffer[1024];
        ssize_t total_read = 0;
        ssize_t bytes_read;
        
        while ((bytes_read = read(sockpair[0], buffer + total_read, 
                                sizeof(buffer) - total_read - 1)) > 0) {
            total_read += bytes_read;
            if (total_read > 0 && buffer[total_read - 1] == '.' && 
                (total_read == 1 || buffer[total_read - 2] == '\n')) {
                break;
            }
        }
        
        buffer[total_read] = '\0';
        close(sockpair[0]);
        
        // Verify response
        TEST_CHECK(strstr(buffer, "file1.txt") != NULL);
        TEST_CHECK(strstr(buffer, ".\n") != NULL);
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        TEST_CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    
    cleanup_test_directory();
}

// Test edge cases and error conditions
void test_edge_cases(void) {
    // Test LIST with non-existent directory
    int sockpair[2];
    TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
    
    pid_t pid = fork();
    if (pid == 0) {
        close(sockpair[0]);
        handle_list_command(sockpair[1], "non_existent_directory");
        close(sockpair[1]);
        exit(0);
    } else if (pid > 0) {
        close(sockpair[1]);
        
        char buffer[1024];
        ssize_t bytes_read = read(sockpair[0], buffer, sizeof(buffer) - 1);
        
        // Should at least get the end marker even if directory doesn't exist
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            TEST_CHECK(strstr(buffer, ".\n") != NULL);
        }
        
        close(sockpair[0]);
        
        int status;
        waitpid(pid, &status, 0);
    }
}

// Test buffer handling and large files
void test_buffer_handling(void) {
    // Create a larger test file
    FILE *large_file = fopen("large_test_file.txt", "w");
    TEST_CHECK(large_file != NULL);
    
    if (large_file) {
        // Write some data larger than typical buffer
        for (int i = 0; i < 1000; i++) {
            fprintf(large_file, "Line %d: This is a test line with some content.\n", i);
        }
        fclose(large_file);
        
        // Test PULL with larger file
        int sockpair[2];
        TEST_CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair) == 0);
        
        pid_t pid = fork();
        if (pid == 0) {
            close(sockpair[0]);
            handle_pull_command(sockpair[1], "large_test_file.txt");
            close(sockpair[1]);
            exit(0);
        } else if (pid > 0) {
            close(sockpair[1]);
            
            // Read response header (size)
            char header[64];
            ssize_t header_read = 0;
            ssize_t bytes_read;
            
            // Read until we find a space
            while (header_read < sizeof(header) - 1) {
                bytes_read = read(sockpair[0], header + header_read, 1);
                if (bytes_read <= 0) break;
                header_read++;
                if (header[header_read - 1] == ' ') break;
            }
            
            header[header_read] = '\0';
            
            if (header_read > 0) {
                long file_size = atol(header);
                TEST_CHECK(file_size > 1000);  // Should be substantial
            }
            
            close(sockpair[0]);
            
            int status;
            waitpid(pid, &status, 0);
        }
        
        unlink("large_test_file.txt");
    }
}

// Test list to run
TEST_LIST = {
    { "list_command_functionality", test_list_command_functionality },
    { "pull_command_functionality", test_pull_command_functionality },
    { "pull_command_error", test_pull_command_error },
    { "push_command_functionality", test_push_command_functionality },
    { "client_connection_handling", test_client_connection_handling },
    { "edge_cases", test_edge_cases },
    { "buffer_handling", test_buffer_handling },
    { NULL, NULL }
};