#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>

#define ARGUMENT_MAX 16
#define CMD_MAX 4
#define CMDLINE_MAX 512


enum {
    ERR_CMD_NOTFOUND,
    ERR_TOO_MANY_ARG,
    ERR_MISSING_CMD,
    ERR_NO_OUTPUT_FILE,
    ERR_CANT_CD_DIRECTORY,
    ERR_CANT_OPEN_FILE,
    ERR_TOO_MANY_FILES,
    ERR_TOO_MANY_CMD,
    ERR_MIS_LOCATED_OUT,
    ERR_CANT_OPEN_DIR,
    DO_NOTHING
};

struct cmd_info{
    char raw_command[CMDLINE_MAX];
    char** pass_argument;
    int argument_amount;
    char** redirect_file;
    int file_amount;
    int exit_value;
};

struct cmd_info *command;

void call_error(int error_case);
void call_pipeline(struct cmd_info *command, int command_no,
        bool file_err_redirect, bool file_out_redirect);

void complete_message(char *raw_cmd, int total_command_no);
bool construct_cmd(struct cmd_info *command, int cmd_counter,
        int redirect_counter, char **ar_redirect_token, bool *should_exit);

bool error_management(char *cmd);
int fork_redirect(struct cmd_info *current_command, int fd, bool error_redirection);
char **parse_strtok(char *parse_char, int *argument, char *cmd, bool *should_exit);
void pipeline_cmd(struct cmd_info *command, int total_command_no,
        char *raw_cmd, bool file_err_redirect, bool file_out_redirect);

int redirection(struct cmd_info *current_command,
        bool error_redirection, bool *should_exit);
int regular_cmd(char *cmd, char **args);
void stone_free(char **args, int argument);
int sls_built_in(void);




int main(void)
{
        char cmd[CMDLINE_MAX];
        char cmd_duplicate[CMDLINE_MAX + 1];

        while (1) {
                char *nl;
                int retval;
                int total_cmd = 0;
                bool should_exit = false;
                bool error_redirection = false;
                char *cmd_no_space;

                /* Print prompt */
                printf("sshell$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line*/
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';  /* \0 is the null terminator */

                /* make a duplicate of the command, since strtok will modify the cmd */
                strcpy(cmd_duplicate, cmd);

                /* Parsing error detection */
                if (error_management(cmd_duplicate))
                        continue;
                else {
                        cmd_no_space = strtok(cmd_duplicate, " ");
                        /* if nothing in the input */
                        if (cmd_no_space == NULL)
                                continue;
                }

                /* Builtin command for exit, pwd, sls */
                if (!strcmp(cmd_no_space, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n",
                                cmd, 0);
                        break;
                } else if (!strcmp(cmd_no_space, "pwd")) {
                        char *buffer = getcwd(NULL, 0);
                        fprintf(stdout, "%s\n",buffer);
                        fprintf(stderr, "+ completed '%s' [%d]\n",
                                cmd, 0);
                        continue;
                } else if (!strcmp(cmd_no_space, "cd")) {
                        cmd_no_space = strtok(NULL, " ");
                        if (chdir(cmd_no_space) == -1)
                                call_error(ERR_CANT_CD_DIRECTORY);
                        else
                                fprintf(stderr, "+ completed '%s' [%d]\n",
                                        cmd, 0);
                        continue;
                } else if (!strcmp(cmd_no_space, "sls")) {
                        retval = sls_built_in();
                        fprintf(stderr, "+ completed '%s' [%d]\n",
                                cmd, retval);
                        continue;
                }
                /* cmd_duplicate has been modified, copy from cmd again */
                strcpy(cmd_duplicate, cmd);

                /* begin arguments parsing */
                char** ar_total_cmd = parse_strtok("|",&total_cmd,
                                                   cmd_duplicate, &should_exit);

                if (total_cmd > 4) {
                        stone_free(ar_total_cmd, total_cmd);
                        continue;
                }

                /* I create an array of struct to store information for each command */
                command = (struct cmd_info*)malloc(CMD_MAX * sizeof(struct cmd_info));

                /* Iterating through commands */
                for(int cmd_counter = 0; cmd_counter < total_cmd; cmd_counter++) {
                        /* Allocating memory, memory will be freed
                         * either inside this loop or after executing commands */
                        command[cmd_counter].pass_argument =
                                (char**) malloc(ARGUMENT_MAX * sizeof(char*));
                        command[cmd_counter].redirect_file =
                                (char**) malloc(ARGUMENT_MAX * sizeof(char*));
                        command[cmd_counter].argument_amount = 0;
                        command[cmd_counter].file_amount = 0;
                        strcpy(command[cmd_counter].raw_command, ar_total_cmd[cmd_counter]);

                        /*
                         * Detect error for missing commands
                         * and no output file
                         */
                        if (ar_total_cmd[cmd_counter][0] == '>') {
                                call_error(ERR_MISSING_CMD);
                                should_exit = true;
                                break;
                        } else if (ar_total_cmd[cmd_counter][strlen(ar_total_cmd[cmd_counter]) - 1] == '>') {
                                call_error(ERR_NO_OUTPUT_FILE);
                                should_exit = true;
                                break;
                        }

                        int redirect_counter = 0;
                        char **ar_redirect_token = parse_strtok(">",
                                                                &redirect_counter,
                                                                ar_total_cmd[cmd_counter],
                                                                &should_exit);

                        /*
                         * Detect error for output redirect before pipeline commands
                         Only the last command can have output redirection
                         */
                        if ((cmd_counter != total_cmd - 1) && (redirect_counter > 1)) {
                                call_error(ERR_MIS_LOCATED_OUT);
                                should_exit = true;
                                stone_free(ar_redirect_token, redirect_counter);
                                break;
                        }

                        error_redirection = construct_cmd(command, cmd_counter,
                                                          redirect_counter,
                                                          ar_redirect_token,
                                                          &should_exit);

                        stone_free(ar_redirect_token, redirect_counter);

                        if (should_exit)
                                break;
                }

                stone_free(ar_total_cmd, total_cmd);

                if (!should_exit) {


                        if (total_cmd == 1) {
                                /* single command */
                                if (command[0].file_amount) {
                                        retval = redirection(&command[0],
                                                             error_redirection,
                                                             &should_exit);
                                        if (!should_exit) {
                                                fprintf(stderr,
                                                        "+ completed '%s' [%d]\n",
                                                        command[0].raw_command,
                                                        retval);
                                        }
                                } else {
                                        retval = regular_cmd(
                                                command[0].pass_argument[0],
                                                command[0].pass_argument);
                                        fprintf(stderr,
                                                "+ completed '%s' [%d]\n",
                                                command[0].raw_command, retval);
                                }

                        } else {
                                /* open pipe and execute */
                                if (command[total_cmd - 1].file_amount != 0)
                                        pipeline_cmd(command, total_cmd, cmd,
                                                     error_redirection, true);
                                else
                                        pipeline_cmd(command, total_cmd, cmd,
                                                     false, false);
                        }


                }
                // free memory inside command[cmd]
                for (int i = 0; i < total_cmd; i++) {
                        stone_free(command[i].pass_argument,
                                   command[i].argument_amount);
                        stone_free(command[i].redirect_file,
                                   command[i].file_amount);
                }
                free(command);

        }

        return EXIT_SUCCESS;
}

void pipeline_cmd(struct cmd_info *command, int total_command_no, char *raw_cmd,
                  bool file_err_redirect, bool file_out_redirect)
{
        pid_t pid;
        pid = fork();

        if (pid == 0)
                /* This is the child */
                call_pipeline(command, total_command_no, file_err_redirect,
                              file_out_redirect);
        else if (pid > 0) {
                /* parent wait for all child process terminated */
                sleep(1);
                complete_message(raw_cmd, total_command_no);
        } else {
                perror("fork");
                exit(1);
        }
}

void
call_pipeline(struct cmd_info *command, int command_no, bool file_err_redirect,
              bool file_out_redirect)
{
        /* open the pipe between command 2 and 3 */
        int fd_23[2];
        pipe(fd_23);

        /*
         * The whole structure is:
         *
         * if (fork() != 0):
         *      Parent running
         *      if (fork() != 0):
         *              This is the parent under parent
         *              command 1
         *      else:
         *              This is the parent under child
         *              command 2
         * else:
         *      Child running
         *      if (fork() != 0):
         *              This is the child under parent
         *              command 3
         *      else:
         *              This is the child under child
         *              command 4
         *
         * if no command is provided for command 3 or 4, they will do nothing
         *
         * */

        if (fork() != 0) {
                int fd_12[2];
                pipe(fd_12);
                /* I am the parent */
                if (fork() != 0) {
                        /* command 1 */
                        close(fd_23[1]);
                        close(fd_23[0]);

                        /* no need for read access */
                        close(fd_12[0]);

                        /* write to command 2 */
                        dup2(fd_12[1], STDOUT_FILENO);
                        close(fd_12[1]);
                        command[0].exit_value = execvp(
                                command[0].pass_argument[0],
                                command[0].pass_argument);

                } else {
                        /* command 2 */
                        if (command_no <= 2) {
                                /* Total command number is 2
                                 * no need for pipe between command 2 and 3
                                 */
                                close(fd_23[1]);
                                close(fd_23[0]);
                                /* no need for write access */
                                close(fd_12[1]);
                                /* read from command 1 */
                                dup2(fd_12[0], STDIN_FILENO);
                                close(fd_12[0]);

                                if (file_out_redirect) {
                                        /* need to redirect output */
                                        int retval = 0;
                                        for (int i = 0; i < command[1].file_amount; i++) {
                                                /*
                                                 * if multiple redirection
                                                 * needs, open all of the
                                                 * file
                                                 */
                                                int fd;
                                                fd = open(command[1].redirect_file[i],
                                                          O_CREAT|O_WRONLY|O_TRUNC, 00777);
                                                if (fd == -1) {
                                                        call_error(ERR_CANT_OPEN_FILE);
                                                        exit(1);
                                                }
                                                /*
                                                 * if multiple redirection
                                                 * needs, open all of the
                                                 * file
                                                 */
                                                if (i == command[1].file_amount - 1) {
                                                        /* reach the last
                                                         * file, begin the
                                                         * redirection
                                                         */
                                                        dup2(fd, STDOUT_FILENO);
                                                        if (file_err_redirect)
                                                                dup2(fd, STDERR_FILENO);

                                                        close(fd);

                                                        retval = execvp(command[1].pass_argument[0],
                                                                        command[1].pass_argument);
                                                        if (retval != 0) {
                                                                call_error(ERR_MISSING_CMD);
                                                                exit(1);
                                                        }
                                                } else
                                                        close(fd);
                                        }

                                } else {
                                        command[1].exit_value = execvp(command[1].pass_argument[0],
                                                                       command[1].pass_argument);
                                }

                        } else {
                                // fprintf(stdout, "im ine line 124\n");
                                close(fd_23[0]);
                                close(fd_12[1]);

                                dup2(fd_12[0], STDIN_FILENO);
                                close(fd_12[0]);

                                dup2(fd_23[1], STDOUT_FILENO);      // write to command 3
                                close(fd_23[1]);

                                // execute command 2
                                command[1].exit_value = execvp(command[1].pass_argument[0],
                                                               command[1].pass_argument);
                                exit(0);

                        }
                }

        } else {
                int fd_34[2];
                pipe(fd_34);
                if (fork() != 0) {
                        /* command 3 */
                        if (command_no >= 3) {
                                if (command_no == 4) {
                                        close(fd_23[1]);
                                        close(fd_34[0]);

                                        /* read from command 2 */
                                        dup2(fd_23[0], STDIN_FILENO);
                                        close(fd_23[0]);

                                        /* write to command 4 */
                                        dup2(fd_34[1], STDOUT_FILENO);
                                        close(fd_34[1]);

                                        command[2].exit_value = execvp(command[2].pass_argument[0],
                                                                       command[2].pass_argument);
                                        exit(0);
                                } else {
                                        close(fd_23[1]);
                                        /* total command_no = 3
                                         * close pipe between command 3 and 4
                                         */
                                        close(fd_34[1]);
                                        close(fd_34[0]);

                                        /* read from command 2 */
                                        dup2(fd_23[0], STDIN_FILENO);
                                        close(fd_23[0]);

                                        /* execute command 3 */
                                        if (file_out_redirect) {
                                                /* need to redirect output */
                                                int retval = 0;
                                                for (int i = 0; i < command[2].file_amount; i++) {
                                                        /* open file */
                                                        int fd;
                                                        fd = open(command[2].redirect_file[i],
                                                                  O_CREAT|O_WRONLY|O_TRUNC, 00777);
                                                        if (fd == -1) {
                                                                call_error(ERR_CANT_OPEN_FILE);
                                                                exit(1);
                                                        }
                                                        if (i == command[2].file_amount - 1) {
                                                                dup2(fd, STDOUT_FILENO);
                                                                if (file_err_redirect)
                                                                        dup2(fd, STDERR_FILENO);
                                                                close(fd);

                                                                retval = execvp(command[2].pass_argument[0],
                                                                                command[2].pass_argument);
                                                                if (retval != 0) {
                                                                        call_error(ERR_MISSING_CMD);
                                                                        exit(1);
                                                                }
                                                        } else
                                                                close(fd);
                                                }

                                        } else
                                                command[2].exit_value = execvp(command[2].pass_argument[0],
                                                                               command[2].pass_argument);
                                }

                        } else {
                                /* no command, just close the pipe */
                                close(fd_23[0]);
                                close(fd_23[1]);
                                close(fd_34[0]);
                                close(fd_34[1]);
                                exit(0);
                        }

                } else {
                        /* command 4 */
                        if (command_no == 4) {
                                close(fd_23[1]);
                                close(fd_23[0]);

                                /* no need for write access */
                                close(fd_34[1]);
                                /* read from command 3 */
                                dup2(fd_34[0], STDIN_FILENO);
                                close(fd_34[0]);
                                /* execute command 3 */
                                if (file_out_redirect) {
                                        /* output redirect to file */
                                        int retval = 0;
                                        for (int i = 0; i < command[3].file_amount; i++) {
                                                int fd;
                                                fd = open(command[3].redirect_file[i],
                                                          O_CREAT|O_WRONLY|O_TRUNC, 00777);
                                                if (fd == -1) {
                                                        call_error(ERR_CANT_OPEN_FILE);
                                                        exit(1);
                                                }

                                                if (i == command[2].file_amount - 1) {
                                                        dup2(fd, STDOUT_FILENO);
                                                        if (file_err_redirect)
                                                                dup2(fd, STDERR_FILENO);
                                                        close(fd);

                                                        retval = execvp(command[3].pass_argument[0],
                                                                        command[3].pass_argument);
                                                        if (retval != 0) {
                                                                call_error(ERR_MISSING_CMD);
                                                        }       exit(1);
                                                } else
                                                        close(fd);
                                        }
                                } else
                                        command[3].exit_value = execvp(command[3].pass_argument[0],
                                                                       command[3].pass_argument);

                        } else {
                                /* no command, just close the pipe */
                                close(fd_23[1]);
                                close(fd_23[0]);
                                close(fd_34[1]);
                                close(fd_34[0]);
                                exit(0);
                        }
                }
        }
}

int regular_cmd(char *cmd, char **args)
{       /* char *cmd is the command,
        *  char **args contains the argument for this command
        *  */
        int cmd_return;
        pid_t pid;
        pid = fork();
        if (pid == 0) {
                /* This is the child */
                cmd_return = execvp(cmd, args);
                /* If command not found, exit */
                if (cmd_return != 0) {
                        call_error(ERR_MISSING_CMD);
                        exit(1);
                }

        } else if (pid > 0) {
                /* This is the parent */
                int status;
                waitpid(pid, &status, 0);
                return WEXITSTATUS(status);

        } else {
                perror("fork");
                exit(1);
        }

        return 0;
}

int redirection(struct cmd_info *current_command, bool error_redirection, bool *should_exit)
{
        int retval;
        for (int i = 0; i < (*current_command).file_amount; i++) {
                int fd;
                /* open multiple redirection file one by one */
                fd = open((*current_command).redirect_file[i], O_CREAT|O_WRONLY|O_TRUNC, 00777);
                if (fd == -1) {
                        call_error(ERR_CANT_OPEN_FILE);
                        *should_exit = true;
                        break;
                }
                if (i == (*current_command).file_amount - 1)
                        retval = fork_redirect(current_command, fd, error_redirection);
                else
                        close(fd);
        }

        return retval;
}

int fork_redirect(struct cmd_info *current_command, int fd, bool file_err_redirect)
{
        int cmd_return;
        pid_t pid;
        pid = fork();
        if (pid == 0) {
                /* This is the child */
                dup2(fd, STDOUT_FILENO);
                if (file_err_redirect) {
                        dup2(fd, STDERR_FILENO);
                }
                close(fd);

                /* check for redirect error */
                cmd_return = execvp((*current_command).pass_argument[0],
                                    (*current_command).pass_argument);
                /* If command not found, exit */
                if (cmd_return != 0) {
                        call_error(ERR_MISSING_CMD);
                        exit(1);
                }

        } else if (pid > 0) {
                /* This is the parent */
                int status;
                waitpid(pid, &status, 0);
                return WEXITSTATUS(status);
        } else {
                perror("fork");
                exit(1);
        }
        return 0;
}

void complete_message(char *raw_cmd, int total_command_no)
{
        /* complete message for pipe */
        int len = 15 + strlen(raw_cmd) + total_command_no * 3;
        char *cmd = (char*)malloc((len + 1) * sizeof(char));
        snprintf(cmd, 16 + strlen(raw_cmd), "+ completed '%s' ", raw_cmd);

        for (int i = 0; i < total_command_no; i++) {
                char temp[4];
                snprintf(temp, 4, "[%d]", command[i].exit_value);
                strcat(cmd, temp);
        }
        printf("%s\n", cmd);
        free(cmd);
}

char **parse_strtok(char *parse_char, int *argument, char *cmd, bool *should_exit)
{
        char *token;
        token = strtok(cmd, parse_char);
        char **args = (char**) malloc(ARGUMENT_MAX * sizeof(char*));
        /* split arguments between parse_char */
        while (token != NULL){
                if (*argument == 16) {
                        *should_exit = true;
                        break;
                }
                args[*argument] = (char*)malloc((strlen(token) + 1) * sizeof(char));
                strcpy(args[*argument], token);
                (*argument)++;
                token = strtok(NULL, parse_char);
        }
        return args;
}

void call_error(int error_case)
{
        switch (error_case) {
                case ERR_CMD_NOTFOUND:
                        fprintf(stderr,"Error: command not found\n");
                        break;
                case ERR_TOO_MANY_ARG:
                        fprintf(stderr,"Error: too many process arguments\n");
                        break;
                case ERR_MISSING_CMD:
                        fprintf(stderr,"Error: missing command\n");
                        break;
                case ERR_NO_OUTPUT_FILE:
                        fprintf(stderr,"Error: no output file\n");
                        break;
                case ERR_CANT_CD_DIRECTORY:
                        fprintf(stderr,"Error: cannot cd into directory\n");
                        break;
                case ERR_MIS_LOCATED_OUT:
                        fprintf(stderr,"Error: mislocated output redirection\n");
                        break;
                case ERR_TOO_MANY_CMD:
                        fprintf(stderr,"Error: too many pipe commands\n");
                        break;
                case ERR_CANT_OPEN_FILE:
                        fprintf(stderr,"Error: cannot open output file\n");
                        break;
                case ERR_CANT_OPEN_DIR:
                        fprintf(stderr,"Error: cannot open directory\n");
                        break;
        }
}

bool construct_cmd(struct cmd_info *command, int cmd_counter, int redirect_counter,
        char **ar_redirect_token, bool *should_exit)
{
        int arg_counter = 0;
        int file_counter = 0;
        bool error_redirection = false;

        for (int i = 0; i < redirect_counter; i++) {
                /* check if standard error redirection should be executed */
                if (i > 0) {
                        if (ar_redirect_token[i][0] == '&') {
                                ar_redirect_token[i][0] = ' ';
                                error_redirection = true;
                        }
                }
                /* deleting the white space */
                int token_atr_split = 0;
                char** arguments_token = parse_strtok(" ",&token_atr_split,
                                                      ar_redirect_token[i], should_exit);
                if (*should_exit) {
                        call_error(ERR_TOO_MANY_ARG);
                        stone_free(arguments_token, token_atr_split);
                        break;
                }

                /* Error detection */
                if (token_atr_split == 0){
                        if (redirect_counter == 1)
                                call_error(ERR_MISSING_CMD);
                        else if (i == 0)
                                call_error(ERR_MISSING_CMD);
                        else
                                call_error(ERR_NO_OUTPUT_FILE);

                        *should_exit = true;
                        stone_free(arguments_token, token_atr_split);
                        break;
                }

                /*
                 * We need to know whether the current parsed token is an
                 * argument or a file name
                 */
                for (int j = 0; j < token_atr_split; j++) {
                        /* i < redirect_counter */
                        if ((i == 0) || (j > 0)) {
                                /* Current token is an argument */
                                if (arg_counter == 16) {
                                        *should_exit = true;
                                        call_error(ERR_TOO_MANY_ARG);
                                        break;
                                }
                                command[cmd_counter].pass_argument[arg_counter] =
                                        (char*)malloc(strlen(arguments_token[j]) * sizeof(char));
                                strcpy(command[cmd_counter].pass_argument[arg_counter], arguments_token[j]);

                                arg_counter++;
                        } else {
                                /* Current token is a file */
                                if (file_counter == 16) {
                                        *should_exit = true;
                                        call_error(ERR_TOO_MANY_FILES);
                                        break;
                                }
                                command[cmd_counter].redirect_file[file_counter] =
                                        (char*)malloc(strlen(arguments_token[j]) * sizeof(char));
                                strcpy(command[cmd_counter].redirect_file[file_counter], arguments_token[j]);
                                file_counter++;
                        }

                }
                stone_free(arguments_token, token_atr_split);
                /* too many arguments, should break */
                if (*should_exit) {
                        break;
                }
        }
        command[cmd_counter].argument_amount = arg_counter;
        command[cmd_counter].file_amount = file_counter;

        return error_redirection;
}

void stone_free(char **args, int argument)
{
        for (int i = 0; i < argument; i++) {
                free(args[i]);
        }
        free(args);
}
int sls_built_in(void)
{
        DIR *dirp;
        struct dirent *dp;
        dirp = opendir(getcwd(NULL, 0));
        if(!dirp){
                call_error(ERR_CANT_OPEN_DIR);
                return 1;
        }
        while ((dp = readdir(dirp)) != NULL){
                if((!strcmp(dp->d_name, "."))||(!strcmp(dp->d_name, ".."))){
                        continue;
                }
                printf("%s ", dp->d_name);
                struct stat sb;
                stat(dp->d_name, &sb);
                printf("(%lld bytes)\n",(long long) sb.st_size);
        }
        closedir(dirp);
        return 0;
}

bool error_management(char *cmd)
{
        if ((cmd[0] == '>') || (cmd[0] == '|')) {
                /* Error: missing command */
                call_error(ERR_MISSING_CMD);
                return true;
        } else if (cmd[strlen(cmd) - 1] == '|') {
                /* Error: missing command */
                call_error(ERR_MISSING_CMD);
                return true;
        } else if (strstr(cmd, ">>")) {
                /* Error: no output file */
                call_error(ERR_NO_OUTPUT_FILE);
                return true;
        } else if (strstr(cmd,"||")) {
                /* Error: missing command */
                call_error(ERR_MISSING_CMD);
                return true;
        }
        return false;
}

