/**
 * @file sync_info.h
 * @brief Synchronization pair information management
 *
 * This module provides thread-safe storage and management of synchronization
 * pair information. It maintains metadata about source-target directory pairs
 * being monitored for synchronization, including status tracking and error counts.
 *
 * The store uses a simple linked list with mutex protection for thread safety.
 * Operations include adding new sync pairs, finding existing pairs, deactivating
 * synchronization, and removing pairs from the store.
 */

#ifndef SYNC_INFO_H
#define SYNC_INFO_H

#include "common.h"

/**
 * @brief Thread-safe store for synchronization pair information
 *
 * Maintains a linked list of sync_info_t structures with thread-safe
 * access through mutex protection. Tracks the total count of stored pairs.
 */
typedef struct sync_info_store {
    sync_info_t *head;               ///< Head of sync info linked list
    pthread_mutex_t mutex;           ///< Mutex for thread-safe access
    int count;                       ///< Current number of sync pairs
} sync_info_store_t;

// Store Management

/**
 * @brief Create new synchronization info store
 * @return Pointer to new store on success, NULL on error
 *
 * Allocates and initializes a new sync info store with empty list
 * and initialized mutex for thread-safe operations.
 */
sync_info_store_t* create_sync_info_store(void);

/**
 * @brief Destroy synchronization info store
 * @param store Store to destroy
 *
 * Frees all sync info entries, destroys mutex, and frees store memory.
 * Safe to call with NULL pointer.
 */
void destroy_sync_info_store(sync_info_store_t *store);

// Sync Info Operations

/**
 * @brief Create new synchronization info entry
 * @param source_host Source server hostname
 * @param source_port Source server port
 * @param source_dir Source directory path
 * @param target_host Target server hostname
 * @param target_port Target server port
 * @param target_dir Target directory path
 * @return Pointer to new sync info on success, NULL on error
 *
 * Creates and initializes a new sync info structure with the provided
 * parameters. Sets active flag to true and initializes timestamps.
 */
sync_info_t* create_sync_info(const char *source_host, int source_port, const char *source_dir,
                             const char *target_host, int target_port, const char *target_dir);

/**
 * @brief Free synchronization info memory
 * @param info Sync info to free
 */
void free_sync_info(sync_info_t *info);

// Store Operations  

/**
 * @brief Add synchronization info to store
 * @param store Sync info store
 * @param info Sync info to add (ownership transferred to store)
 * @return 0 on success, 1 if already exists, -1 on error
 *
 * Thread-safe operation to add sync info to store. Checks for duplicates
 * based on source host, port, and directory. Adds to front of list.
 */
int add_sync_info(sync_info_store_t *store, sync_info_t *info);

/**
 * @brief Find synchronization info in store
 * @param store Sync info store
 * @param source_host Source hostname to search for
 * @param source_port Source port to search for
 * @param source_dir Source directory to search for
 * @return Pointer to sync info if found, NULL if not found
 *
 * Thread-safe search operation. Returns pointer to sync info structure
 * still owned by the store (do not free).
 */
sync_info_t* find_sync_info(sync_info_store_t *store, const char *source_host, int source_port, const char *source_dir);

/**
 * @brief Remove synchronization info from store
 * @param store Sync info store
 * @param source_host Source hostname to remove
 * @param source_port Source port to remove
 * @param source_dir Source directory to remove
 * @return 0 on success, 1 if not found, -1 on error
 *
 * Thread-safe removal operation. Frees the sync info memory and
 * updates the linked list structure.
 */
int remove_sync_info(sync_info_store_t *store, const char *source_host, int source_port, const char *source_dir);

/**
 * @brief Deactivate synchronization without removing from store
 * @param store Sync info store
 * @param source_host Source hostname to deactivate
 * @param source_port Source port to deactivate
 * @param source_dir Source directory to deactivate
 * @return 0 on success, 1 if not found, -1 on error
 *
 * Sets the active flag to false, effectively stopping synchronization
 * while keeping the configuration for potential future reactivation.
 */
int deactivate_sync_info(sync_info_store_t *store, const char *source_host, int source_port, const char *source_dir);

// Utility Functions

/**
 * @brief Print all synchronization pairs in store
 * @param store Sync info store to display
 *
 * Thread-safe operation to print formatted information about all
 * sync pairs in the store, including status and timestamps.
 */
void print_sync_info_store(sync_info_store_t *store);

/**
 * @brief Get current count of sync pairs in store
 * @param store Sync info store
 * @return Number of sync pairs in store, 0 if store is NULL
 */
int get_sync_info_count(sync_info_store_t *store);

#endif // SYNC_INFO_H