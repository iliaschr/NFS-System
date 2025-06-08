/**
 * @file thread_pool.h
 * @brief Thread pool implementation for parallel file synchronization
 *
 * This module provides a robust thread pool implementation using the producer-consumer
 * pattern for handling file synchronization jobs. Worker threads process jobs from
 * a bounded queue, performing file transfers between nfs_client instances.
 *
 * Key features:
 * - Bounded job queue with blocking operations
 * - Thread-safe job submission and retrieval
 * - Graceful shutdown with worker thread cleanup
 * - Individual file synchronization with error handling
 * - Comprehensive logging of sync operations
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "common.h"

// Thread Pool Management

/**
 * @brief Create and initialize thread pool
 * @param thread_count Number of worker threads to create
 * @param buffer_size Maximum capacity of job queue
 * @return Pointer to thread pool on success, NULL on error
 *
 * Creates the specified number of worker threads and initializes
 * all synchronization primitives. Worker threads start immediately
 * and wait for jobs to be submitted.
 */
thread_pool_t* create_thread_pool(int thread_count, int buffer_size);

/**
 * @brief Shutdown and destroy thread pool
 * @param pool Thread pool to destroy
 *
 * Signals all worker threads to shutdown, waits for them to complete
 * current jobs, cleans up remaining queued jobs, and frees all resources.
 */
void destroy_thread_pool(thread_pool_t *pool);

// Job Management

/**
 * @brief Add synchronization job to queue
 * @param pool Thread pool instance
 * @param job Job to enqueue (ownership transferred to pool)
 * @return 0 on success, -1 on error or shutdown
 *
 * Blocks if queue is full until space becomes available or shutdown
 * is signaled. Job memory is managed by the thread pool after submission.
 */
int enqueue_sync_job(thread_pool_t *pool, sync_job_t *job);

/**
 * @brief Remove synchronization job from queue
 * @param pool Thread pool instance
 * @return Pointer to job on success, NULL on shutdown
 *
 * Blocks if queue is empty until a job becomes available or shutdown
 * is signaled. Caller takes ownership of returned job memory.
 */
sync_job_t* dequeue_sync_job(thread_pool_t *pool);

/**
 * @brief Create new synchronization job
 * @param source_host Source server hostname
 * @param source_port Source server port
 * @param source_dir Source directory path
 * @param target_host Target server hostname
 * @param target_port Target server port
 * @param target_dir Target directory path
 * @param filename Name of file to synchronize
 * @return Pointer to new job on success, NULL on error
 */
sync_job_t* create_sync_job(const char *source_host, int source_port, const char *source_dir,
                           const char *target_host, int target_port, const char *target_dir,
                           const char *filename);

/**
 * @brief Free synchronization job memory
 * @param job Job to free
 */
void free_sync_job(sync_job_t *job);

// Worker Thread Functions

/**
 * @brief Main worker thread function
 * @param arg Pointer to thread pool (cast from void*)
 * @return NULL (unused)
 *
 * Worker thread main loop: dequeue jobs, process them, and repeat
 * until shutdown is signaled. Each thread processes one file at a time.
 */
void* worker_thread(void *arg);

/**
 * @brief Synchronize a single file between clients
 * @param job Job containing source/target information
 * @return 0 on success, -1 on error
 *
 * Connects to both source and target clients, retrieves file from source
 * using PULL command, and stores it on target using PUSH command.
 * Handles file transfer in chunks to support large files.
 */
int sync_single_file(sync_job_t *job);

// Thread Pool Control

/**
 * @brief Signal thread pool to shutdown
 * @param pool Thread pool instance
 *
 * Sets shutdown flag and wakes up all waiting worker threads.
 * Non-blocking operation that initiates graceful shutdown.
 */
void signal_shutdown(thread_pool_t *pool);

/**
 * @brief Wait for all worker threads to finish
 * @param pool Thread pool instance
 *
 * Blocks until all worker threads have completed and exited.
 * Should be called after signal_shutdown().
 */
void wait_for_workers(thread_pool_t *pool);

#endif // THREAD_POOL_H