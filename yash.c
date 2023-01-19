#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#define STR_EQUAL(x1, x2)(strcmp(x1, x2) == 0)
#define REDIR_INDEX 0
#define REDIR_DIR 1
#define min(a, b)(a <= b ? a : b)

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

int check_for_pipe(char** tokens)
{
    int index = 0;
    while (tokens[index] != NULL)
    {
        if (STR_EQUAL(tokens[index], "|") == true)
        {
            return index;
        }
    }
    return -1;
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
    return -1;
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
            O_CREAT|O_RDWR, 
            0666
        );
        dup2(fd, 0);
    }
    redirection_index = check_for_token(tokens, ">");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_CREAT|O_RDWR, 
            0666
        );
        dup2(fd, 1);
    }
    redirection_index = check_for_token(tokens, "2>");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_CREAT|O_RDWR, 
            0666
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
    return -1;
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

int handle_child(char **tokens, bool pipe, int pfd_open, int pfd_close, int replaced_fd)
{
    int cpid = fork();
    if (cpid == 0) 
    {   
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

        execvp(tokens[0], tokens);
    }
    return cpid;
}

int main()
{
    int right_cpid, left_cpid, status;
    char* cmdline;
    char **tokens;
    char **p1_tokens;
    char **p2_tokens;
    int pipe_index;
    int pfd[2];
    while(cmdline = readline("# "))
    {
        tokens = parse(cmdline, " ");
        pipe_index = check_for_token(tokens, "|");
        if (pipe_index > -1)
        {
            pipe(pfd);
            p1_tokens = set_child_tokens(tokens, 0, pipe_index);
            p2_tokens = set_child_tokens(tokens, pipe_index + 1, get_number_tokens(tokens) - 1);
            left_cpid = handle_child(p1_tokens, true, pfd[1], pfd[0], 1);
            right_cpid = handle_child(p2_tokens, true, pfd[0], pfd[1], 0);
        }
        else
        {
           right_cpid = handle_child(tokens, false, -1, -1, -1);
        }
        close(pfd[0]);
        close(pfd[1]);
        waitpid(right_cpid, &status, WUNTRACED);
    }

    if (tokens != NULL)
    {
        free(tokens);
    }
    if (p1_tokens != NULL)
    {
        free(p1_tokens);
    }
    if(p2_tokens != NULL)
    {
        free(p2_tokens);
    }
}