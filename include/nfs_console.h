/**
 * @file nfs_console.h
 * @brief Network File System Console - User interface and command processing
 *
 * This module implements the console component that provides a user interface
 * for controlling the NFS manager. It handles command parsing, validation,
 * and communication with the manager through TCP connections.
 *
 * Key features:
 * - Interactive command-line interface
 * - Command validation and syntax checking
 * - TCP connection management to manager
 * - Response handling and display
 * - Command logging with timestamps
 */

#ifndef NFS_CONSOLE_H
#define NFS_CONSOLE_H

#include "common.h"

// Command Processing

/**
 * @brief Process user input command and validate syntax
 * @param input Raw input string from user
 * @param command Output buffer for parsed command
 * @param args Output buffer for command arguments
 * @return 0 on success, -1 on invalid command
 *
 * Parses user input into command and arguments, validates command syntax,
 * and checks argument count for known commands. Provides user feedback
 * for invalid commands or missing arguments.
 */
int process_user_command(const char *input, char *command, char *args);

/**
 * @brief Send command to manager and handle response
 * @param sockfd Socket connected to manager
 * @param command Command string to send
 * @param logfile Log file for recording responses
 * @return 0 on success, -1 on communication error
 *
 * Sends the specified command to the manager, receives and displays
 * the response, and logs the interaction with timestamps. Handles
 * network errors and connection issues gracefully.
 */
int send_command_to_manager(int sockfd, const char *command, FILE *logfile);

// Logging

/**
 * @brief Log console command with timestamp
 * @param logfile Console log file handle
 * @param command Command string to log
 *
 * Records user commands in the console log file with timestamps
 * for audit trail and debugging purposes. Safe to call with
 * NULL logfile parameter.
 */
void console_log_command(FILE *logfile, const char *command);

#endif // NFS_CONSOLE_H