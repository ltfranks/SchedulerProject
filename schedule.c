#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

#define MAX_PROCESSES 100
#define MAX_ARGUMENTS 10

typedef struct{
    pid_t pid; 
    char* argv[MAX_ARGUMENTS + 2]; /* 2 for the program name and NULL term */
    int active; /* status of process */
} process;

process processes[MAX_PROCESSES];
int current_process = -1; /* Index of current process */
int total_processes = 0;
int quantum;

/* function declarations */
int parse_commands(char *argv[], process processes[MAX_PROCESSES], int argc);
void sigalrm_handler(int signum);
void sigchld_handler(int signum);
void execute_and_schedule();
void schedule_next_process();

int parse_commands(char *argv[], process processes[MAX_PROCESSES], int argc){
    int process_count = 0;
    /* 
    argv[0] = program
    argv[1] = quantum
    Start reading input processes @ argv[2]. 
    */
   int i;
    for (i = 2; i < argc && process_count < MAX_PROCESSES; i++){
        int arg_count = 0;
        /*  */
        while (argv[i] != NULL && strcmp(argv[i], ":") != 0 && arg_count < MAX_ARGUMENTS){
            processes[process_count].argv[arg_count++] = argv[i++];
        }
        /* putting NULL at end of args */
        processes[process_count].argv[arg_count] = NULL;
        processes[process_count].active = 1;
        process_count++;
        
        /* next arg */
        if(i < argc && strcmp(argv[i], ":") == 0) i++;
    }
    return process_count;
}

void sigalrm_handler(int signum){
    if (current_process != -1){
        /* stop current process */
        kill(processes[current_process].pid, SIGSTOP);
        schedule_next_process();
    }
}

void sigchld_handler(int signum){
    int status;
    /* wait for any child process to change then notify parent of what changed */
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if(WIFEXITED(status) || WIFSIGNALED(status)){
            /* mark process as DONE if it exited */
            int i;
            for(i = 0; i < total_processes; i++){
                if (processes[i].pid == pid){
                    processes[i].active = 0;
                    break;
                }
                
            }
        }
    }
}

void schedule_next_process(){
    int next_process = (current_process + 1) % total_processes;
    /* no processes case */
    int all_inactive = 1;
    int initial_process = next_process;

    do {
        if (processes[next_process].active) {
            /* there is still processes to behold */
            all_inactive = 0;
            /* if process has NOT started (pid == 0), then fork and exec. else continue; */
            if (processes[next_process].pid == 0) {
                pid_t pid = fork();
                if(pid == 0){
                    printf("before!\n");
                    printf("'%s' ", processes[next_process].argv[0]);
                    printf("'%s' ", processes[next_process].argv[1]);
                    printf("'%s' ", processes[next_process].argv[2]);
                    printf("\n");

                    execvp(processes[next_process].argv[0], processes[next_process].argv);
                    
                    printf("after!\n");
                    exit(EXIT_FAILURE); /* shouldn't be reached */
                } else {
                    processes[next_process].pid = pid;
                }
                /* process HAS started so continue */
            } else {
                /* kill() is used to send CONTINUE signal */
                kill(processes[next_process].pid, SIGCONT);
            }

            current_process = next_process;
/* problem: only sets alarm one time. So stuck during 1st process */            
            alarm(quantum);
            break;
        }
        /* next_process + 1 */
        next_process = (next_process + 1) % total_processes;
        /* if there is NO next process */
        
    } while (next_process != initial_process);

    if (all_inactive){
        exit(EXIT_SUCCESS);
    }
}

void execute_and_schedule() {
    // Initially trigger the scheduling of the first process
    schedule_next_process();

    while (1) {
        pause(); // Wait for signals (SIGALRM or SIGCHLD)
        
        // Check if all processes are inactive to potentially break the loop
        int all_inactive = 1;
        int i;
        for (i = 0; i < total_processes; i++) {
            
            /* printf("yo!\n"); */
            
            if (processes[i].active) {
                all_inactive = 0;
                break;
            }
        }

        // If all processes are inactive, break the while loop and end scheduling
        if (all_inactive) {
            printf("All processes have completed.\n");
            break;
        }
    }
}


int main(int argc, char *argv[]){
    
    quantum = atoi(argv[1]);
    total_processes = parse_commands((char **) argv, processes, argc);
    
    signal(SIGALRM, sigalrm_handler);
    signal(SIGCHLD, sigchld_handler);
    
    execute_and_schedule();

    return 0;
}