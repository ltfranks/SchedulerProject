#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

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
void setTimer(int milliseconds);

int parse_commands(char *argv[], process processes[MAX_PROCESSES], int argc){
    int process_count = 0;
   int i;
    for (i = 2; i < argc && process_count < MAX_PROCESSES; i++){
        if(strcmp(argv[i], ":") == 0){
            i++;
            continue;
        }
        
        /* adding ./ to the beginning of two */
        char *command_with_path = malloc(strlen(argv[i]) + 3);
        strcpy(command_with_path, "./");
        strcat(command_with_path, argv[i]);

        processes[process_count].argv[0] = command_with_path;

        i++;
        int arg_count = 1;
        while (i < argc && strcmp(argv[i], ":") != 0 && arg_count < MAX_ARGUMENTS){
            processes[process_count].argv[arg_count++] = argv[i++];
        }
        /* putting NULL at end of args */
        processes[process_count].argv[arg_count] = NULL;
        processes[process_count].active = 1;
        process_count++;
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
            /* mark process as DONE (active = 0) if it exited */
            int i;
            for(i = 0; i < total_processes; i++){
                if (processes[i].pid == pid){
                    processes[i].active = 0;
                    break;
                }
            }
            setTimer(0);
            schedule_next_process();
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

                    execvp(processes[next_process].argv[0], processes[next_process].argv);
                    
                    perror("execvp failed");
                    exit(EXIT_FAILURE); /* shouldn't be reached */
                } else {
                    processes[next_process].pid = pid;
                }
                /* process HAS started so continue */
            } else {
                /* kill() is used to send CONTINUE signal */
                kill(processes[next_process].pid, SIGCONT);
            }
            /* Alarm is being set before process starts? */
            current_process = next_process;
            setTimer(quantum);
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

void setTimer(int milliseconds){
    struct itimerval timer = {0};
    timer.it_value.tv_sec = milliseconds / 1000;
    timer.it_value.tv_usec = (milliseconds % 1000) * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}

int main(int argc, char *argv[]){
    
    quantum = atoi(argv[1]);
    total_processes = parse_commands((char **) argv, processes, argc);
    
    signal(SIGALRM, sigalrm_handler);
    signal(SIGCHLD, sigchld_handler);
    
    execute_and_schedule();

    return 0;
}