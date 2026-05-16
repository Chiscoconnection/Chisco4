// Chiscoconnection OS - Enhanced Kernel
// Cross-platform (Windows, macOS, Linux)
// A realistic OS simulator with process management, memory, interrupts, and scheduling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Cross-platform sleep
#ifdef _WIN32
    #include <windows.h>
    #define OS_SLEEP(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define OS_SLEEP(ms) usleep(ms * 1000)
#endif

#define MAX_PROCESSES 32
#define MAX_MEMORY 4096
#define MAX_FILES 50
#define MAX_SYSCALLS 100
#define TIME_SLICE 100 // milliseconds for round-robin scheduling

// Process states
typedef enum {
    STATE_NEW = 0,
    STATE_READY = 1,
    STATE_RUNNING = 2,
    STATE_WAITING = 3,
    STATE_BLOCKED = 4,
    STATE_TERMINATED = -1
} ProcessState;

// Process Control Block
typedef struct {
    int pid;
    char name[64];
    ProcessState state;
    int priority;           // 0-10, higher = more important
    int memory_start;
    int memory_size;
    int cpu_time;          // total CPU time used (in ticks)
    int creation_time;
    int wait_time;
    int io_wait_count;
} ProcessControlBlock;

// Memory management
typedef struct {
    int page_number;
    int process_pid;
    int valid;
} PageTableEntry;

// File structure
typedef struct {
    char name[64];
    char content[512];
    int in_use;
    int size;
    int owner_pid;
    time_t created_time;
} File;

// System call tracker
typedef struct {
    int pid;
    char syscall_name[32];
    time_t timestamp;
} SyscallLog;

// Global kernel state
ProcessControlBlock process_table[MAX_PROCESSES];
PageTableEntry page_table[MAX_MEMORY / 256];  // Simple paging
File file_system[MAX_FILES];
SyscallLog syscall_log[MAX_SYSCALLS];
char memory[MAX_MEMORY];
int current_pid = 1000;
int num_processes = 0;
int syscall_count = 0;
int total_cpu_ticks = 0;
int ready_queue[MAX_PROCESSES];
int ready_queue_size = 0;

// ==================== INITIALIZATION ====================

void init_kernel() {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║    Chiscoconnection OS - Enhanced Kernel v2.0      ║\n");
    printf("║         Cross-Platform Operating System           ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");
    
    memset(process_table, 0, sizeof(process_table));
    memset(file_system, 0, sizeof(file_system));
    memset(memory, 0, MAX_MEMORY);
    memset(page_table, 0, sizeof(page_table));
    
    printf("[KERNEL] Initializing memory management...\n");
    printf("[KERNEL] Page size: 256 bytes\n");
    printf("[KERNEL] Total pages: %d\n", MAX_MEMORY / 256);
    printf("[KERNEL] Total memory: %d bytes\n\n", MAX_MEMORY);
}

// ==================== PROCESS MANAGEMENT ====================

int create_process(const char *name, int priority, int mem_size) {
    if (num_processes >= MAX_PROCESSES) {
        printf("[ERROR] Process table full! Cannot create %s\n", name);
        return -1;
    }
    
    if (mem_size > MAX_MEMORY / MAX_PROCESSES) {
        printf("[ERROR] Insufficient memory for process %s\n", name);
        return -1;
    }
    
    int pid = current_pid++;
    int idx = num_processes;
    
    process_table[idx].pid = pid;
    strncpy(process_table[idx].name, name, 63);
    process_table[idx].state = STATE_NEW;
    process_table[idx].priority = priority;
    process_table[idx].memory_start = idx * (MAX_MEMORY / MAX_PROCESSES);
    process_table[idx].memory_size = mem_size;
    process_table[idx].cpu_time = 0;
    process_table[idx].creation_time = (int)time(NULL);
    process_table[idx].wait_time = 0;
    process_table[idx].io_wait_count = 0;
    
    // Initialize page table entries for this process
    int pages_needed = (mem_size + 255) / 256;
    for (int i = 0; i < pages_needed; i++) {
        int page_idx = process_table[idx].memory_start / 256 + i;
        if (page_idx < MAX_MEMORY / 256) {
            page_table[page_idx].page_number = i;
            page_table[page_idx].process_pid = pid;
            page_table[page_idx].valid = 1;
        }
    }
    
    num_processes++;
    
    printf("[PROCESS] Created process '%s' (PID: %d, Priority: %d, Memory: %d bytes)\n",
           name, pid, priority, mem_size);
    
    // Add to ready queue
    ready_queue[ready_queue_size++] = idx;
    process_table[idx].state = STATE_READY;
    
    return pid;
}

void terminate_process(int pid) {
    for (int i = 0; i < num_processes; i++) {
        if (process_table[i].pid == pid) {
            process_table[i].state = STATE_TERMINATED;
            printf("[PROCESS] Terminated process '%s' (PID: %d, CPU Time: %d ticks)\n",
                   process_table[i].name, pid, process_table[i].cpu_time);
            return;
        }
    }
    printf("[ERROR] Process PID %d not found\n", pid);
}

void list_processes() {
    printf("\n╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      PROCESS TABLE                                ║\n");
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
    printf("║ PID   │ Name                 │ State      │ Priority │ CPU Ticks  ║\n");
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < num_processes; i++) {
        const char *state_str;
        switch (process_table[i].state) {
            case STATE_READY: state_str = "READY"; break;
            case STATE_RUNNING: state_str = "RUNNING"; break;
            case STATE_WAITING: state_str = "WAITING"; break;
            case STATE_BLOCKED: state_str = "BLOCKED"; break;
            case STATE_TERMINATED: state_str = "TERMINATED"; break;
            default: state_str = "UNKNOWN"; break;
        }
        printf("║ %5d │ %-20s │ %-10s │ %8d │ %10d ║\n",
               process_table[i].pid, process_table[i].name, state_str,
               process_table[i].priority, process_table[i].cpu_time);
    }
    
    printf("╚════════════════════════════════════════════════════════════════════╝\n\n");
}

// ==================== SCHEDULING ====================

int current_running_pid = -1;

void scheduler() {
    if (ready_queue_size == 0) {
        printf("[SCHEDULER] No ready processes\n");
        return;
    }
    
    // Find highest priority process in ready queue
    int best_idx = 0;
    int best_priority = -1;
    
    for (int i = 0; i < ready_queue_size; i++) {
        int idx = ready_queue[i];
        if (process_table[idx].state == STATE_READY &&
            process_table[idx].priority > best_priority) {
            best_priority = process_table[idx].priority;
            best_idx = i;
        }
    }
    
    // Move best process to front and execute
    int exec_idx = ready_queue[best_idx];
    int temp = ready_queue[best_idx];
    ready_queue[best_idx] = ready_queue[0];
    ready_queue[0] = temp;
    
    process_table[exec_idx].state = STATE_RUNNING;
    current_running_pid = process_table[exec_idx].pid;
    
    printf("[SCHEDULER] Running process '%s' (PID: %d, Priority: %d)\n",
           process_table[exec_idx].name,
           process_table[exec_idx].pid,
           process_table[exec_idx].priority);
    
    // Simulate execution
    process_table[exec_idx].cpu_time += TIME_SLICE;
    total_cpu_ticks += TIME_SLICE;
    
    OS_SLEEP(50);  // Simulate execution
    
    if (process_table[exec_idx].state != STATE_TERMINATED) {
        process_table[exec_idx].state = STATE_READY;
        // Move to back of queue (round-robin)
        int temp_pid = ready_queue[0];
        for (int i = 0; i < ready_queue_size - 1; i++) {
            ready_queue[i] = ready_queue[i + 1];
        }
        ready_queue[ready_queue_size - 1] = temp_pid;
    } else {
        // Remove from ready queue
        for (int i = 0; i < ready_queue_size - 1; i++) {
            ready_queue[i] = ready_queue[i + 1];
        }
        ready_queue_size--;
    }
}

void run_scheduler_cycles(int cycles) {
    printf("\n[SCHEDULER] Running %d scheduling cycles...\n", cycles);
    for (int i = 0; i < cycles; i++) {
        scheduler();
        printf("[CYCLE %d/%d]\n", i + 1, cycles);
    }
}

// ==================== SYSTEM CALLS ====================

void log_syscall(int pid, const char *syscall_name) {
    if (syscall_count >= MAX_SYSCALLS) return;
    
    syscall_log[syscall_count].pid = pid;
    strncpy(syscall_log[syscall_count].syscall_name, syscall_name, 31);
    syscall_log[syscall_count].timestamp = time(NULL);
    syscall_count++;
}

void syscall_exit(int pid) {
    log_syscall(pid, "exit");
    terminate_process(pid);
    printf("[SYSCALL] exit() - Process %d terminated\n", pid);
}

void syscall_fork(int pid) {
    log_syscall(pid, "fork");
    printf("[SYSCALL] fork() - Process %d spawned child\n", pid);
    // Simplified fork: just increment priority slightly
    for (int i = 0; i < num_processes; i++) {
        if (process_table[i].pid == pid) {
            create_process("child", process_table[i].priority - 1, 256);
            break;
        }
    }
}

void syscall_read(int pid, const char *filename) {
    log_syscall(pid, "read");
    printf("[SYSCALL] read('%s') - Process %d\n", filename, pid);
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].in_use && strcmp(file_system[i].name, filename) == 0) {
            printf("[SYSCALL] File content: %s\n", file_system[i].content);
            return;
        }
    }
    printf("[SYSCALL] File not found\n");
}

void syscall_write(int pid, const char *filename, const char *content) {
    log_syscall(pid, "write");
    printf("[SYSCALL] write('%s') - Process %d\n", filename, pid);
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].in_use && strcmp(file_system[i].name, filename) == 0) {
            strncpy(file_system[i].content, content, 511);
            printf("[SYSCALL] File updated\n");
            return;
        }
    }
}

void syscall_sleep(int pid, int milliseconds) {
    log_syscall(pid, "sleep");
    printf("[SYSCALL] sleep(%d ms) - Process %d\n", milliseconds, pid);
    for (int i = 0; i < num_processes; i++) {
        if (process_table[i].pid == pid) {
            process_table[i].state = STATE_WAITING;
            printf("[SYSCALL] Process %d blocking...\n", pid);
            OS_SLEEP(milliseconds);
            process_table[i].state = STATE_READY;
            printf("[SYSCALL] Process %d unblocked\n", pid);
            return;
        }
    }
}

// ==================== FILE SYSTEM ====================

void create_file(const char *name, const char *content, int owner_pid) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_system[i].in_use) {
            strncpy(file_system[i].name, name, 63);
            strncpy(file_system[i].content, content, 511);
            file_system[i].in_use = 1;
            file_system[i].size = strlen(content);
            file_system[i].owner_pid = owner_pid;
            file_system[i].created_time = time(NULL);
            printf("[FS] File created: '%s' (%d bytes)\n", name, file_system[i].size);
            return;
        }
    }
    printf("[FS] File system full!\n");
}

void list_files() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                       FILE SYSTEM                             ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Filename             │ Owner PID │ Size │ Created Time        ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    int file_count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].in_use) {
            char time_str[20];
            struct tm *timeinfo = localtime(&file_system[i].created_time);
            strftime(time_str, 20, "%H:%M:%S", timeinfo);
            printf("║ %-20s │ %9d │ %4d │ %s              ║\n",
                   file_system[i].name, file_system[i].owner_pid,
                   file_system[i].size, time_str);
            file_count++;
        }
    }
    
    if (file_count == 0) {
        printf("║ (No files)                                                     ║\n");
    }
    
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
}

// ==================== MEMORY MANAGEMENT ====================

void show_memory_stats() {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║              MEMORY MANAGEMENT STATUS                 ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    
    int used_memory = 0;
    for (int i = 0; i < num_processes; i++) {
        if (process_table[i].state != STATE_TERMINATED) {
            used_memory += process_table[i].memory_size;
        }
    }
    
    int valid_pages = 0;
    for (int i = 0; i < MAX_MEMORY / 256; i++) {
        if (page_table[i].valid) valid_pages++;
    }
    
    printf("║ Total Memory:        %d bytes                        ║\n", MAX_MEMORY);
    printf("║ Used Memory:         %d bytes                        ║\n", used_memory);
    printf("║ Free Memory:         %d bytes                        ║\n", MAX_MEMORY - used_memory);
    printf("║ Memory Usage:        %.1f%%                          ║\n", (100.0 * used_memory) / MAX_MEMORY);
    printf("║ Valid Pages:         %d / %d                          ║\n", valid_pages, MAX_MEMORY / 256);
    printf("║ Total CPU Ticks:     %d                              ║\n", total_cpu_ticks);
    printf("╚════════════════════════════════════════════════════════╝\n\n");
}

// ==================== SYSTEM CALL LOG ====================

void show_syscall_log() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    SYSTEM CALL LOG                            ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ PID   │ Syscall              │ Time                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < syscall_count && i < 20; i++) {
        char time_str[20];
        struct tm *timeinfo = localtime(&syscall_log[i].timestamp);
        strftime(time_str, 20, "%H:%M:%S", timeinfo);
        printf("║ %5d │ %-20s │ %s                       ║\n",
               syscall_log[i].pid, syscall_log[i].syscall_name, time_str);
    }
    
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
}

// ==================== INTERACTIVE SHELL ====================

void show_help() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    CHISCOCONNECTION OS SHELL                   ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Process Commands:                                              ║\n");
    printf("║   create <name> <priority>    - Create new process            ║\n");
    printf("║   terminate <pid>             - Terminate process             ║\n");
    printf("║   ps                          - List all processes            ║\n");
    printf("║   schedule <cycles>           - Run scheduler cycles          ║\n");
    printf("║                                                                ║\n");
    printf("║ File System Commands:                                          ║\n");
    printf("║   touch <filename> <content>  - Create file                   ║\n");
    printf("║   ls                          - List all files                ║\n");
    printf("║   read <filename>             - Read file                     ║\n");
    printf("║                                                                ║\n");
    printf("║ System Commands:                                               ║\n");
    printf("║   memstat                     - Show memory statistics        ║\n");
    printf("║   syslog                      - Show system call log          ║\n");
    printf("║   help                        - Show this help menu           ║\n");
    printf("║   exit                        - Shutdown kernel               ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
}

void shell() {
    show_help();
    char command[256];
    char arg1[64], arg2[64], arg3[64];
    
    while (1) {
        printf("chiscoconnection-os> ");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;
        
        if (strcmp(command, "exit") == 0) {
            printf("\n[KERNEL] Shutting down Chiscoconnection OS...\n");
            show_memory_stats();
            printf("[KERNEL] Goodbye!\n\n");
            break;
        }
        else if (sscanf(command, "create %s %s", arg1, arg2) == 2) {
            int priority = atoi(arg2);
            create_process(arg1, priority, 256);
        }
        else if (sscanf(command, "terminate %s", arg1) == 1) {
            int pid = atoi(arg1);
            terminate_process(pid);
        }
        else if (strcmp(command, "ps") == 0) {
            list_processes();
        }
        else if (sscanf(command, "schedule %s", arg1) == 1) {
            int cycles = atoi(arg1);
            run_scheduler_cycles(cycles);
        }
        else if (sscanf(command, "touch %s %s", arg1, arg2) == 2) {
            create_file(arg1, arg2, current_running_pid > 0 ? current_running_pid : 0);
        }
        else if (strcmp(command, "ls") == 0) {
            list_files();
        }
        else if (sscanf(command, "read %s", arg1) == 1) {
            syscall_read(current_running_pid, arg1);
        }
        else if (strcmp(command, "memstat") == 0) {
            show_memory_stats();
        }
        else if (strcmp(command, "syslog") == 0) {
            show_syscall_log();
        }
        else if (strcmp(command, "help") == 0) {
            show_help();
        }
        else if (strlen(command) > 0) {
            printf("[SHELL] Unknown command: %s\n", command);
        }
    }
}

// ==================== MAIN ====================

int main() {
    init_kernel();
    
    // Create system processes
    create_process("Init", 10, 512);
    create_process("Kernel", 9, 256);
    create_process("NetworkManager", 7, 512);
    create_process("FileServer", 8, 384);
    
    // Create some demo files
    create_file("system.cfg", "kernel_version=2.0\nplatform=cross", 0);
    create_file("boot.log", "System started successfully", 0);
    
    // Enter shell
    shell();
    
    printf("\n[KERNEL] System halted.\n\n");
    return 0;
}
