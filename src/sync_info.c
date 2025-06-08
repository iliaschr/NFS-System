#include "../include/sync_info.h"

sync_info_store_t* create_sync_info_store(void) {
    sync_info_store_t *store = malloc(sizeof(sync_info_store_t));
    if (!store) {
        fprintf(stderr, "Failed to allocate memory for sync info store\n");
        return NULL;
    }
    
    store->head = NULL;
    store->count = 0;
    
    if (pthread_mutex_init(&store->mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize sync info store mutex: %s\n", strerror(errno));
        free(store);
        return NULL;
    }
    
    return store;
}

void destroy_sync_info_store(sync_info_store_t *store) {
    if (!store) return;
    
    pthread_mutex_lock(&store->mutex);
    
    // Free all sync info entries
    sync_info_t *current = store->head;
    while (current) {
        sync_info_t *next = current->next;
        free_sync_info(current);
        current = next;
    }
    
    pthread_mutex_unlock(&store->mutex);
    pthread_mutex_destroy(&store->mutex);
    free(store);
}

sync_info_t* create_sync_info(const char *source_host, int source_port, const char *source_dir,
                             const char *target_host, int target_port, const char *target_dir) {
    if (!source_host || !source_dir || !target_host || !target_dir) {
        fprintf(stderr, "Invalid parameters for creating sync info\n");
        return NULL;
    }
    
    sync_info_t *info = malloc(sizeof(sync_info_t));
    if (!info) {
        fprintf(stderr, "Failed to allocate memory for sync info\n");
        return NULL;
    }
    
    // Copy source information
    strncpy(info->source_host, source_host, MAX_HOST_SIZE - 1);
    info->source_host[MAX_HOST_SIZE - 1] = '\0';
    info->source_port = source_port;
    strncpy(info->source_dir, source_dir, MAX_PATH - 1);
    info->source_dir[MAX_PATH - 1] = '\0';
    
    // Copy target information
    strncpy(info->target_host, target_host, MAX_HOST_SIZE - 1);
    info->target_host[MAX_HOST_SIZE - 1] = '\0';
    info->target_port = target_port;
    strncpy(info->target_dir, target_dir, MAX_PATH - 1);
    info->target_dir[MAX_PATH - 1] = '\0';
    
    // Initialize metadata
    info->active = 1;
    info->last_sync_time = time(NULL);
    info->error_count = 0;
    info->next = NULL;
    
    return info;
}

void free_sync_info(sync_info_t *info) {
    if (info) {
        free(info);
    }
}

int add_sync_info(sync_info_store_t *store, sync_info_t *info) {
    if (!store || !info) {
        return -1;
    }
    
    pthread_mutex_lock(&store->mutex);
    
    // Check if entry already exists
    sync_info_t *current = store->head;
    while (current) {
        if (strcmp(current->source_host, info->source_host) == 0 &&
            current->source_port == info->source_port &&
            strcmp(current->source_dir, info->source_dir) == 0) {
            // Already exists
            pthread_mutex_unlock(&store->mutex);
            return 1;
        }
        current = current->next;
    }
    
    // Add to front of list
    info->next = store->head;
    store->head = info;
    store->count++;
    
    pthread_mutex_unlock(&store->mutex);
    return 0;
}

sync_info_t* find_sync_info(sync_info_store_t *store, const char *source_host, int source_port, const char *source_dir) {
    if (!store || !source_host || !source_dir) {
        return NULL;
    }
    
    pthread_mutex_lock(&store->mutex);
    
    sync_info_t *current = store->head;
    while (current) {
        if (strcmp(current->source_host, source_host) == 0 &&
            current->source_port == source_port &&
            strcmp(current->source_dir, source_dir) == 0) {
            pthread_mutex_unlock(&store->mutex);
            return current;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&store->mutex);
    return NULL;
}

int remove_sync_info(sync_info_store_t *store, const char *source_host, int source_port, const char *source_dir) {
    if (!store || !source_host || !source_dir) {
        return -1;
    }
    
    pthread_mutex_lock(&store->mutex);
    
    sync_info_t *current = store->head;
    sync_info_t *prev = NULL;
    
    while (current) {
        if (strcmp(current->source_host, source_host) == 0 &&
            current->source_port == source_port &&
            strcmp(current->source_dir, source_dir) == 0) {
            
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                store->head = current->next;
            }
            
            store->count--;
            free_sync_info(current);
            
            pthread_mutex_unlock(&store->mutex);
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&store->mutex);
    return 1; // Not found
}

int deactivate_sync_info(sync_info_store_t *store, const char *source_host, int source_port, const char *source_dir) {
    if (!store || !source_host || !source_dir) {
        return -1;
    }
    
    sync_info_t *info = find_sync_info(store, source_host, source_port, source_dir);
    if (!info) {
        return 1; // Not found
    }
    
    pthread_mutex_lock(&store->mutex);
    info->active = 0;
    pthread_mutex_unlock(&store->mutex);
    
    return 0;
}

void print_sync_info_store(sync_info_store_t *store) {
    if (!store) {
        printf("Sync info store is NULL\n");
        return;
    }
    
    pthread_mutex_lock(&store->mutex);
    
    printf("=== Sync Info Store (Count: %d) ===\n", store->count);
    
    sync_info_t *current = store->head;
    int index = 0;
    
    while (current) {
        char timestamp[32];
        struct tm *tm_info = localtime(&current->last_sync_time);
        strftime(timestamp, sizeof(timestamp), TIMESTAMP_FORMAT, tm_info);
        
        printf("%d. Source: %s@%s:%d\n", 
               index + 1, current->source_dir, current->source_host, current->source_port);
        printf("   Target: %s@%s:%d\n", 
               current->target_dir, current->target_host, current->target_port);
        printf("   Active: %s, Last Sync: %s, Errors: %d\n", 
               current->active ? "Yes" : "No", timestamp, current->error_count);
        printf("\n");
        
        current = current->next;
        index++;
    }
    
    if (store->count == 0) {
        printf("No sync pairs configured.\n");
    }
    
    printf("=====================================\n");
    
    pthread_mutex_unlock(&store->mutex);
}

int get_sync_info_count(sync_info_store_t *store) {
    if (!store) return 0;
    
    pthread_mutex_lock(&store->mutex);
    int count = store->count;
    pthread_mutex_unlock(&store->mutex);
    
    return count;
}