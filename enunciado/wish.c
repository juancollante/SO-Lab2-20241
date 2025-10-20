/* Minimal skeleton for wish shell
 * - interactive prompt (wish> ) when stdin is a terminal
 * - optional batch mode: ./wish batchfile
 * - simple tokenization (spaces/tabs)
 * - initial path: /bin
 * - execute external commands via fork + execv + wait
 * - minimal builtin: exit (no args)
 * - prints exact error message on failures
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ERROR_MESSAGE "An error has occurred\n"

static void print_error() {
    write(STDERR_FILENO, ERROR_MESSAGE, sizeof(ERROR_MESSAGE) - 1);
}

/* trim trailing newline(s) */
static void chomp(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        --n;
    }
}

int main(int argc, char **argv) {
    FILE *in = stdin;
    int interactive = isatty(STDIN_FILENO) && argc == 1;

    if (argc > 2) {
        print_error();
        return 1;
    }

    if (argc == 2) {
        in = fopen(argv[1], "r");
        if (!in) {
            print_error();
            return 1;
        }
        interactive = 0;
    }

    /* initial path list: dynamic, start with /bin */
    size_t path_cap = 4;
    size_t path_count = 1;
    char **path_list = malloc(sizeof(char*) * path_cap);
    if (!path_list) {
        print_error();
        return 1;
    }
    path_list[0] = strdup("/bin");
    path_list[1] = NULL;

    char *line = NULL;
    size_t linecap = 0;

    while (1) {
        if (interactive) {
            if (isatty(STDIN_FILENO)) {
                printf("wish> ");
                fflush(stdout);
            }
        }

        ssize_t linelen = getline(&line, &linecap, in);
        if (linelen < 0) {
            /* EOF or error */
            break;
        }
        chomp(line);

        /* skip empty lines */
        char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') continue;

        /* simple tokenization by spaces/tabs */
        char *saveptr = NULL;
        char *tok = NULL;
        int arg_count = 0;
        int arg_cap = 8;
        char **args = malloc(sizeof(char*) * arg_cap);
        if (!args) {
            print_error();
            continue;
        }

        tok = strtok_r(line, " \t", &saveptr);
        while (tok) {
            if (arg_count + 1 >= arg_cap) {
                arg_cap *= 2;
                char **tmp = realloc(args, sizeof(char*) * arg_cap);
                if (!tmp) break;
                args = tmp;
            }
            args[arg_count++] = tok;
            tok = strtok_r(NULL, " \t", &saveptr);
        }
        args[arg_count] = NULL;

        if (arg_count == 0) {
            free(args);
            continue;
        }

        /* builtins */
        if (strcmp(args[0], "exit") == 0) {
            if (arg_count != 1) {
                print_error();
            } else {
                free(args);
                break;
            }
            free(args);
            continue;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (arg_count != 2) {
                print_error();
            } else {
                if (chdir(args[1]) != 0) {
                    print_error();
                }
            }
            free(args);
            continue;
        }

        if (strcmp(args[0], "path") == 0) {
            /* replace path_list with given args (args[1..]) */
            /* free old list */
            for (size_t i = 0; i < path_count; ++i) {
                free(path_list[i]);
            }
            path_count = 0;

            /* allocate if necessary */
            /* ensure capacity */
            if (arg_count - 1 + 1 > (int)path_cap) {
                size_t needed = arg_count - 1 + 1;
                char **tmp = realloc(path_list, sizeof(char*) * needed);
                if (!tmp) {
                    print_error();
                    free(args);
                    continue;
                }
                path_list = tmp;
                path_cap = needed;
            }

            for (int i = 1; i < arg_count; ++i) {
                path_list[path_count++] = strdup(args[i]);
            }
            path_list[path_count] = NULL;
            free(args);
            continue;
        }

        /* find executable in path_list
         * If the command contains a '/' use it directly (absolute/relative path).
         */
        char cmdpath[4096];
        char *found = NULL;
        if (strchr(args[0], '/') != NULL) {
            if (access(args[0], X_OK) == 0) {
                found = args[0];
            }
        } else {
            for (int i = 0; path_list[i] != NULL; ++i) {
                snprintf(cmdpath, sizeof(cmdpath), "%s/%s", path_list[i], args[0]);
                if (access(cmdpath, X_OK) == 0) {
                    found = cmdpath;
                    break;
                }
            }
        }

        if (!found) {
            print_error();
            free(args);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            print_error();
            free(args);
            continue;
        }

        if (pid == 0) {
            /* child */
            execv(found, args);
            /* if execv returns, an error occurred */
            print_error();
            _exit(1);
        } else {
            /* parent */
            int status = 0;
            waitpid(pid, &status, 0);
        }

        free(args);
    }

    free(line);
    if (in != stdin) fclose(in);
    return 0;
}
