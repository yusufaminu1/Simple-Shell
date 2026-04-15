#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

/* =========================================================
 * TOKENIZER
 * ========================================================= */

/*
 * tokenize(line, tokens, max_tokens)
 *
 * Split `line` into tokens stored in `tokens[]`.
 * Rules:
 *   - Split on whitespace
 *   - '<', '>', '|' are always their own token (assume space-separated)
 *   - '#' starts a comment — stop tokenizing at '#'
 * Returns the number of tokens found.
 */
int tokenize(char *line, char **tokens, int max_tokens) {
    int count = 0;                                                                          
    int i = 0;                                                                              
    while(line[i] != '\0' && count < max_tokens){                                         
        if(line[i] == '#') break;
        if(isspace(line[i])){                                                             
            i++;                                                                          
            continue;                                                                       
        }
        tokens[count++] = &line[i];
        while(line[i] != '\0' && !isspace(line[i]) && line[i] != '#'){
            i++;
        }
        if(line[i] != '\0' && line[i] != '#'){
            line[i] = '\0';
            i++;
        }else{
            line[i] = '\0';
        }
      }
      return count;
    return 0;
}

/* =========================================================
 * WILDCARD EXPANSION
 * ========================================================= */

/*
 * expand_wildcards(tokens, count, expanded, max)
 *
 * For each token in tokens[0..count-1]:
 *   - If it contains '*', find all matching filenames using opendir/readdir
 *     and append them to `expanded[]`
 *   - Hidden files (starting with '.') are NOT matched by a leading '*'
 *   - If no files match, pass the token through unchanged
 *   - Otherwise, copy the token to `expanded[]` as-is
 * Returns the new token count.
 */
int expand_wildcards(char **tokens, int count, char **expanded, int max) {
    /* TODO */
    return 0;
}

/* =========================================================
 * PROGRAM RESOLUTION
 * ========================================================= */

/*
 * find_program(name, result)
 *
 * Search for `name` in /usr/local/bin, /usr/bin, /bin (in that order).
 * Use access(path, X_OK) to check existence.
 * If found, write the full path into `result` and return 1.
 * If not found, return 0.
 * Do NOT handle built-ins or paths containing '/' here.
 */
int find_program(const char *name, char *result) {
    char *dirs[] = {"/usr/local/bin","/usr/bin","/bin"};
    for(int i = 0;i<3;i++){
        snprintf(result,4096,"%s/%s",dirs[i],name);
        if(access(result,X_OK)==0){
            return 1;
        }
    }
    return 0;
}

/* =========================================================
 * BUILT-IN COMMANDS
 * ========================================================= */

/*
 * builtin_cd(args, count)
 *
 * Change the working directory.
 *   - 0 args: cd to HOME (getenv("HOME"))
 *   - 1 arg:  cd to args[0]
 *   - 2+ args: print error, return failure
 * Use chdir(). Print error and return 0 on failure, 1 on success.
 */
int builtin_cd(char **args, int count) {
    /* TODO */
    return 0;
}

/*
 * builtin_pwd()
 *
 * Print the current working directory to stdout using getcwd().
 * Returns 1 on success, 0 on failure.
 */
int builtin_pwd(void) {
    /* TODO */
    return 0;
}

/*
 * builtin_which(args, count)
 *
 * Print the full path of the program named args[0].
 *   - Fails (returns 0, prints nothing) if:
 *       - count != 1
 *       - args[0] is a built-in name (cd, pwd, which, exit)
 *       - program is not found in the search path
 * Uses find_program() internally.
 */
int builtin_which(char **args, int count) {
    /* TODO */
    return 0;
}

/* =========================================================
 * REDIRECTION HELPERS
 * ========================================================= */

/*
 * parse_redirection(tokens, count, args, arg_count, in_file, out_file)
 *
 * Walk tokens[0..count-1] and separate:
 *   - Normal tokens   → appended to args[], arg_count incremented
 *   - '<' token       → next token saved into *in_file
 *   - '>' token       → next token saved into *out_file
 * Sets *in_file and *out_file to NULL if not present.
 * Returns 1 on success, 0 on syntax error (e.g. '<' with no following token,
 * or two '<' in a row).
 */
int parse_redirection(char **tokens, int count,
                      char **args, int *arg_count,
                      char **in_file, char **out_file) {
    /* TODO */
    return 0;
}

/* =========================================================
 * PROCESS EXECUTION
 * ========================================================= */

/*
 * exec_command(args, in_file, out_file, interactive)
 *
 * Execute a single (non-pipe) command described by args[].
 * args[0] is the program name or built-in.
 *
 * Steps:
 *   1. Check for built-ins (cd, pwd, which, exit) and run them directly.
 *   2. Resolve the program path (contains '/' → use directly;
 *      otherwise call find_program()).
 *   3. fork():
 *       Child:
 *         - If in_file set:  open and dup2 onto STDIN_FILENO
 *         - If out_file set: open (create/truncate, mode 0640) and dup2 onto STDOUT_FILENO
 *         - If batch mode and no in_file: open /dev/null and dup2 onto STDIN_FILENO
 *         - execv(path, args)
 *         - On execv failure: print error, _exit(EXIT_FAILURE)
 *       Parent:
 *         - waitpid(), return exit status info
 *
 * Returns the exit status of the child (as returned by waitpid),
 * or -1 on fork/exec setup failure.
 * For built-ins, returns 0 on success, -1 on failure.
 */
int exec_command(char **args, char *in_file, char *out_file, int interactive) {
    int arg_count = 0;
    while(args[arg_count]!=NULL){
        arg_count++;
    }
    if(strcmp(args[0],"cd")==0){
        return builtin_cd(args+1,arg_count-1);
    }else if(strcmp(args[0],"pwd")==0){
        return builtin_pwd();
    }else if(strcmp(args[0],"which")==0){
        return builtin_which(args+1,arg_count-1);
    }else if(strcmp(args[0],"exit")==0){
        return -2;
    }
    char pathbuf[4096];                                                                                                                                                                 
    if(strchr(args[0], '/') != NULL) {                                                         
        strcpy(pathbuf, args[0]);                                                               
    }else{                                                                                    
      if(find_program(args[0], pathbuf) == 0) {                                              
            dprintf(STDERR_FILENO, "error: command not found: %s\n", args[0]);
            return -1;
      }
    }
    pid_t pid = fork();
    if(pid==-1){
        perror("fork");
        return -1;
    }
    if(pid==0){
        if (in_file != NULL) {                                                                      
            int fd = open(in_file, O_RDONLY);                                                       
            if (fd == -1) { perror(in_file); _exit(EXIT_FAILURE); }                                 
            dup2(fd, STDIN_FILENO);                                                                 
            close(fd);                                                                            
        }else if(!interactive) {
            int fd = open("/dev/null", O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if(out_file != NULL) {
            int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (fd == -1) { perror(out_file); _exit(EXIT_FAILURE); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execv(pathbuf, args);
        perror(args[0]);
        _exit(EXIT_FAILURE);
    }else{
        int status;                                                                                 
        waitpid(pid, &status, 0);                                                                   
        return status;
    }
    return -1;
}

/*
 * exec_pipeline(segments, seg_count, interactive)
 *
 * Execute a pipeline of `seg_count` sub-commands.
 * Each segment is a NULL-terminated array of argument strings.
 *
 * Steps:
 *   - For each adjacent pair, call pipe() to create a pipe
 *   - fork() each sub-command:
 *       - All but the last: dup2 write-end of pipe onto STDOUT_FILENO
 *       - All but the first: dup2 read-end of previous pipe onto STDIN_FILENO
 *       - Close all pipe fds in child before execv
 *   - Parent closes all pipe fds, then waitpid() for all children
 *   - Success is determined by the exit status of the LAST sub-command
 *
 * Returns the exit status of the last child, or -1 on error.
 */
int exec_pipeline(char ***segments, int seg_count, int interactive) {
    /* TODO */
    return -1;
}

/* =========================================================
 * COMMAND DISPATCH
 * ========================================================= */

/*
 * run_command(tokens, count, interactive)
 *
 * Given the token list for one line, figure out what to run:
 *   1. If count == 0: do nothing (empty line / comment-only)
 *   2. If tokens contain '|': split into pipeline segments,
 *      call exec_pipeline()
 *   3. Otherwise: call parse_redirection(), then exec_command()
 *
 * In interactive mode, after the command finishes report exit status:
 *   - Exited with non-zero code → print "Exited with status N"
 *   - Killed by signal          → print "Terminated by signal X" (use strsignal)
 *
 * Returns 1 normally, 0 if the command was `exit` (signals main loop to stop).
 */
int run_command(char **tokens, int count, int interactive) {
    /* TODO */
    return 1;
}

/* =========================================================
 * INPUT LOOP
 * ========================================================= */

/*
 * print_prompt(interactive)
 *
 * If interactive, print the prompt to STDOUT_FILENO.
 * Format: "<cwd>$ " where the home directory prefix is replaced with '~'.
 * Use getcwd() and getenv("HOME").
 */
void print_prompt(int interactive) {
    if(interactive==0){
        return;
    }
    char cwd[4096];
    getcwd(cwd,sizeof(cwd));
    char *home = getenv("HOME");
    if(home!= NULL && strncmp(cwd,home,strlen(home))==0){
        write(STDOUT_FILENO,"~",1);
        char *remainder = cwd+strlen(home);
        write(STDOUT_FILENO,remainder,strlen(remainder));
    }else{
        write(STDOUT_FILENO,cwd,strlen(cwd));
    }
    write(STDOUT_FILENO,"$ ",2);
    
}

/*
 * read_line(fd, buf, max)
 *
 * Read one line from `fd` into `buf` using read() one byte at a time.
 * Stop at '\n' or EOF.
 * Null-terminate the result (strip the trailing '\n').
 * Returns the number of bytes read (0 on EOF).
 */
int read_line(int fd, char *buf, int max) {
    int i = 0;
    while(i<max-1){
        int r = read(fd,&buf[i],1);
        if(r==0 || buf[i]=='\n'){
            break;
        }
        i++;
    }
    buf[i]='\0';
    return i;
}

/* =========================================================
 * MAIN
 * ========================================================= */

int main(int argc, char *argv[]) {
    int fd;
    int interactive;

    if(argc > 2){
        write(STDERR_FILENO, "error: too many arguments\n", 26);
        exit(EXIT_FAILURE);
    }else if(argc == 2){
        fd = open(argv[1], O_RDONLY);
        if(fd == -1){
            dprintf(STDERR_FILENO,"error: cannot open file %s\n",argv[1]);
            exit(EXIT_FAILURE);
        }

    }else if(argc == 1){
        fd = STDIN_FILENO;
        
    }
    interactive = (argc == 1) && isatty(STDIN_FILENO);
    if(interactive){
        write(STDOUT_FILENO, "Welcome to my shell\n",20);
    }


    /* Main loop */
    char line[4096];
    char *tokens[256];
    while (1) {
        print_prompt(interactive);

        int n = read_line(fd, line, sizeof(line));
        if (n == 0) break; /* EOF */

        int count = tokenize(line, tokens, 256);

        int keep_going = run_command(tokens, count, interactive);
        if (!keep_going) break;
    }

    if (interactive) write(STDOUT_FILENO, "Exiting my shell\n", 17);                            
    if (fd != STDIN_FILENO) close(fd); 

    return EXIT_SUCCESS;
}
