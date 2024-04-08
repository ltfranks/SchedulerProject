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

int main(int argc, char *argv[]){
    int quantum;
    quantum = atoi(argv[1]);
    total_processes = parse_commands((char **) argv, processes, argc);
    
    execute_and_schedule();

    return 0;
}

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
    }
}

void sigchld_handler(int signum){
    int status;
    /* wait for any child process to change then notify parent of what changed */
    pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
    
    while (pid > 0) {
        if(WIFSTOPPED(status) || WIFEXITED(status) || WIFSIGNALED(status)){
            schedule_next_process();
        }
    }
}

void schedule_next_process(){
    /* go through process index and wrap around if need be */
    current_process = (current_process + 1) % total_processes;
    /* if process is already complete, skip and look at next process */
    while(!processes[current_process].active){
        current_process = (current_process+1) % total_processes;
    }

    /* if process has not started (Pid == 0), fork(), exec() */
    /* else, continue same process -> SIGCONT */
    if(processes[current_process].pid == 0){
        pid_t pid = fork();
        if (pid == 0){
            execvp(processes[current_process].argv[0], processes[current_process].argv);
            exit(EXIT_FAILURE);
        } else {
            processes[current_process].pid = pid;
        }
    } else {
        kill(processes[current_process].pid, SIGCONT);
    }

    /* sets quantum for next process */
    alarm(quantum);
}

void execute_and_schedule(){
    int i;
    for(i = 0; i < total_processes; i++){
        /* if process is done, skip to next process */
        if(!processes[i].active) continue;
    
        current_process = i;
        pid_t pid = fork();
        /* child -> execute new process */
        if(pid == 0){
            execvp(processes[i].argv[0], processes[i].argv);
            exit(EXIT_FAILURE);
        } else {
            processes[i].pid = pid; /* save child Pid in parent */
            alarm(quantum);
            pause();
        }
    }
}