#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>


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
  int exp_count = 0;

  for (int i = 0; i < count && exp_count < max; i++) {
    if (strchr(tokens[i], '*') == NULL) {
      expanded[exp_count++] = tokens[i];
      continue;
    }

    char dir_part[4096];
    char *pattern;
    char *last_slash = strrchr(tokens[i], '/');

    if (last_slash == NULL) {
      strcpy(dir_part, ".");
      pattern = tokens[i];
    } else {
      int dir_len = last_slash - tokens[i];
      strncpy(dir_part, tokens[i], dir_len);
      dir_part[dir_len] = '\0';
      pattern = last_slash + 1;
    }

    char *star = strchr(pattern, '*');
    int prefix_len = star - pattern;
    char *suffix = star + 1;
    int suffix_len = strlen(suffix);

    DIR *dir = opendir(dir_part);
    if (dir == NULL) {
      expanded[exp_count++] = tokens[i];
      continue;
  }

    char *matches[1024];
    int match_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        if (prefix_len == 0 && name[0] == '.') continue; // skip hidden

        int name_length = strlen(name);
        if (name_length < prefix_len + suffix_len) continue;
        if (strncmp(name, pattern, prefix_len) != 0) continue;
        if (suffix_len > 0 && strcmp(name + name_length - suffix_len, suffix) != 0) continue;

        char *full = malloc(strlen(dir_part) + 1 + name_length + 1);
        if (strcmp(dir_part, ".") == 0) {
          strcpy(full, name);
        } else {
          sprintf(full, "%s/%s", dir_part, name);
        }
        matches[match_count++] = full;
    }
    closedir(dir);

    if (match_count == 0) {
      expanded[exp_count++] = tokens[i];
    } else {
      for (int a = 0; a < match_count - 1; a++) {
        for (int b = a + 1; b < match_count; b++) {
            if (strcmp(matches[a], matches[b]) > 0) {
              char *tmp = matches[a];
              matches[a] = matches[b];
              matches[b] = tmp;
            }
          }
        }
      for (int j = 0; j < match_count && exp_count < max; j++) {
        expanded[exp_count++] = matches[j];
      }
    }
  }
  return exp_count;

}



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
    if (count == 0) {
      if(chdir(getenv("HOME")) == -1) {
        perror("cd");
        return 0;
      }
     }
    else if (count == 1) {
      if(chdir(args[0]) == -1) {
        perror("cd");
        return 0;
      }
    }
    else {
      dprintf(STDERR_FILENO, "cd: too many arguments\n");
      return 0;
    }
    return 1;
}

/*
 * builtin_pwd()
 *
 * Print the current working directory to stdout using getcwd().
 * Returns 1 on success, 0 on failure.
 */
int builtin_pwd(void) {
    char buf[4096];
    if(getcwd(buf, sizeof(buf)) == NULL) {
      perror("pwd");
      return 0;
    }
    write(STDOUT_FILENO, buf, strlen(buf));
    write(STDOUT_FILENO, "\n", 1);
    return 1;
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
  char path[4096];
    if(count != 1) { return 0; }
    if(strcmp(args[0], "cd") == 0 || strcmp(args[0], "pwd") == 0 || strcmp(args[0], "which") == 0 || strcmp(args[0], "exit") == 0) {
      return 0;
    }
    if(find_program(args[0], path) == 0) { return 0; }
    write(STDOUT_FILENO, path, strlen(path));
    write(STDOUT_FILENO, "\n", 1);
    return 1;
}



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
    *in_file = NULL;
    *out_file = NULL;
    *arg_count = 0;
    for(int i = 0; i < count; i++) {
      if(strcmp(tokens[i], ">") == 0) {
        if(i + 1 >= count || strcmp(tokens[i+1], "<") == 0 || strcmp(tokens[i+1], ">") == 0) {
          dprintf(STDERR_FILENO, "syntax error: expected filename after %s\n", tokens[i]);
          return 0;
        }
        *out_file = tokens[i+1];
        i++;
      } else if(strcmp(tokens[i], "<") == 0) {
        if(i + 1 >= count || strcmp(tokens[i+1], "<") == 0 || strcmp(tokens[i+1], ">") == 0) {
          dprintf(STDERR_FILENO, "syntax error: expected filename after %s\n", tokens[i]);
          return 0;
        }
        *in_file = tokens[i+1];
        i++;
      } else {
        args[*arg_count] = tokens[i];
        (*arg_count)++;
      }
    }
    args[*arg_count] = NULL;
    return 1;
}



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
    if(args[0] == NULL) return -1;
    int arg_count = 0;
    while(args[arg_count]!=NULL){
        arg_count++;
    }
    /* cd and exit must run in the parent process */
    if(strcmp(args[0],"cd")==0){
        return builtin_cd(args+1,arg_count-1);
    }else if(strcmp(args[0],"exit")==0){
        return -2;
    }
    /* pwd and which are forked so they can participate in output redirection */
    int is_builtin = (strcmp(args[0],"pwd")==0 || strcmp(args[0],"which")==0);
    char pathbuf[4096];
    if(!is_builtin){
        if(strchr(args[0], '/') != NULL) {
            strcpy(pathbuf, args[0]);
        }else{
            if(find_program(args[0], pathbuf) == 0) {
                dprintf(STDERR_FILENO, "error: command not found: %s\n", args[0]);
                return -1;
            }
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
        if(is_builtin){
            int result;
            if(strcmp(args[0],"pwd")==0) result = builtin_pwd();
            else result = builtin_which(args+1, arg_count-1);
            _exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
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
  int pipes[64][2];
  pid_t pids[64];
  int num_pipes = seg_count - 1;

  for(int i = 0; i < num_pipes; i++) {
    if(pipe(pipes[i]) == -1) {
      perror("pipe");
      return -1;
    }
  }
  int has_exit = 0;
  for(int i = 0; i < seg_count; i++) {
    if(strcmp(segments[i][0], "exit") == 0) { has_exit = 1; break; }
  }

  for(int i = 0; i < seg_count; i++) {
    pids[i] = fork();
    if (pids[i] == -1) {
      perror("fork");
      return -1;
    }
    if(pids[i] == 0) {
      if(i > 0) {
        dup2(pipes[i-1][0], STDIN_FILENO);
      } else if (!interactive) {
        int fd = open("/dev/null", O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);
      }
      if(i < seg_count - 1) {
        dup2(pipes[i][1], STDOUT_FILENO);
      }
      for(int j = 0; j < num_pipes; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      /* handle exit built-in in pipeline */
      if(strcmp(segments[i][0], "exit") == 0) {
        _exit(EXIT_SUCCESS);
      }
      char pathbuf[4096];
      if(strchr(segments[i][0], '/')) {
        strcpy(pathbuf, segments[i][0]);
      } else {
        if(!find_program(segments[i][0], pathbuf)) {
          dprintf(STDERR_FILENO, "error: command not found: %s\n", segments[i][0]);
          _exit(EXIT_FAILURE);
        }
      }
      execv(pathbuf, segments[i]);
      perror(segments[i][0]);
      _exit(EXIT_FAILURE);
    }
  }

  for(int i = 0; i < num_pipes; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  int last_status = 0;
  for(int i = 0; i < seg_count; i++) {
    int status;
    waitpid(pids[i], &status, 0);
    if(i == seg_count - 1) {
      last_status = status;
    }
  }
  if(has_exit) return -2;
  return last_status;
}



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
  char *args[256];
  int arg_count = 0;
  char *in_file, *out_file;
  if(count == 0) { return 1; }
  int has_pipe = 0;
  char *expanded[1024];
  count = expand_wildcards(tokens, count, expanded, 1024);
  tokens = expanded;
  for(int i = 0; i < count; i++){
    if(strcmp(tokens[i], "|") == 0) {
      has_pipe = 1;
      break;
    }
  }
  if (has_pipe) {
    char **segments[64];
    char *seg_args[64][256];
    int seg_count = 0;
    int seg_start = 0;

    for (int i = 0; i <= count; i++) {
      if (i == count || strcmp(tokens[i], "|") == 0) {
        int len = i - seg_start;
        for (int j = 0; j < len; j++) {
          seg_args[seg_count][j] = tokens[seg_start + j];
        }
        seg_args[seg_count][len] = NULL;
        segments[seg_count] = seg_args[seg_count];
        seg_count++;
        seg_start = i + 1;
      }
    }
    int status = exec_pipeline(segments, seg_count, interactive);
    if (status == -2) return 0;
    if (interactive) {
      if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        dprintf(STDOUT_FILENO, "Exited with status %d\n", WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        dprintf(STDOUT_FILENO, "Terminated by signal %d\n", WTERMSIG(status));
      }
    }
} else {
    if (!parse_redirection(tokens, count, args, &arg_count, &in_file, &out_file)) {
      return 1;
    }
    int status = exec_command(args, in_file, out_file, interactive);
    if (status == -2) return 0;
    if (interactive) {
      if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        dprintf(STDOUT_FILENO, "Exited with status %d\n", WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        dprintf(STDOUT_FILENO, "Terminated by signal %d\n", WTERMSIG(status));
      }
    }
  }
  return 1;
}



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
    int got_eof = 0;
    while(i<max-1){
        int r = read(fd,&buf[i],1);
        if(r==0){
            got_eof = 1;
            break;
        }
        if(buf[i]=='\n'){
            break;
        }
        i++;
    }
    buf[i]='\0';
    if(got_eof && i==0) return -1;
    return i;
}


int main(int argc, char *argv[]) {
    int fd = STDIN_FILENO;
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



    char line[4096];
    char *tokens[256];
    while (1) {
        print_prompt(interactive);

        int n = read_line(fd, line, sizeof(line));
        if (n == -1) break; /* EOF */

        int count = tokenize(line, tokens, 256);

        int keep_going = run_command(tokens, count, interactive);
        if (!keep_going) break;
    }

    if (interactive) write(STDOUT_FILENO, "Exiting my shell\n", 17);
    if (fd != STDIN_FILENO) close(fd);

    return EXIT_SUCCESS;
}
