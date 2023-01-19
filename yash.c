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

int check_for_redirection(char** tokens, char* direction)
{
    int index = 0;
    while (tokens[index] != NULL)
    {
        if (STR_EQUAL(tokens[index], direction) == true)
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

    redirection_index = check_for_redirection(tokens, "<");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_CREAT|O_RDWR, 
            0666
        );
        dup2(fd, 0);
    }
    redirection_index = check_for_redirection(tokens, ">");
    if (redirection_index > 0)
    {
        fd = open(
            get_file_name(tokens, redirection_index), 
            O_CREAT|O_RDWR, 
            0666
        );
        dup2(fd, 1);
    }
    redirection_index = check_for_redirection(tokens, "2>");
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

int main()
{
    int pfd[2];
    int cpid;
    char* cmdline;
    char **tokens;
    while(cmdline = readline("# "))
    {
        tokens = parse(cmdline, " ");
        cpid = fork();
        if (cpid == 0) 
        {   
            int redirection_index = find_smallest_redirection_index(tokens);
            if (redirection_index > 0)
            {
                set_redirection(tokens);
                tokens[redirection_index] = NULL;
            }
            execvp(tokens[0], tokens);
        }
        wait(NULL);
    }
}