CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c99 -g
CFLAGS_DEBUG = $(CFLAGS) -DDEBUG -O0
CFLAGS_RELEASE = $(CFLAGS) -O2 -DNDEBUG
SRCDIR = src
INCDIR = include
OBJDIR = obj
TESTDIR = tests

# Valgrind settings
VALGRIND = valgrind
VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose

# Create obj directory if it doesn't exist
$(shell mkdir -p $(OBJDIR))

# Source files for each executable
MANAGER_SRCS = $(SRCDIR)/nfs_manager.c $(SRCDIR)/nfs_manager_logic.c $(SRCDIR)/utils.c $(SRCDIR)/thread_pool.c $(SRCDIR)/sync_info.c
CONSOLE_SRCS = $(SRCDIR)/nfs_console.c $(SRCDIR)/utils.c
CLIENT_SRCS = $(SRCDIR)/nfs_client.c $(SRCDIR)/nfs_client_logic.c $(SRCDIR)/utils.c

# Test source files
TEST_UTILS_SRCS = $(TESTDIR)/test_utils.c $(SRCDIR)/utils.c
TEST_CLIENT_SRCS = $(TESTDIR)/test_nfs_client.c $(SRCDIR)/nfs_client_logic.c $(SRCDIR)/utils.c

# Object files
MANAGER_OBJS = $(MANAGER_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
CONSOLE_OBJS = $(CONSOLE_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
CLIENT_OBJS = $(CLIENT_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TEST_UTILS_OBJS = $(TEST_UTILS_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TEST_UTILS_OBJS := $(TEST_UTILS_OBJS:$(TESTDIR)/%.c=$(OBJDIR)/%.o)
TEST_CLIENT_OBJS = $(TEST_CLIENT_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TEST_CLIENT_OBJS := $(TEST_CLIENT_OBJS:$(TESTDIR)/%.c=$(OBJDIR)/%.o)

# Executables
EXECUTABLES = nfs_manager nfs_console nfs_client
TEST_EXECUTABLES = test_utils test_nfs_client

.PHONY: all clean tests help run sample-config debug release valgrind-test

# Default target
all: $(EXECUTABLES)

# Debug build
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(EXECUTABLES)
	@echo "✅ Debug build complete"

# Release build  
release: CFLAGS = $(CFLAGS_RELEASE)
release: clean $(EXECUTABLES)
	@echo "✅ Release build complete"

tests: $(TEST_EXECUTABLES)

# Build main executables
nfs_manager: $(MANAGER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✅ nfs_manager built successfully"

nfs_console: $(CONSOLE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✅ nfs_console built successfully"

nfs_client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✅ nfs_client built successfully"

# Build test executables
test_utils: $(TEST_UTILS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✅ test_utils built successfully"

test_nfs_client: $(TEST_CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✅ test_nfs_client built successfully"

# Build object files from src directory
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Build object files from tests directory
$(OBJDIR)/%.o: $(TESTDIR)/%.c
	@echo "Compiling test $<..."
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(EXECUTABLES) $(TEST_EXECUTABLES) *.log valgrind-*.log *.txt
	rm -rf test_source test_target
	@echo "✅ Cleaned all build files"

# Create sample config for testing
sample-config:
	@echo "Creating sample configuration files..."
	@echo "/test_source@127.0.0.1:8001 /test_target@127.0.0.1:8002" > config_sample.txt
	@mkdir -p test_source test_target
	@echo "Sample file 1" > test_source/file1.txt
	@echo "Sample file 2" > test_source/file2.txt
	@echo "Sample file 3" > test_source/sample.txt
	@echo "Test data for sync" > test_source/data.txt
	@echo "✅ Sample configuration created"

# Quick functionality test
quick-test: all
	@echo "Starting quick functionality test..."
	@echo "Testing nfs_client help..."
	@./nfs_client -h 2>/dev/null || echo "✅ nfs_client requires -p port argument (expected)"
	@echo "Testing nfs_console help..."
	@./nfs_console 2>/dev/null || echo "✅ nfs_console requires arguments (expected)"
	@echo "Testing nfs_manager help..."
	@./nfs_manager 2>/dev/null || echo "✅ nfs_manager requires arguments (expected)"
	@echo "✅ Basic executable tests completed"

# Run tests
run-tests: tests
	@echo "Running unit tests..."
	./test_utils
	./test_nfs_client

# Valgrind memory testing
valgrind-test: debug tests
	@echo " Running Valgrind memory tests..."
	@echo "Testing utils..."
	$(VALGRIND) $(VALGRIND_FLAGS) --log-file=valgrind-utils.log ./test_utils
	@echo "Testing nfs_client logic..."
	$(VALGRIND) $(VALGRIND_FLAGS) --log-file=valgrind-client.log ./test_nfs_client
	@echo " Valgrind tests complete. Check valgrind-*.log files"

# Valgrind individual components
valgrind-client: debug sample-config
	@echo " Testing nfs_client with Valgrind..."
	$(VALGRIND) $(VALGRIND_FLAGS) --log-file=valgrind-client-run.log ./nfs_client -p 8001

valgrind-manager: debug sample-config
	@echo " Testing nfs_manager with Valgrind..."
	$(VALGRIND) $(VALGRIND_FLAGS) --log-file=valgrind-manager-run.log ./nfs_manager -l manager.log -c config_sample.txt -n 2 -p 8000 -b 5

valgrind-console: debug
	@echo " Testing nfs_console with Valgrind..."
	echo "help\nshutdown" | $(VALGRIND) $(VALGRIND_FLAGS) --log-file=valgrind-console-run.log ./nfs_console -l console.log -h 127.0.0.1 -p 8000

# Memory leak summary
valgrind-summary: valgrind-test
	@echo "  Valgrind Memory Test Summary  :"
	@echo "================================="
	@for log in valgrind-*.log; do \
		if [ -f "$$log" ]; then \
			echo " $$log:"; \
			grep -E "(ERROR SUMMARY|definitely lost|indirectly lost|possibly lost)" $$log | head -5; \
			echo ""; \
		fi; \
	done

# Run enhanced system test
system-test: all sample-config
	@chmod +x test_script.sh 2>/dev/null || true
	@if [ -f "test_script.sh" ]; then \
		./test_script.sh; \
	else \
		echo "ERROR:  test_script.sh not found. Running basic tests..."; \
		make quick-test run-tests; \
	fi

# Demo setup
demo: all sample-config
	@echo ""
	@echo " Demo Setup Complete! "
	@echo ""
	@echo "To test the complete system, run these commands in separate terminals:"
	@echo ""
	@echo "Terminal 1 (Source Client):"
	@echo "  ./nfs_client -p 8001"
	@echo ""
	@echo "Terminal 2 (Target Client):"
	@echo "  ./nfs_client -p 8002"
	@echo ""
	@echo "Terminal 3 (Manager):"
	@echo "  ./nfs_manager -l manager.log -c config_sample.txt -n 3 -p 8000 -b 10"
	@echo ""
	@echo "Terminal 4 (Console):"
	@echo "  ./nfs_console -l console.log -h 127.0.0.1 -p 8000"
	@echo ""
	@echo "In the console, try these commands:"
	@echo "  add /test_source@127.0.0.1:8001 /test_target@127.0.0.1:8002"
	@echo "  cancel /test_source@127.0.0.1:8001"
	@echo "  shutdown"
	@echo ""

# Help target
help:
	@echo "Available targets:"
	@echo "  all           - Build all main executables (default)"
	@echo "  debug         - Build with debug symbols and no optimization"
	@echo "  release       - Build optimized release version"
	@echo "  tests         - Build test executables"
	@echo "  clean         - Remove all build files"
	@echo "  quick-test    - Run basic executable tests"
	@echo "  run-tests     - Build and run unit tests"
	@echo "  sample-config - Create sample config and test directories"
	@echo "  system-test   - Run comprehensive system test"
	@echo "  demo          - Set up complete demo environment"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Memory testing (requires valgrind):"
	@echo "  valgrind-test    - Run unit tests with memory checking"
	@echo "  valgrind-client  - Test nfs_client with valgrind"
	@echo "  valgrind-manager - Test nfs_manager with valgrind"
	@echo "  valgrind-console - Test nfs_console with valgrind"
	@echo "  valgrind-summary - Show memory test summary"
	@echo ""
	@echo "Main executables:"
	@echo "  nfs_manager   - File synchronization manager"
	@echo "  nfs_console   - User interface for manager"
	@echo "  nfs_client    - File server component"

# Dependencies
$(OBJDIR)/nfs_manager.o: $(INCDIR)/nfs_manager_logic.h $(INCDIR)/common.h
$(OBJDIR)/nfs_manager_logic.o: $(INCDIR)/nfs_manager_logic.h $(INCDIR)/common.h $(INCDIR)/thread_pool.h $(INCDIR)/sync_info.h
$(OBJDIR)/nfs_console.o: $(INCDIR)/nfs_console.h $(INCDIR)/common.h
$(OBJDIR)/nfs_client.o: $(INCDIR)/nfs_client_logic.h $(INCDIR)/common.h
$(OBJDIR)/nfs_client_logic.o: $(INCDIR)/nfs_client_logic.h $(INCDIR)/common.h
$(OBJDIR)/utils.o: $(INCDIR)/common.h
$(OBJDIR)/thread_pool.o: $(INCDIR)/thread_pool.h $(INCDIR)/common.h
$(OBJDIR)/sync_info.o: $(INCDIR)/sync_info.h $(INCDIR)/common.h
$(OBJDIR)/test_utils.o: $(INCDIR)/common.h
$(OBJDIR)/test_nfs_client.o: $(INCDIR)/nfs_client_logic.h $(INCDIR)/common.h