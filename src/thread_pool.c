#include "../include/thread_pool.h"
#include "../include/common.h"

// Global log file for worker threads to use
extern FILE *g_worker_logfile;

thread_pool_t* create_thread_pool(int thread_count, int buffer_size) {
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    if (!pool) {
        fprintf(stderr, "Failed to allocate memory for thread pool\n");
        return NULL;
    }
    
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        fprintf(stderr, "Failed to allocate memory for threads\n");
        free(pool);
        return NULL;
    }
    
    pool->thread_count = thread_count;
    pool->buffer_size = buffer_size;
    pool->job_queue_head = NULL;
    pool->job_queue_tail = NULL;
    pool->queue_size = 0;
    pool->shutdown = 0;
    
    // Initialize synchronization primitives
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize queue mutex\n");
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->queue_not_empty, NULL) != 0) {
        fprintf(stderr, "Failed to initialize queue_not_empty condition\n");
        pthread_mutex_destroy(&pool->queue_mutex);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->queue_not_full, NULL) != 0) {
        fprintf(stderr, "Failed to initialize queue_not_full condition\n");
        pthread_cond_destroy(&pool->queue_not_empty);
        pthread_mutex_destroy(&pool->queue_mutex);
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    // Create worker threads
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            fprintf(stderr, "Failed to create worker thread %d\n", i);
            // Signal shutdown and wait for created threads to finish
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->queue_not_empty);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            pthread_cond_destroy(&pool->queue_not_full);
            pthread_cond_destroy(&pool->queue_not_empty);
            pthread_mutex_destroy(&pool->queue_mutex);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }
    
    printf("Created thread pool with %d workers\n", thread_count);
    return pool;
}

void destroy_thread_pool(thread_pool_t *pool) {
    if (!pool) return;
    
    // Signal shutdown first
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_cond_broadcast(&pool->queue_not_full);
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Wait for all workers to finish
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Clean up remaining jobs in queue
    pthread_mutex_lock(&pool->queue_mutex);
    while (pool->job_queue_head) {
        sync_job_t *job = pool->job_queue_head;
        pool->job_queue_head = job->next;
        free_sync_job(job);
    }
    pthread_mutex_unlock(&pool->queue_mutex);
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&pool->queue_not_full);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_mutex_destroy(&pool->queue_mutex);
    
    free(pool->threads);
    free(pool);
}

sync_job_t* create_sync_job(const char *source_host, int source_port, const char *source_dir,
                           const char *target_host, int target_port, const char *target_dir,
                           const char *filename) {
    sync_job_t *job = malloc(sizeof(sync_job_t));
    if (!job) {
        fprintf(stderr, "Failed to allocate memory for sync job\n");
        return NULL;
    }
    
    strncpy(job->source_host, source_host, MAX_HOST_SIZE - 1);
    job->source_host[MAX_HOST_SIZE - 1] = '\0';
    job->source_port = source_port;
    strncpy(job->source_dir, source_dir, MAX_PATH - 1);
    job->source_dir[MAX_PATH - 1] = '\0';
    
    strncpy(job->target_host, target_host, MAX_HOST_SIZE - 1);
    job->target_host[MAX_HOST_SIZE - 1] = '\0';
    job->target_port = target_port;
    strncpy(job->target_dir, target_dir, MAX_PATH - 1);
    job->target_dir[MAX_PATH - 1] = '\0';
    
    strncpy(job->filename, filename, MAX_FILENAME - 1);
    job->filename[MAX_FILENAME - 1] = '\0';
    
    job->next = NULL;
    
    return job;
}

void free_sync_job(sync_job_t *job) {
    if (job) {
        free(job);
    }
}

int enqueue_sync_job(thread_pool_t *pool, sync_job_t *job) {
    if (!pool || !job) return -1;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    // Check if shutting down
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    // Wait if queue is full
    while (pool->queue_size >= pool->buffer_size && !pool->shutdown) {
        pthread_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
    }
    
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_mutex);
        return -1;
    }
    
    // Add job to queue
    if (pool->job_queue_tail) {
        pool->job_queue_tail->next = job;
    } else {
        pool->job_queue_head = job;
    }
    pool->job_queue_tail = job;
    pool->queue_size++;
    
    // Signal that queue is not empty
    pthread_cond_signal(&pool->queue_not_empty);
    
    pthread_mutex_unlock(&pool->queue_mutex);
    return 0;
}

sync_job_t* dequeue_sync_job(thread_pool_t *pool) {
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->queue_mutex);
    
    // Wait if queue is empty and not shutting down
    while (pool->queue_size == 0 && !pool->shutdown) {
        pthread_cond_wait(&pool->queue_not_empty, &pool->queue_mutex);
    }
    
    if (pool->shutdown && pool->queue_size == 0) {
        pthread_mutex_unlock(&pool->queue_mutex);
        return NULL;
    }
    
    // Remove job from queue
    sync_job_t *job = pool->job_queue_head;
    if (job) {
        pool->job_queue_head = job->next;
        if (!pool->job_queue_head) {
            pool->job_queue_tail = NULL;
        }
        pool->queue_size--;
        job->next = NULL;
        
        // Signal that queue is not full
        pthread_cond_signal(&pool->queue_not_full);
    }
    
    pthread_mutex_unlock(&pool->queue_mutex);
    return job;
}

int sync_single_file(sync_job_t *job) {
    if (!job) return -1;
    
    char source_path[MAX_PATH * 2];
    char target_path[MAX_PATH * 2];
    char command[MAX_COMMAND_SIZE];
    char response[MAX_BUFFER_SIZE];
    
    // Build full paths
    snprintf(source_path, sizeof(source_path), "%s/%s", job->source_dir, job->filename);
    snprintf(target_path, sizeof(target_path), "%s/%s", job->target_dir, job->filename);
    
    // Connect to source client
    int source_fd = connect_to_server(job->source_host, job->source_port);
    if (source_fd < 0) {
        if (g_worker_logfile) {
            char timestamp[32];
            get_timestamp(timestamp, sizeof(timestamp));
            fprintf(g_worker_logfile, "[%s] [%s@%s:%d] [%s@%s:%d] [%d] [PULL] [ERROR] [Connection failed to source: %s]\n",
                   timestamp, job->source_dir, job->source_host, job->source_port,
                   job->target_dir, job->target_host, job->target_port,
                   (int)pthread_self(), strerror(errno));
            fflush(g_worker_logfile);
        }
        return -1;
    }
    
    // Connect to target client
    int target_fd = connect_to_server(job->target_host, job->target_port);
    if (target_fd < 0) {
        if (g_worker_logfile) {
            char timestamp[32];
            get_timestamp(timestamp, sizeof(timestamp));
            fprintf(g_worker_logfile, "[%s] [%s@%s:%d] [%s@%s:%d] [%d] [PUSH] [ERROR] [Connection failed to target: %s]\n",
                   timestamp, job->source_dir, job->source_host, job->source_port,
                   job->target_dir, job->target_host, job->target_port,
                   (int)pthread_self(), strerror(errno));
            fflush(g_worker_logfile);
        }
        close(source_fd);
        return -1;
    }
    
    // Send PULL command to source
    snprintf(command, sizeof(command), "PULL %s\n", source_path);
    if (send_command(source_fd, command) != 0) {
        close(source_fd);
        close(target_fd);
        return -1;
    }
    
    // Read file size first
    ssize_t received = recv(source_fd, response, sizeof(response) - 1, 0);
    if (received <= 0) {
        close(source_fd);
        close(target_fd);
        return -1;
    }
    response[received] = '\0';
    
    // Parse file size
    long file_size;
    char *space_pos = strchr(response, ' ');
    if (!space_pos) {
        close(source_fd);
        close(target_fd);
        return -1;
    }
    
    file_size = atol(response);
    if (file_size < 0) {
        // Error from source
        if (g_worker_logfile) {
            char timestamp[32];
            get_timestamp(timestamp, sizeof(timestamp));
            fprintf(g_worker_logfile, "[%s] [%s@%s:%d] [%s@%s:%d] [%d] [PULL] [ERROR] [File: %s - %s]\n",
                   timestamp, job->source_dir, job->source_host, job->source_port,
                   job->target_dir, job->target_host, job->target_port,
                   (int)pthread_self(), job->filename, space_pos + 1);
            fflush(g_worker_logfile);
        }
        close(source_fd);
        close(target_fd);
        return -1;
    }
    
    // Start PUSH to target (send -1 to indicate start of new file)
    snprintf(command, sizeof(command), "PUSH %s -1\n", target_path);
    if (send_command(target_fd, command) != 0) {
        close(source_fd);
        close(target_fd);
        return -1;
    }
    
    // Transfer file data in chunks
    long total_transferred = 0;
    char buffer[MAX_BUFFER_SIZE];
    
    // Skip the size header in first read
    size_t header_len = space_pos - response + 1;
    if ((size_t)received > header_len) {  // Fix signed/unsigned comparison
        // We have some file data in the first read
        size_t data_in_first_read = received - header_len;
        
        // Send this chunk to target
        snprintf(command, sizeof(command), "PUSH %s %zu ", target_path, data_in_first_read);
        send_command(target_fd, command);
        send(target_fd, space_pos + 1, data_in_first_read, 0);
        
        total_transferred += data_in_first_read;
    }
    
    // Continue reading and transferring
    while (total_transferred < file_size) {
        ssize_t chunk_size = recv(source_fd, buffer, sizeof(buffer), 0);
        if (chunk_size <= 0) break;
        
        // Send chunk to target
        snprintf(command, sizeof(command), "PUSH %s %zd ", target_path, chunk_size);
        send_command(target_fd, command);
        send(target_fd, buffer, chunk_size, 0);
        
        total_transferred += chunk_size;
    }
    
    // Send end-of-file marker to target
    snprintf(command, sizeof(command), "PUSH %s 0\n", target_path);
    send_command(target_fd, command);
    
    close(source_fd);
    close(target_fd);
    
    // Log successful transfer
    if (g_worker_logfile) {
        char timestamp[32];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(g_worker_logfile, "[%s] [%s@%s:%d] [%s@%s:%d] [%d] [PULL] [SUCCESS] [%ld bytes pulled]\n",
               timestamp, job->source_dir, job->source_host, job->source_port,
               job->target_dir, job->target_host, job->target_port,
               (int)pthread_self(), total_transferred);
        fprintf(g_worker_logfile, "[%s] [%s@%s:%d] [%s@%s:%d] [%d] [PUSH] [SUCCESS] [%ld bytes pushed]\n",
               timestamp, job->source_dir, job->source_host, job->source_port,
               job->target_dir, job->target_host, job->target_port,
               (int)pthread_self(), total_transferred);
        fflush(g_worker_logfile);
    }
    
    return 0;
}

void* worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t*)arg;
    
    printf("Worker thread %d started\n", (int)pthread_self());
    
    while (1) {
        sync_job_t *job = dequeue_sync_job(pool);
        if (!job) {
            // Pool is shutting down
            break;
        }
        
        printf("Worker %d processing file: %s\n", (int)pthread_self(), job->filename);
        
        // Process the sync job
        if (sync_single_file(job) != 0) {
            printf("Worker %d failed to sync file: %s\n", (int)pthread_self(), job->filename);
        } else {
            printf("Worker %d successfully synced file: %s\n", (int)pthread_self(), job->filename);
        }
        
        free_sync_job(job);
    }
    
    printf("Worker thread %d finished\n", (int)pthread_self());
    return NULL;
}

void signal_shutdown(thread_pool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_cond_broadcast(&pool->queue_not_full);
    pthread_mutex_unlock(&pool->queue_mutex);
}

void wait_for_workers(thread_pool_t *pool) {
    if (!pool) return;
    
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}