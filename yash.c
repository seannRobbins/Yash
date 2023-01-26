#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>

#define STR_EQUAL(x1, x2)(strcmp(x1, x2) == 0)
#define NONE -1
#define MAX_LINE_LENGTH 2001
enum job_status
{
    Running,
    Stopped
};

char *job_status_strings[] = {"Running", "Stopped"};

struct Process
{
    int pid;
};

struct Job
{
    int pgid;
    int status;
    char* cmd;
    struct Process p1;
    struct Process p2;
};

struct Job *jobs = NULL;
static int job_count = 0;
static bool fg_flag = false;
#define top_stack_job()(job_count - 1)

char** parse(char *cmdline, char* delim)
{
    char *string_to_parse, *token;
    char *saveptr;
    int index;
    char** tokens = NULL;

    for (index = 0, string_to_parse = cmdline; ; index++, string_to_parse = NULL) {
        tokens = realloc(tokens, (index + 1) * sizeof(char*));
        token = strtok_r(string_to_parse, delim, &saveptr);
        tokens[index] = token;
        if (token == NULL)
        {
            break;
        }
    }
    return tokens;
}

char* get_file_name(char** tokens, int index)
{
    return tokens[index + 1];
}

int check_for_token(char** tokens, char* symbol)
{
    int index = 0;
    while (tokens[index] != NULL)
    {
        if (STR_EQUAL(tokens[index], symbol) == true)
        {
            return index;
        }
        index++;
    }
    return NONE;
}

void set_redirection(char** tokens)
{
    int fd;
    int redirection_index;

    redirection_index = check_for_token(tokens, "<");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_RDWR, 
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
        );
        // File does not exist
        if (fd == -1)
        {
            exit(-1);
        }
        dup2(fd, 0);
    }
    redirection_index = check_for_token(tokens, ">");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_CREAT|O_RDWR, 
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
        );
        dup2(fd, 1);
    }
    redirection_index = check_for_token(tokens, "2>");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_CREAT|O_RDWR, 
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
        );
        dup2(fd, 2);
    }
}

int find_smallest_redirection_index(char** tokens)
{
    int index = 0;
    while (tokens[index] != NULL)
    {
        if (STR_EQUAL(tokens[index], ">") || STR_EQUAL(tokens[index], "<") || STR_EQUAL(tokens[index], "2>"))
        {
            return index;
        }
        index++;
    }
    return NONE;
}

int get_number_tokens(char **tokens)
{
    int index = 0;
    while (tokens[index] != NULL)
    {
        index++;
    }
    return index + 1;
}

char** set_child_tokens(char  **tokens, int start_index, int end_index)
{
    char **child_tokens = NULL;
    int child_index = 0;
    for (int index = start_index; index < end_index; child_index++, index++)
    {
        child_tokens = realloc(child_tokens, (child_index + 1) * sizeof(char*));
        child_tokens[child_index] = tokens[index];
    }
    child_tokens = realloc(child_tokens, (child_index + 1) * sizeof(char*));
    child_tokens[child_index] = NULL;
    return child_tokens;
}

void set_job_parameters(int job_index, int p1id, int p2id, int pgid, int status, char* cmd)
{
    jobs[job_index].p1.pid = p1id;
    jobs[job_index].p2.pid = p2id;
    jobs[job_index].pgid = pgid;
    jobs[job_index].status = status;
    if (job_index == top_stack_job())
    {
        jobs[job_index].cmd = malloc(MAX_LINE_LENGTH * sizeof( char ));
        strcpy(jobs[job_index].cmd, cmd);
    }
    else
    {
        jobs[job_index].cmd = cmd;
    }
}

void delete_dead_job(int index)
{
    if (jobs[index].cmd != NULL)
    {
        free(jobs[index].cmd);
    }
    for (int i = index; i < job_count - 1; i++)
    {
        set_job_parameters(
            index,
            jobs[i + 1].p1.pid,
            jobs[i + 1].p2.pid,
            jobs[i + 1].pgid,
            jobs[i + 1].status,
            jobs[i + 1].cmd
        );
    }
    job_count--;
}

void handle_waitpid_status(int retcpid, int status, int index)
{
    int sig = WTERMSIG(status);
    if (retcpid > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            delete_dead_job(index);
        }
        else if (WIFSTOPPED(status))
        {
            jobs[index].status = Stopped;
        }
    }
    else if (retcpid == -1)
    {
        printf("error occured");
    }
}

void sigchild_handler(int sig)
{
    int index;
    int retval;
    int status; 

    if (fg_flag)
    {
        return;
    }

    for (index = 0; index < job_count; index++)
    {
        retval = waitpid(jobs[index].p2.pid, &status, WNOHANG | WUNTRACED); 
        handle_waitpid_status(retval, status, index);
    }
}

void init_parent_signals()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, &sigchild_handler);
}

void init_child_signals()
{
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

int handle_child(char **tokens, bool pipe, int pfd_open, int pfd_close, int replaced_fd, int pgid)
{
    int cpid = fork();
    if (cpid == 0) 
    {   
        int exec_code;
        init_child_signals();
        setpgid(0, pgid);
        if (pipe)
        {
            dup2(pfd_open, replaced_fd);
            close(pfd_close);
        }

        int redirection_index = find_smallest_redirection_index(tokens);
        if (redirection_index > 0)
        {
            set_redirection(tokens);
            tokens[redirection_index] = NULL;
        }

        exec_code = execvp(tokens[0], tokens);
        if (exec_code < 0)
        {
            exit(255);
        }
    }
    return cpid;
}

void handle_jobs_cmd()
{
    int i;
    for (i = 0; i < job_count - 1; i++)
    {
        printf("[%d] - %s       %s\n", i + 1, job_status_strings[jobs[i].status], jobs[i].cmd);
    }
    printf("[%d] + %s       %s\n", i + 1, job_status_strings[jobs[i].status], jobs[i].cmd);
}

void handle_bg_cmd()
{
    int index;
    if (job_count > 0)
    {
        for (index = job_count - 1; index >= 0; index--)
        {
            if (jobs[index].status == Stopped)
            {
                break;
            }
        }
        kill(-jobs[index].pgid, SIGCONT);
        jobs[index].status = Running;
    }
}

void handle_fg_cmd()
{
    int status, retcpid;
    if (job_count > 0)
    {
        fg_flag = true;
        tcsetpgrp(STDIN_FILENO, jobs[top_stack_job()].pgid);
        kill(-jobs[top_stack_job()].pgid, SIGCONT);
        jobs[top_stack_job()].status = Running;

        // Wait for Yash to get terminal control back
        retcpid = waitpid(jobs[top_stack_job()].p2.pid, &status, WUNTRACED);
        fg_flag = false;
        tcsetpgrp(STDIN_FILENO, getpgid(getpid()));
        handle_waitpid_status(retcpid, status, top_stack_job());
        jobs[top_stack_job()].status = Stopped;
    }
}

int main()
{
    int right_cpid, left_cpid, retcpid, status;
    char *cmdline;
    char *cmdlinecopy = malloc(MAX_LINE_LENGTH * sizeof( char ));
    char **tokens = NULL;
    char **p1_tokens = NULL;
    char **p2_tokens = NULL;
    int pipe_index, amp_index;
    int pfd[2];
    bool wait_flag = false;
    init_parent_signals();
    while(cmdline = readline("# "))
    {
        if (cmdline[0] == 0)
        {
            continue;
        }

        cmdlinecopy = strdup(cmdline);
        tokens = parse(cmdline, " ");

        if (check_for_token(tokens, "jobs") != NONE)
        {
            if (get_number_tokens(tokens) == 2)
            {
                handle_jobs_cmd();
            }
        }
        else if (check_for_token(tokens, "bg") != NONE)
        {
            if (get_number_tokens(tokens) == 2)
            {
                handle_bg_cmd();
            }
        }
        else if (check_for_token(tokens, "fg") != NONE)
        {
            if (get_number_tokens(tokens) == 2)
            {
                handle_fg_cmd();
            }
        }
        else
        {
            amp_index = check_for_token(tokens, "&");
            if (amp_index != NONE)
            {
                tokens[amp_index] = NULL;
            }
            pipe_index = check_for_token(tokens, "|");
            job_count++;
            jobs = realloc(jobs, job_count * sizeof( struct Job ));
            if (pipe_index > -1)
            {
                pipe(pfd);
                p1_tokens = set_child_tokens(tokens, 0, pipe_index);
                p2_tokens = set_child_tokens(tokens, pipe_index + 1, get_number_tokens(tokens) - 1);
                left_cpid = handle_child(p1_tokens, true, pfd[1], pfd[0], STDOUT_FILENO, 0);
                right_cpid = handle_child(p2_tokens, true, pfd[0], pfd[1], STDIN_FILENO, left_cpid);
                close(pfd[0]);
                close(pfd[1]);
            }
            else
            {
                left_cpid = handle_child(tokens, false, NONE, NONE, NONE, 0);
                right_cpid = left_cpid;
            }
            set_job_parameters(top_stack_job(), left_cpid, right_cpid, left_cpid, Running, cmdlinecopy);
        
            if (amp_index == NONE)
            {
                fg_flag = true;
                tcsetpgrp(STDIN_FILENO, jobs[top_stack_job()].pgid);
                retcpid = waitpid(jobs[top_stack_job()].p2.pid, &status, WUNTRACED);
                tcsetpgrp(STDIN_FILENO, getpgid(getpid()));
                handle_waitpid_status(retcpid, status, top_stack_job());
                fg_flag = false;
            }
        }
    }
    free(cmdlinecopy);

    if (tokens != NULL)
    {
        free(tokens);
    }
    if (p1_tokens != NULL)
    {
        free(p1_tokens);
    }
    if (p2_tokens != NULL)
    {
        free(p2_tokens);
    }
    if (jobs != NULL)
    {
        for (int i = 0; i < job_count; i++)
        {
            if (jobs[i].cmd != NULL)
            {
                free(jobs[i].cmd);
            }
        }
        free(jobs);
    }
}