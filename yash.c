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

void check_for_redirection(char** tokens, int *retVal)
{
    int index = 0;
    while (tokens[index] != NULL)
    {
        if (STR_EQUAL(tokens[index], ">") == true)
        {
            retVal[REDIR_DIR] = -1;
        }
        else if (STR_EQUAL(tokens[index], "<") == true)
        {
            retVal[REDIR_DIR] = 0;
        }
        else if (STR_EQUAL(tokens[index], "2>") == true)
        {
            retVal[REDIR_DIR] = 1;
        }
        else
        {
            index++;
            continue;
        }
        retVal[REDIR_INDEX] = index;
        return;
    }
}

int main()
{
    int pfd[2];
    int cpid;
    char* cmdline;
    char **tokens;
    int redirection[2];
    while(cmdline = readline("# "))
    {
        tokens = parse(cmdline, " ");
        check_for_redirection(tokens, redirection);
        cpid = fork();
        if (cpid == 0) 
        {   
            if (redirection[REDIR_INDEX] > 0)
            {
                int redir_status;
                int oldfd;
                int newfd;
                char* filename = tokens[redirection[REDIR_INDEX] + 1]; // get next token after redirect operator
                
                // Check if a filename is given for redirection
                if (filename == NULL)
                {
                    exit(-1);
                }

                if (redirection[REDIR_DIR] == -1) // redirect output
                {
                    oldfd = STDOUT_FILENO;
                }
                else if (redirection[REDIR_DIR] == 0) // redirect input
                {
                    oldfd = STDIN_FILENO;
                }
                else if (redirection[REDIR_DIR] == 1) //redirect error
                {
                    oldfd = STDERR_FILENO;
                }
                newfd = open(filename, O_CREAT|O_RDWR, 0666);
                redir_status = dup2(newfd, oldfd);
                if (redir_status == -1)
                {
                    printf("Bad redirect CHANGE THIS PRINT STATEMENT");
                }
                // stop arguments at redirect symbol
                tokens[redirection[REDIR_INDEX]] = NULL;
            }
            execvp(tokens[0], tokens);
        }
        wait(NULL);
    }
}