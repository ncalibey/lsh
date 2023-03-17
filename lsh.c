#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lsh.h"

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"

int main(int argc, char **argv) {
  // Load config files, if any.

  // Run command loop.
  lsh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}

void lsh_loop(void) {
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    // Read the command from STDIN.
    line = lsh_read_line();
    // Parse the command string into a program and arguments.
    args = lsh_split_line(line);
    // Execute the parsed command.
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

char *lsh_read_line(void) {
  char *line = NULL;
  ssize_t bufsize = 0; // Have getline allocate a buffer for us;

  // Cast is safe since `bufsize` is 0.
  if (getline(&line, (size_t *)&bufsize, stdin) == -1) {
    if (feof(stdin))
      exit(EXIT_SUCCESS); // We received an EOF.
    perror("readline");
    exit(EXIT_FAILURE);
  }

  return line;
}

// Some simplifications:
//   * No quoting or backslash escaping
//   * Whitespace separator for arguments
char **lsh_split_line(char *line) {
  int bufsize = LSH_TOK_BUFSIZE;
  char **tokens = malloc(bufsize * sizeof(char *));
  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  int position = 0;
  // Returns pointers to within the string, each of which has a null character
  // at the end. These are our tokens.
  char *token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    // Reallocate in case we go beyond default token buffer size.
    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char *));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, LSH_TOK_DELIM);
  }
  // Null-terminate the list of tokens.
  tokens[position] = NULL;
  return tokens;
}

int lsh_launch(char **args) {
  pid_t pid = fork();
  if (pid == 0) {
    // Child process. Use `exec` to replace self with a new program.
    // For `execvp` we provide:
    //   * Program name
    //   * Vector (array) of args (hence `v`)
    // The `p` means that the OS finds the program in the PATH instead of
    // us providing it.
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE); // Exit so shell can keep running.
  } else if (pid < 0) {
    // Error forking. User decides what to do, so no `exit`.
    perror("lsh");
  } else {
    // Parent process. Use `wait` to monitor children processes.
    int status;
    do {
      pid_t wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}

// List of builtin commands and corresponding functions.

char *builtin_str[] = {"cd", "help", "exit"};

// Array of function pointers where the functions take an array of strings and
// return an int.
int (*builtin_func[])(char **) = {&lsh_cd, &lsh_help, &lsh_exit};

int lsh_num_builtins() { return sizeof(builtin_str) / sizeof(char *); }

// Builtin implementations.

int lsh_cd(char **args) {
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}

int lsh_help(char **args) {
  puts("Stephen Brennan's LSH");
  puts("Type program names and arguments, and hit enter.");
  puts("The following are built in:");

  for (int i = 0; i < lsh_num_builtins(); i++) {
    printf("   %s\n", builtin_str[i]);
  }

  puts("Use the man command for information on other programs.");
  return 1;
}

int lsh_exit(char **args) { return 0; }

int lsh_execute(char **args) {
  if (args[0] == NULL)
    return 1;

  for (int i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0)
      return (*builtin_func[i])(args);
  }
  return lsh_launch(args);
}
