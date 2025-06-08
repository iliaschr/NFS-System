/**
 * @file nfs_client_logic.h
 * @brief Network File System Client - Core file operations
 *
 * This module implements the client-side file server that handles file
 * operations for the distributed NFS. Each client serves files from its
 * local directory and responds to commands from the manager's worker threads.
 *
 * Supported operations:
 * - LIST: Return directory file listing
 * - PULL: Send file content to requesting client
 * - PUSH: Receive and store file content from manager
 *
 * The client uses low-level I/O syscalls for all file operations as required
 * by the specification, avoiding high-level library functions.
 */

#ifndef NFS_CLIENT_LOGIC_H
#define NFS_CLIENT_LOGIC_H

#include "common.h"

// Command Handlers

/**
 * @brief Handle LIST command to enumerate directory files
 * @param client_fd Socket connected to requesting client
 * @param dir_path Local directory path to list
 *
 * Scans the specified directory and sends a list of regular files
 * to the client, one filename per line, terminated with a "." marker.
 * Uses stat() for portable file type detection.
 */
void handle_list_command(int client_fd, const char *dir_path);

/**
 * @brief Handle PULL command to send file content
 * @param client_fd Socket connected to requesting client
 * @param file_path Local file path to send
 *
 * Opens the specified file, sends the file size followed by a space,
 * then streams the complete file content. Sends error code (-1) and
 * error message if file cannot be accessed.
 */
void handle_pull_command(int client_fd, const char *file_path);

/**
 * @brief Handle PUSH command to receive file content
 * @param client_fd Socket connected to sending client
 * @param file_path Local file path to create/write
 * @param chunk_size Size of data chunk to receive
 *
 * Handles chunked file reception with special chunk sizes:
 * - chunk_size = -1: Start new file (truncate if exists)
 * - chunk_size = 0: End of file (close file)
 * - chunk_size > 0: Data chunk to append to file
 *
 * Maintains static file descriptor across multiple PUSH calls
 * for the same file transfer operation.
 */
void handle_push_command(int client_fd, const char *file_path, int chunk_size);

// Connection Management

/**
 * @brief Handle client connection and process commands
 * @param client_fd Socket connected to manager worker thread
 *
 * Main command processing loop for connected clients. Receives commands,
 * parses them, and dispatches to appropriate handlers. Continues until
 * client disconnects or an error occurs.
 *
 * Supported command formats:
 * - "LIST <directory_path>"
 * - "PULL <file_path>"
 * - "PUSH <file_path> <chunk_size> [data]"
 */
void handle_client_connection(int client_fd);

#endif //NFS_CLIENT_LOGIC_H