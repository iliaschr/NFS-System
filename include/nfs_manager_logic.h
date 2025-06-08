/**
 * @file nfs_manager_logic.h
 * @brief Network File System Manager - Core logic and data structures
 *
 * This module implements the central manager component of the distributed NFS.
 * The manager coordinates file synchronization between multiple nfs_client instances
 * using a pool of worker threads. It handles configuration management, job scheduling,
 * and provides a command interface for the nfs_console.
 *
 * Key responsibilities:
 * - Managing worker thread pool for parallel synchronization
 * - Maintaining sync pair configuration and status
 * - Handling console commands (add, cancel, shutdown)
 * - Coordinating file transfers between clients
 * - Logging synchronization events and errors
 */

#ifndef NFS_MANAGER_LOGIC_H
#define NFS_MANAGER_LOGIC_H

#include "common.h"
#include "thread_pool.h"
#include "sync_info.h"

/**
 * @brief Main manager structure containing all system state
 *
 * This structure encapsulates all manager configuration, runtime state,
 * and resource handles. It serves as the central data structure passed
 * between all manager functions.
 */
typedef struct {
    char *logfile_path;               ///< Path to manager log file
    char *config_file_path;           ///< Path to configuration file
    int worker_limit;                 ///< Maximum number of worker threads
    int port;                         ///< TCP port for console connections
    int buffer_size;                  ///< Maximum job queue size
    FILE *logfile;                    ///< Open log file handle
    int server_sockfd;                ///< Server socket for console connections
    thread_pool_t *thread_pool;       ///< Worker thread pool instance
    sync_info_store_t *sync_store;    ///< Sync pair information store
    int shutdown_requested;           ///< Shutdown flag
} nfs_manager_t;

// Global Variables

/**
 * @brief Global manager instance for signal handling
 *
 * Used by signal handlers to safely request shutdown when receiving
 * SIGINT or SIGTERM signals.
 */
extern nfs_manager_t *global_manager;

/**
 * @brief Atomic shutdown flag for signal-safe communication
 *
 * Set by signal handlers to indicate shutdown request. Checked by
 * main loops to ensure graceful termination.
 */
extern volatile sig_atomic_t shutdown_flag;

// Initialization and Configuration

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument vector
 * @param manager Manager structure to populate
 * @return 0 on success, -1 on error
 */
int parse_arguments(int argc, char *argv[], nfs_manager_t *manager);

/**
 * @brief Initialize manager components and resources
 * @param manager Manager structure to initialize
 * @return 0 on success, -1 on error
 */
int initialize_manager(nfs_manager_t *manager);

/**
 * @brief Load and process configuration file
 * @param manager Initialized manager instance
 * @return 0 on success, -1 on error
 */
int load_config_file(nfs_manager_t *manager);

/**
 * @brief Clean up manager resources and shutdown components
 * @param manager Manager instance to clean up
 */
void cleanup_manager(nfs_manager_t *manager);

// Command Handlers

/**
 * @brief Handle 'add' command to create new sync pair
 * @param manager Manager instance
 * @param source_spec Source directory specification (/path@host:port)
 * @param target_spec Target directory specification (/path@host:port)
 * @return 0 on success, 1 if already exists, -1 on error
 */
int handle_add_command(nfs_manager_t *manager, const char *source_spec, const char *target_spec);

/**
 * @brief Handle 'cancel' command to stop synchronization
 * @param manager Manager instance
 * @param source_spec Source directory specification to cancel
 * @return 0 on success, 1 if not found, -1 on error
 */
int handle_cancel_command(nfs_manager_t *manager, const char *source_spec);

/**
 * @brief Handle 'shutdown' command to stop manager
 * @param manager Manager instance
 * @return 0 on success, -1 on error
 */
int handle_shutdown_command(nfs_manager_t *manager);

// Connection Handling

/**
 * @brief Handle console connection and process commands
 * @param manager Manager instance
 * @param client_fd Connected console socket
 *
 * This function runs in a loop processing commands from the connected
 * console until shutdown is requested or the connection is closed.
 */
void handle_console_connection(nfs_manager_t *manager, int client_fd);

// Synchronization Operations

/**
 * @brief Start directory synchronization for a sync pair
 * @param manager Manager instance
 * @param sync_info Sync pair information
 * @return 0 on success, -1 on error
 *
 * Connects to source client, retrieves file list, and creates
 * synchronization jobs for each file in the worker queue.
 */
int start_directory_sync(nfs_manager_t *manager, sync_info_t *sync_info);

// Signal Handling

/**
 * @brief Signal handler for graceful shutdown
 * @param sig Signal number (SIGINT, SIGTERM)
 *
 * Sets shutdown flags in a signal-safe manner to trigger
 * graceful termination of the manager process.
 */
void signal_handler(int sig);

#endif // NFS_MANAGER_LOGIC_H