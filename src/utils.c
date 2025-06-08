#include "../include/common.h"
#include "../include/nfs_client_logic.h"

void get_timestamp(char* buffer, size_t size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(buffer, size, TIMESTAMP_FORMAT, tm_info);
}

int create_server_socket(int port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
    return -1; 
  }
  
  
  // Allow reuse of address
  int opt = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    fprintf(stderr, "Error setting socket options: %s\n", strerror(errno));
    close(sockfd);
    return -1;
  }
  
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  
  if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
    close(sockfd);
    return -1;
  }
  
  if (listen(sockfd, MAX_CONNECTIONS) < 0) {
    fprintf(stderr, "Error listening on socket: %s\n", strerror(errno));
    close(sockfd);
    return -1;
  }
  
  return sockfd;
}

void log_message(FILE *logfile, const char *format, ...) {
    if (!format) {
        fprintf(stderr, "Warning: log_message called with NULL format\n");
        return;
    }
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    va_list args;
    va_start(args, format);
    
    // Create a safe buffer for the formatted message
    char message_buffer[MAX_BUFFER_SIZE];
    int result = vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    
    if (result < 0) {
        fprintf(stderr, "Warning: log_message formatting failed\n");
        va_end(args);
        return;
    }
    
    // Print to stdout
    printf("[%s] %s\n", timestamp, message_buffer);
    
    // Write to logfile if provided
    if (logfile) {
        fprintf(logfile, "[%s] %s\n", timestamp, message_buffer);
        fflush(logfile);
    }
    
    va_end(args);
}


int connect_to_server(const char *host, int port) {
  if (!host) {
      fprintf(stderr, "Error: NULL host provided to connect_to_server\n");
      return -1;
  }
  
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
      fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
      return -1;
  }
  
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  
  if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
      fprintf(stderr, "Invalid address: %s\n", host);
      close(sockfd);
      return -1;
  }
  
  if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      fprintf(stderr, "Error connecting to %s:%d - %s\n", host, port, strerror(errno));
      close(sockfd);
      return -1;
  }
  
  return sockfd;
}

void cleanup_socket(int sockfd) {
  if (sockfd >= 0) {
      close(sockfd);
  }
}

int send_command(int sockfd, const char *command) {
  if (!command) {
      fprintf(stderr, "Error: NULL command provided to send_command\n");
      return -1;
  }
  
  size_t len = strlen(command);
  ssize_t sent = send(sockfd, command, len, 0);
  if (sent < 0) {
      fprintf(stderr, "Error sending command: %s\n", strerror(errno));
      return -1;
  }
  if ((size_t)sent != len) {
      fprintf(stderr, "Partial send: sent %zd of %zu bytes\n", sent, len);
      return -1;
  }
  return 0;
}

int receive_response(int sockfd, char *buffer, size_t buffer_size) {
  if (!buffer) {
      fprintf(stderr, "Error: NULL buffer provided to receive_response\n");
      return -1;
  }
  
  ssize_t received = recv(sockfd, buffer, buffer_size - 1, 0);
  if (received < 0) {
      fprintf(stderr, "Error receiving response: %s\n", strerror(errno));
      return -1;
  }
  buffer[received] = '\0';
  return received;
}

int parse_directory_spec(const char *spec, char *host, int *port, char *dir) {
  if (!spec || !host || !port || !dir) {
      fprintf(stderr, "Error: NULL parameter provided to parse_directory_spec\n");
      return -1;
  }
  
  // Parse format: /path@host:port
  char *at_pos = strchr(spec, '@');
  if (!at_pos) {
      fprintf(stderr, "Invalid directory specification: %s\n", spec);
      return -1;
  }
  
  // Extract directory path
  size_t dir_len = at_pos - spec;
  if (dir_len >= MAX_PATH) {
      fprintf(stderr, "Directory path too long in: %s\n", spec);
      return -1;
  }
  strncpy(dir, spec, dir_len);
  dir[dir_len] = '\0';
  
  // Extract host and port
  char *colon_pos = strchr(at_pos + 1, ':');
  if (!colon_pos) {
      fprintf(stderr, "Invalid directory specification: %s\n", spec);
      return -1;
  }
  
  size_t host_len = colon_pos - (at_pos + 1);
  if (host_len >= MAX_HOST_SIZE) {
      fprintf(stderr, "Host name too long in: %s\n", spec);
      return -1;
  }
  strncpy(host, at_pos + 1, host_len);
  host[host_len] = '\0';
  
  *port = atoi(colon_pos + 1);
  if (*port <= 0) {
      fprintf(stderr, "Invalid port number in: %s\n", spec);
      return -1;
  }
  
  return 0;
}

int parse_config_line(const char *line, char *source_host, int *source_port, 
                   char *source_dir, char *target_host, int *target_port, char *target_dir) {
  if (!line || !source_host || !source_port || !source_dir || 
      !target_host || !target_port || !target_dir) {
      fprintf(stderr, "Error: NULL parameter provided to parse_config_line\n");
      return -1;
  }
  
  char source_spec[MAX_PATH], target_spec[MAX_PATH];
  
  if (sscanf(line, "%s %s", source_spec, target_spec) != 2) {
      fprintf(stderr, "Invalid config line format: %s\n", line);
      return -1;
  }
  
  if (parse_directory_spec(source_spec, source_host, source_port, source_dir) != 0) {
      return -1;
  }
  
  if (parse_directory_spec(target_spec, target_host, target_port, target_dir) != 0) {
      return -1;
  }
  
  return 0;
}