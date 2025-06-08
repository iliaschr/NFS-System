/**
 * @file common.h
 * @brief Common definitions, structures, and utility functions for the NFS system
 *
 * This header file contains all shared constants, data structures, and function
 * declarations used across the distributed network file system components.
 * It defines the core data types for synchronization jobs and thread pools,
 * as well as common networking and utility functions.
 */

#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE  ///< Enable GNU extensions for additional functionality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdarg.h>

// System Constants
#define MAX_PATH 1024              ///< Maximum path length for directories and files
#define MAX_BUFFER_SIZE 8192       ///< Maximum buffer size for I/O operations
#define MAX_COMMAND_SIZE 4096      ///< Maximum size for command strings
#define MAX_CONNECTIONS 10         ///< Maximum concurrent connections
#define DEFAULT_WORKERS 5          ///< Default number of worker threads
#define MAX_FILENAME 256           ///< Maximum filename length
#define MAX_HOST_SIZE 256          ///< Maximum hostname/IP address length

// Protocol Commands
#define CMD_ADD "add"              ///< Console command to add sync pair
#define CMD_CANCEL "cancel"        ///< Console command to cancel sync
#define CMD_SHUTDOWN "shutdown"    ///< Console command to shutdown manager
#define CMD_LIST "LIST"            ///< Client command to list directory files
#define CMD_PULL "PULL"            ///< Client command to retrieve file
#define CMD_PUSH "PUSH"            ///< Client command to store file

// Log timestamp format (ISO 8601 style)
#define TIMESTAMP_FORMAT "%Y-%m-%d %H:%M:%S"

// Define DT_REG if not available (compatibility)
#ifndef DT_REG
#define DT_REG 8                   ///< Regular file type identifier
#endif

/**
 * @brief Structure representing a file synchronization job
 *
 * This structure contains all information needed for a worker thread
 * to synchronize a single file between source and target locations.
 * Jobs are queued and processed by the thread pool.
 */
typedef struct sync_job {
    char source_host[MAX_HOST_SIZE];  ///< Source server hostname/IP
    int source_port;                  ///< Source server port number
    char source_dir[MAX_PATH];        ///< Source directory path
    char target_host[MAX_HOST_SIZE];  ///< Target server hostname/IP
    int target_port;                  ///< Target server port number
    char target_dir[MAX_PATH];        ///< Target directory path
    char filename[MAX_FILENAME];     ///< Name of file to synchronize
    struct sync_job *next;           ///< Pointer to next job in queue
} sync_job_t;

/**
 * @brief Structure for tracking directory synchronization pairs
 *
 * Maintains metadata about each source-target directory pair being
 * monitored for synchronization, including status and error tracking.
 */
typedef struct sync_info {
    char source_host[MAX_HOST_SIZE];  ///< Source server hostname/IP
    int source_port;                  ///< Source server port number
    char source_dir[MAX_PATH];        ///< Source directory path
    char target_host[MAX_HOST_SIZE];  ///< Target server hostname/IP
    int target_port;                  ///< Target server port number
    char target_dir[MAX_PATH];        ///< Target directory path
    int active;                       ///< Whether sync is currently active
    time_t last_sync_time;           ///< Timestamp of last synchronization
    int error_count;                 ///< Number of errors encountered
    struct sync_info *next;          ///< Pointer to next sync info in list
} sync_info_t;

/**
 * @brief Thread pool structure for managing worker threads
 *
 * Implements a producer-consumer pattern with a bounded buffer for
 * synchronization jobs. Provides thread-safe job queuing and processing
 * with proper synchronization primitives.
 */
typedef struct {
    pthread_t *threads;               ///< Array of worker thread handles
    int thread_count;                 ///< Number of worker threads
    sync_job_t *job_queue_head;      ///< Head of job queue linked list
    sync_job_t *job_queue_tail;      ///< Tail of job queue linked list
    int queue_size;                  ///< Current number of jobs in queue
    int buffer_size;                 ///< Maximum queue capacity
    pthread_mutex_t queue_mutex;     ///< Mutex for queue access
    pthread_cond_t queue_not_empty;  ///< Condition variable for consumers
    pthread_cond_t queue_not_full;   ///< Condition variable for producers
    int shutdown;                    ///< Shutdown flag for worker threads
} thread_pool_t;

// Utility Functions

/**
 * @brief Generate timestamp string in standard format
 * @param buffer Output buffer for timestamp string
 * @param size Size of output buffer
 */
void get_timestamp(char *buffer, size_t size);

/**
 * @brief Log formatted message with timestamp
 * @param logfile File handle for log output (NULL for stdout only)
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void log_message(FILE *logfile, const char *format, ...);

// Network Functions

/**
 * @brief Create and bind server socket
 * @param port Port number to bind to
 * @return Socket file descriptor on success, -1 on error
 */
int create_server_socket(int port);

/**
 * @brief Connect to remote server
 * @param host Hostname or IP address
 * @param port Port number
 * @return Socket file descriptor on success, -1 on error
 */
int connect_to_server(const char *host, int port);

/**
 * @brief Clean up socket resources
 * @param sockfd Socket file descriptor to close
 */
void cleanup_socket(int sockfd);

/**
 * @brief Send command string over socket
 * @param sockfd Socket file descriptor
 * @param command Command string to send
 * @return 0 on success, -1 on error
 */
int send_command(int sockfd, const char *command);

/**
 * @brief Receive response from socket
 * @param sockfd Socket file descriptor
 * @param buffer Buffer to store response
 * @param buffer_size Size of response buffer
 * @return Number of bytes received on success, -1 on error
 */
int receive_response(int sockfd, char *buffer, size_t buffer_size);

// Parsing Functions

/**
 * @brief Parse directory specification string
 * @param spec Directory specification in format "/path@host:port"
 * @param host Output buffer for hostname
 * @param port Output pointer for port number
 * @param dir Output buffer for directory path
 * @return 0 on success, -1 on error
 */
int parse_directory_spec(const char *spec, char *host, int *port, char *dir);

/**
 * @brief Parse configuration file line
 * @param line Configuration line with source and target specs
 * @param source_host Output buffer for source hostname
 * @param source_port Output pointer for source port
 * @param source_dir Output buffer for source directory
 * @param target_host Output buffer for target hostname
 * @param target_port Output pointer for target port
 * @param target_dir Output buffer for target directory
 * @return 0 on success, -1 on error
 */
int parse_config_line(const char *line, char *source_host, int *source_port, 
                     char *source_dir, char *target_host, int *target_port, char *target_dir);

#endif // COMMON_H