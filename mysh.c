#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#define TKN_BUFSIZE 64
#define TKN_DEL " \t\r\n"
#define TKN_KID "<>|&"
#define BUFSIZE 1024
#define TKN_NORMAL 1
#define TKN_REDIR_IN 2
#define TKN_REDIR_OUT 3
#define TKN_PIPE 4
#define TKN_BG 5
#define TKN_EOL 6
#define TKN_EOF 7

sig_atomic_t e_flag = 0;

int mysh_cd(char **args);
int mysh_pwd(char **args);
int mysh_exit(char **args);
int mysh_launch(char **args);

int mysh_input_redirect(char **args);
int mysh_output_redirect(char **args);
int mysh_pipe(char **args);

int gettoken(char token) {
    switch(token) {
        case EOF:   return TKN_EOF;
        case '\n':  return TKN_EOL;
        case '&':   return TKN_BG;
        case '|':   return TKN_PIPE;
        case '<':   return TKN_REDIR_IN;
        case '>':   return TKN_REDIR_OUT;
        default:    return TKN_NORMAL;
    }
}

char *builtin_str[] = {
    "cd",
    "pwd",
    "exit"
};

int (*builtin_func[])(char **) = {
    &mysh_cd,
    &mysh_pwd,
    &mysh_exit
};

int mysh_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int mysh_cd(char **args) {
    if (args[1] == NULL) {
        chdir(getenv("HOME"));
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
    return 1;
}

int mysh_pwd(char **args) {
    char pathname[BUFSIZE];
    memset(pathname, '\0', BUFSIZE);
    getcwd(pathname, BUFSIZE);
    if (args[1] == NULL) {
        fprintf(stdout, "%s\n", pathname);
    } else if ((strcmp(args[1], "-L") == 0) || (strcmp(args[1], "-P") == 0)) {
        fprintf(stdout, "%s\n", pathname);
    } else if (args[1] != NULL) {
        perror("pwd");
    }
    return 1;
}

int mysh_exit(char **args) {
    return 0;
}

int mysh_execute(char **args) {
    int i;

    if (args[0] == NULL) {
        return 1;
    }

    for (i = 0;args[i] != NULL;i ++) {
        if ((strcmp(args[i], "|") == 0)) {
            return mysh_pipe(args);
        } else if (strcmp(args[i], ">") == 0) {
            return mysh_output_redirect(args);
        } else if (strcmp(args[i], "<") == 0) {
            return mysh_input_redirect(args);
        }
    }

    for (i = 0; i < mysh_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return mysh_launch(args);
}

int mysh_launch(char **args) {
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("child");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("child");
    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

char **mysh_split_cpy(char *line) {
    int bufsize = TKN_BUFSIZE, positioncpy = 0;
    char *linecpy, *tokencpy;
    linecpy = (char*)malloc(bufsize * sizeof(char*));
    char **tokenscpy = malloc(bufsize * sizeof(char*));

    strcpy(linecpy, line);
    tokencpy = strtok(linecpy, TKN_DEL);

    while (tokencpy != NULL) {
        tokenscpy[positioncpy++] = tokencpy;
        tokencpy = strtok(NULL, TKN_DEL);
    }
    tokenscpy[positioncpy] = NULL;

    return tokenscpy;
}

char **mysh_split_line(char *line) {
    int bufsize = TKN_BUFSIZE, position = 0, i, flag = 0, flag2 = 0, flag3 = 0, keycount = 0, positioncpy = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char **tokenscpy = malloc(bufsize * sizeof(char*));
    char *token, *token2;
    char kid;

    if (!tokens) {
        fprintf(stderr, "lsh:allocation error\n");
        exit(EXIT_FAILURE);
    }

    tokenscpy = mysh_split_cpy(line);
    for (i = 0;tokenscpy[i] != NULL;i ++) {
        positioncpy ++;
    }
    token = strtok(line, TKN_DEL);

    while (token != NULL) {
        keycount = 0;
        for (i = 0;token[i] != '\0';i ++) {
            if ((gettoken(token[i]) > 1) && (gettoken(token[0]) == 1)) {
                keycount ++;
            }
        }

        for (i = 0;token[i] != '\0';i ++) {
            if ((gettoken(token[i]) > 1) && (gettoken(token[0]) == 1)) {
                flag = 1;
                kid = token[i];
                token2 = strtok(token, TKN_KID);
                while (token2 != NULL) {
                    tokens[position++] = token2;
                    token2 = strtok(NULL, TKN_KID);
                    if (flag2 < keycount) {
                        switch(kid) {
                            case '<':     tokens[position++] = "<"; break;
                            case '>':     tokens[position++] = ">"; break;
                            case '|':     tokens[position++] = "|"; break;
                            case '&':     tokens[position++] = "&"; break;
                        }
                        flag2 ++;
                    }
                }
            } else if ((gettoken(token[0]) > 1) && (strlen(token) > 1)) {
                flag = 1;
                flag3 = 1; 
                kid = token[i];
                token2 = strtok(token, TKN_KID);
                switch(kid) {
                    case '<':     tokens[position++] = "<"; break;
                    case '>':     tokens[position++] = ">"; break;
                    case '|':     tokens[position++] = "|"; break;
                    case '&':     tokens[position++] = "&"; break;
                }
                tokens[position++] = token2;
                token2 = strtok(NULL, TKN_KID);
                break;
            }
        }

        if (!flag) {
            tokens[position++] = token;
        } 
        
        token = strtok(NULL, TKN_DEL);
    }
    
    if (positioncpy > 1) {
        for (i = 0;tokenscpy[i] != NULL;i ++) {
            if ((strchr(tokenscpy[i], '|') != NULL && strcmp(tokenscpy[i], "|") != 0) || (strchr(tokenscpy[i], '>') != NULL && strcmp(tokenscpy[i], ">") != 0) || (strchr(tokenscpy[i], '<') != NULL && strcmp(tokenscpy[i], "<") != 0)) {
                if (tokenscpy[i+1] == NULL) {
                    break;
                } else if (strchr(tokenscpy[i+1], '|') != NULL) {
                    tokens[position++] = "|";
                    tokens[position++] = strtok(tokenscpy[i+1], TKN_KID);
                    break;
                } else if (strchr(tokenscpy[i+1], '>') != NULL) {
                    tokens[position++] = ">";
                    tokens[position++] = strtok(tokenscpy[i+1], TKN_KID);
                    break;
                } else if (strchr(tokenscpy[i+1], '<') != NULL) {
                    tokens[position++] = "<";
                    tokens[position++] = strtok(tokenscpy[i+1], TKN_KID);
                    break;
                }
                tokens[position++] = tokenscpy[i+1];
                if (tokenscpy[i+2] != NULL && strchr(tokenscpy[i+2], '|') == NULL) {
                    tokens[position++] = tokenscpy[i+2];
                } 
                break;
            }
        }
    }
    tokens[position] = NULL;
    
    return tokens;
}


char *mysh_read_line(void) {
    char *line = NULL;
    ssize_t bufsize = 0;
    getline(&line, &bufsize, stdin);
    return line;
}

void abort_handler(int signal) {
    e_flag = 1;
    return;
}

int main(int argc, char *argv[]) {
    char *line;
    char **args;
    int status, *sig;
    
    do {
        if (!e_flag) {
            printf("mysh$ ");
        }
        
        if (signal(SIGINT, abort_handler) == SIG_ERR) {
            exit(1);
        }
        
        if (!e_flag) {
            line = mysh_read_line();
            args = mysh_split_line(line);
            status = mysh_execute(args);
            free(line);
            free(args);
        } else {
            e_flag = 0;
            continue;
        }
    } while (status);
    return 0;
}


int mysh_output_redirect(char **args) {
    pid_t ret;
    int status, fd;
    int cmd_n = 2, j, k;
    int bufsize = TKN_BUFSIZE;
    char **cmd1 = malloc(bufsize * sizeof(char*));
    char **cmd2 = malloc(bufsize * sizeof(char*));

    for (j = 0;args[j] != NULL;j ++) {
        if (strcmp(args[j], ">") != 0) {
            cmd1[j] = args[j];
        } else {
        cmd1[j] = NULL;
        k = j;
        break;
        }
    }
    for (j = k;args[j] != NULL;j ++) {
        cmd2[j-k] = args[j+1];
    }
    cmd2[j-k+1] = NULL;

    ret = fork();
    if (ret == 0) {
        fd = open(cmd2[0], O_WRONLY|O_CREAT|O_TRUNC, 0644);
        close(1);
        dup(fd);
        close(fd);
        execvp(cmd1[0], cmd1);
    } else {
        waitpid(ret, &status, WUNTRACED);
    }
    return 1;
}

int mysh_input_redirect(char **args) {
    pid_t ret;
    int status, fd;
    int cmd_n = 2, j, k;
    int bufsize = TKN_BUFSIZE;
    char **cmd1 = malloc(bufsize * sizeof(char*));
    char **cmd2 = malloc(bufsize * sizeof(char*));

    for (j = 0;args[j] != NULL;j ++) {
        if (strcmp(args[j], "<") != 0) {
            cmd1[j] = args[j];
        } else {
        cmd1[j] = NULL;
        k = j;
        break;
        }
    }
    for (j = k;args[j] != NULL;j ++) {
        cmd2[j-k] = args[j+1];
    }
    cmd2[j-k+1] = NULL;

    ret = fork();
    if (ret == 0) {
        fd = open(cmd2[0], O_RDONLY, 0644);
        dup2(fd, 0);
        close(fd);
        execvp(cmd1[0], cmd1);
    } else {
        waitpid(ret, &status, WUNTRACED);
    }
    return 1;
}

int mysh_pipe(char **args) {
    int i, pipe_locate[5], pipe_count = 0;
    pipe_locate[0] = -1;
    for (i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_count++;
            pipe_locate[pipe_count] = i;
            args[i] = NULL;
        }
    }

    int pfd[4][2];
    if (pipe_count == 0) {
        if (fork() == 0) {
            execvp(args[0], args);
            exit(0);
        }
        else {
            int status;
            wait(&status);
        }
    }

    for (i = 0; i < pipe_count + 1 && pipe_count != 0; i++) {
        if (i != pipe_count) pipe(pfd[i]);

        if (fork() == 0) {
            if (i == 0) {
                dup2(pfd[i][1], 1);
                close(pfd[i][0]); close(pfd[i][1]);
            } else if (i == pipe_count) {
                dup2(pfd[i-1][0], 0);
                close(pfd[i-1][0]); close(pfd[i-1][1]);
            } else {
                dup2(pfd[i-1][0], 0);
                dup2(pfd[i][1], 1);
                close(pfd[i-1][0]); close(pfd[i-1][1]);
                close(pfd[i][0]); close(pfd[i][1]);
            }

            execvp(args[pipe_locate[i]+1], args + pipe_locate[i] + 1);
            exit(0);
        }
        else if (i > 0) {
            close(pfd[i-1][0]); close(pfd[i-1][1]);
        }
    }
    int status;

    for (i = 0; i < pipe_count + 1; i++) {
        wait(&status);
    }
    return 1;
}