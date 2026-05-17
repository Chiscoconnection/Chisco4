// chiscoconnection_os_fixed.c
// A toy operating system simulation in C - CORRECTED VERSION
// Fixes: buffer overflow, scheduler logic, memory management, cross-platform support

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Cross-platform sleep
#ifdef _WIN32
    #include <windows.h>
    #define OS_SLEEP(ms) Sleep(ms)
    #define CLEAR_SCREEN "cls"
#else
    #include <unistd.h>
    #define OS_SLEEP(ms) usleep(ms * 1000)
    #define CLEAR_SCREEN "clear"
#endif

#define MAX_PROCESSES 10
#define MAX_MEMORY 1024
#define MAX_FILES 10

// Process structure
typedef struct {
    int pid;
    char name[32];
    int state; // 0 = ready, 1 = running, 2 = waiting, -1 = terminated
    int memory_start;
    int memory_size;
} Process;

// File structure
typedef struct {
    char name[32];
    char content[128];
    int in_use;
} File;

// Globals
Process process_table[MAX_PROCESSES];
File file_system[MAX_FILES];
int process_count = 0;
int current_pid = 1000;
char memory[MAX_MEMORY];

// Initialize file system
void init_filesystem(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        file_system[i].in_use = 0;
        memset(file_system[i].name, 0, 32);
        memset(file_system[i].content, 0, 128);
    }
}

// Initialize process table
void init_process_table(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = -1;
        process_table[i].state = -1;
        process_table[i].memory_size = 0;
    }
}

// Create a new process
void create_process(const char *name, int mem_size) {
    if (process_count >= MAX_PROCESSES) {
        printf("[ERROR] Process table full!\n");
        return;
    }
    if (mem_size > MAX_MEMORY / MAX_PROCESSES) {
        printf("[ERROR] Not enough memory! (max per process: %d bytes)\n", MAX_MEMORY / MAX_PROCESSES);
        return;
    }
    
    int idx = process_count;
    process_table[idx].pid = current_pid;
    strncpy(process_table[idx].name, name, 31);
    process_table[idx].name[31] = '\0';
    process_table[idx].state = 0; // ready
    process_table[idx].memory_start = idx * (MAX_MEMORY / MAX_PROCESSES);
    process_table[idx].memory_size = mem_size;
    
    printf("[PROCESS] Created process '%s' (PID: %d, Memory: %d bytes)\n",
           name, current_pid, mem_size);
    
    current_pid++;
    process_count++;
}

// Find process by PID (safe lookup)
int find_process_index(int pid) {
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].pid == pid) {
            return i;
        }
    }
    return -1; // Not found
}

// Scheduler
void scheduler(void) {
    printf("\n[SCHEDULER] Running scheduler...\n");
    int found = 0;
    
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state == 0) { // READY
            process_table[i].state = 1; // Change to RUNNING
            printf("[SCHEDULER] Running process '%s' (PID %d)\n", 
                   process_table[i].name, process_table[i].pid);
            
            OS_SLEEP(100); // Simulate execution
            
            if (process_table[i].state == 1) {
                process_table[i].state = 0; // Back to READY for round-robin
            }
            found = 1;
        }
    }
    
    if (!found) {
        printf("[SCHEDULER] No ready processes\n");
    }
}

// System call handler
void syscall_handler(int pid, const char *call) {
    int idx = find_process_index(pid);
    
    if (idx == -1) {
        printf("[SYSCALL ERROR] Process %d not found\n", pid);
        return;
    }
    
    printf("[SYSCALL] Process %d (%s) invoked: %s\n", 
           pid, process_table[idx].name, call);
    
    if (strcmp(call, "exit") == 0) {
        printf("[SYSCALL] Process %d terminated.\n", pid);
        process_table[idx].state = -1; // TERMINATED
    } else if (strcmp(call, "sleep") == 0) {
        printf("[SYSCALL] Process %d sleeping...\n", pid);
        process_table[idx].state = 2; // WAITING
    }
}

// List all processes
void list_processes(void) {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║            PROCESS TABLE (Active Processes)            ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ PID  │ Name               │ State     │ Memory         ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    
    int count = 0;
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].pid != -1) {
            const char *state_str;
            switch (process_table[i].state) {
                case 0: state_str = "READY"; break;
                case 1: state_str = "RUNNING"; break;
                case 2: state_str = "WAITING"; break;
                case -1: state_str = "TERMINATED"; break;
                default: state_str = "UNKNOWN"; break;
            }
            
            printf("║ %4d │ %-18s │ %-9s │ %d bytes        ║\n",
                   process_table[i].pid,
                   process_table[i].name,
                   state_str,
                   process_table[i].memory_size);
            count++;
        }
    }
    
    if (count == 0) {
        printf("║ (No active processes)                                  ║\n");
    }
    
    printf("╚════════════════════════════════════════════════════════╝\n\n");
}

// File system functions
void create_file(const char *name, const char *content) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_system[i].in_use) {
            strncpy(file_system[i].name, name, 31);
            file_system[i].name[31] = '\0';
            strncpy(file_system[i].content, content, 127);
            file_system[i].content[127] = '\0';
            file_system[i].in_use = 1;
            printf("[FS] File '%s' created (%zu bytes)\n", name, strlen(content));
            return;
        }
    }
    printf("[FS ERROR] File system full!\n");
}

void read_file(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].in_use && strcmp(file_system[i].name, name) == 0) {
            printf("[FS] File '%s' content:\n", name);
            printf("--- START ---\n%s\n--- END ---\n", file_system[i].content);
            return;
        }
    }
    printf("[FS ERROR] File '%s' not found.\n", name);
}

// List all files
void list_files(void) {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║                  FILE SYSTEM                           ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ Filename             │ Size                           ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (file_system[i].in_use) {
            printf("║ %-20s │ %zu bytes                     ║\n",
                   file_system[i].name,
                   strlen(file_system[i].content));
            count++;
        }
    }
    
    if (count == 0) {
        printf("║ (No files)                                             ║\n");
    }
    
    printf("╚════════════════════════════════════════════════════════╝\n\n");
}

void show_help(void) {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║          CHISCOCONNECTION OS - COMMAND HELP            ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ create <name> <size>  - Create a new process          ║\n");
    printf("║ ps                    - List all processes            ║\n");
    printf("║ schedule              - Run scheduler once            ║\n");
    printf("║ syscall <pid> <call>  - Invoke system call            ║\n");
    printf("║ file <name> <content> - Create a file                 ║\n");
    printf("║ read <filename>       - Read file contents            ║\n");
    printf("║ ls                    - List all files                ║\n");
    printf("║ help                  - Show this help                ║\n");
    printf("║ exit                  - Shutdown OS                   ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
}

// Shell loop
void shell(void) {
    char command[256];
    char arg1[64], arg2[128], arg3[64];
    
    show_help();
    
    while (1) {
        printf("chiscoconnectionOS> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\n")] = 0; // strip newline
        
        if (strlen(command) == 0) continue;
        
        if (strcmp(command, "exit") == 0) {
            printf("\n[KERNEL] Shutting down Chiscoconnection OS...\n");
            break;
        } 
        else if (sscanf(command, "create %63s %63s", arg1, arg3) == 2) {
            int size = atoi(arg3);
            create_process(arg1, size);
        } 
        else if (strcmp(command, "ps") == 0) {
            list_processes();
        } 
        else if (strcmp(command, "schedule") == 0) {
            scheduler();
        } 
        else if (sscanf(command, "syscall %63s %63s", arg1, arg2) == 2) {
            int pid = atoi(arg1);
            syscall_handler(pid, arg2);
        }
        else if (sscanf(command, "file %63s %127[^\n]", arg1, arg2) == 2) {
            create_file(arg1, arg2);
        } 
        else if (sscanf(command, "read %63s", arg1) == 1) {
            read_file(arg1);
        } 
        else if (strcmp(command, "ls") == 0) {
            list_files();
        } 
        else if (strcmp(command, "help") == 0) {
            show_help();
        } 
        else {
            printf("[SHELL ERROR] Unknown command: '%s' (type 'help' for commands)\n", command);
        }
    }
}

int main(void) {
    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║  Chiscoconnection OS Kernel - FIXED VERSION            ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");

    // Initialize
    init_process_table();
    init_filesystem();
    
    // Create initial system processes
    create_process("Init", 128);
    create_process("NetworkManager", 256);
    create_process("Shell", 128);
    
    // Run scheduler once
    scheduler();
    
    // Enter interactive shell
    shell();

    printf("[KERNEL] System halted.\n\n");
    return 0;
}
