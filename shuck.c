////////////////////////////////////////////////////////////////////////
// COMP1521 21t2 -- Assignment 2 -- shuck, A Simple Shell
// <https://www.cse.unsw.edu.au/~cs1521/21T2/assignments/ass2/index.html>
//
// Written by Jayden Matthews (z5360350) on 6/08/2021.
//
// 2021-07-12    v1.0    Team COMP1521 <cs1521@cse.unsw.edu.au>
// 2021-07-21    v1.1    Team COMP1521 <cs1521@cse.unsw.edu.au>
//     * Adjust qualifiers and attributes in provided code,
//       to make `dcc -Werror' happy.
//

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <errno.h>
#include <ctype.h>


//
// Interactive prompt:
//     The default prompt displayed in `interactive' mode --- when both
//     standard input and standard output are connected to a TTY device.
//
static const char *const INTERACTIVE_PROMPT = "shuck& ";

//
// Default path:
//     If no `$PATH' variable is set in Shuck's environment, we fall
//     back to these directories as the `$PATH'.
//
static const char *const DEFAULT_PATH = "/bin:/usr/bin";

//
// Default history shown:
//     The number of history items shown by default; overridden by the
//     first argument to the `history' builtin command.
//     Remove the `unused' marker once you have implemented history.
//
static const int DEFAULT_HISTORY_SHOWN __attribute__((unused)) = 10;

//
// Input line length:
//     The length of the longest line of input we can read.
//
static const size_t MAX_LINE_CHARS = 1024;

//
// Special characters:
//     Characters that `tokenize' will return as words by themselves.
//
static const char *const SPECIAL_CHARS = "!><|";

//
// Word separators:
//     Characters that `tokenize' will use to delimit words.
//
static const char *const WORD_SEPARATORS = " \t\r\n";


static void execute_command(char **words, char **path, char **environment);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);
static void change_directory(char **words);
static void print_working_dir(char **words);
static void print_history(char **words);
static void add_history(char **words);
static void nth_command(char **words, char **path, char **environment);
static void run_executable(char **words);
static void search_run_exec(char **words, char **path);
static void globber_checker(char **words, char *new_line);
static void input_redirection(char **words, char **path);
static void output_redirection(char **words, char **path, int nw);
static void pathmaker(char *pathname, char *words, char **path);
static void pipes(char **words, char **path, int numpipes);
static void in_out_redirection(char **words, char **path, int nw);

int main (void)
{
    // Ensure `stdout' is line-buffered for autotesting.
    setlinebuf(stdout);

    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    // Grab the `PATH' environment variable for our path.
    // If it isn't set, use the default path defined above.
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL) {
        pathp = (char *) DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    // Should this shell be interactive?
    bool interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

    // Main loop: print prompt, read line, execute command
    while (1) {
        // If `stdout' is a terminal (i.e., we're an interactive shell),
        // print a prompt before reading a line of input.
        if (interactive) {
            fputs(INTERACTIVE_PROMPT, stdout);
            fflush(stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL)
            break;

        // Tokenise and execute the input line.
        char **command_words =
            tokenize(line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
          
        /**************** SUBSET 3 ****************/
        //Check for words that require globbering, and expand them 
        char new_line[1024] = {0};
        globber_checker(command_words, new_line);
        
        char **new_command_words = 
        	tokenize(new_line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
        
        execute_command(new_command_words, path, environ);
        
        if (new_command_words[0][0] != '!') {
                add_history(command_words);
        }
        
        free_tokens(command_words);
        free_tokens(new_command_words);
    }

    free_tokens(path);
    return 0;
}


//
// Execute a command, and wait until it finishes.
//
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.
//
static void execute_command(char **words, char **path, char **environment)
{
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);

    char *program = words[0];

    if (program == NULL) {
        // nothing to do
        return;
    }

    if (strcmp(program, "exit") == 0) {
        do_exit(words);
        // `do_exit' will only return if there was an error.
        return;
  	}	 
  	
	/**************** SUBSET 5 ****************/
  	int numpipes = 0;
  	for(int k = 0; words[k] != NULL; k++) {
  		if (strcmp(words[k], "|") == 0) {
  			numpipes += 1;
  		}
  		if (strcmp(words[k], "|") == 0 && strcmp(words[k+1], "|") == 0) {
  			fprintf(stderr, "invalid pipe\n");
  			return;
  		}
  	}
  	
  	if (numpipes > 0) {
		pipes(words, path, numpipes);
		return;
  	}
  	
  	/**************** SUBSET 4 ****************/
  	int nw = 0; 
  	while (words[nw] != NULL) {
  		nw++;
  	}
  	nw -= 1;
  	
  	int num_outdirectors = 0;
  	for (int i = 0; words[i] != NULL; i++) {
  		if (strcmp(words[i], ">") == 0) {
  			num_outdirectors += 1;
  		}
  	}
  	
  	int num_indirectors = 0;
  	for (int i = 0; words[i] != NULL; i++) {
  		if (strcmp(words[i], "<") == 0) {
  			num_indirectors += 1;
  		}
  	}
  	
  	if (num_indirectors > 1) {
  		fprintf(stderr, "invalid input redirection\n");
  		return;
  	} else if (num_outdirectors > 2) {
  		fprintf(stderr, "invalid output redirection\n");
  		return;
  	} 
  	
  	if (num_indirectors == 1 && num_outdirectors == 0) { 
  		input_redirection(words, path);
  		return;
  		
  	} else if (num_indirectors == 0 && num_outdirectors >= 1) {
  		output_redirection(words, path, nw);
  		return;
  		
  	} else if (num_indirectors == 1 && num_outdirectors >= 1) { 
  		in_out_redirection(words, path, nw);
  		return;
  	}
  
 	 /**************** SUBSET 2 ****************/
    if (strcmp(program, "history") == 0) { //Print History
    	print_history(words);
    	return;
    	
    } else if (strcmp(program, "!") == 0) { //Print nth command in history
    	nth_command(words, path, environment);
    	return;
    }
    
   /**************** SUBSET 0 ****************/
    if (strcmp(program, "cd") == 0) { // Change directory
    	if (words[1] != NULL) {
			if (words[2] != NULL) {
				fprintf(stderr, "cd: too many arguments\n");
				return;
			}
		}
    	change_directory(words);
    	return;
    	
    } else if (strcmp(program, "pwd") == 0) { // Print working directory
    if (words[1] != NULL) {
    		fprintf(stderr, "pwd: too many arguments\n");
    		return;
    	}
    	print_working_dir(words);
    	return;	
	}
    	
   	/**************** SUBSET 1 ****************/
    if (strrchr(program, '/') == NULL) { //Search for & Execute the given command
    	search_run_exec(words, path);
    	return;
    }

    if (is_executable(program)) { //Execute the specified command 
		run_executable(words);
        return;
    } else {
        fprintf(stderr, "%s: command not found\n", program);
    }
    return;
    
}

	
//
// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words)
{
    assert(words != NULL);
    assert(strcmp(words[0], "exit") == 0);

    int exit_status = 0;

    if (words[1] != NULL && words[2] != NULL) {
        // { "exit", "word", "word", ... }
        fprintf(stderr, "exit: too many arguments\n");

    } else if (words[1] != NULL) {
        // { "exit", something, NULL }
        char *endptr;
        exit_status = (int) strtol(words[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "exit: %s: numeric argument required\n", words[1]);
        }
    }

    exit(exit_status);
}


//
// Check whether this process can execute a file.  This function will be
// useful while searching through the list of directories in the path to
// find an executable file.
//
static int is_executable(char *pathname)
{
    struct stat s;
    return
        // does the file exist?
        stat(pathname, &s) == 0 &&
        // is the file a regular file?
        S_ISREG(s.st_mode) &&
        // can we execute it?
        faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}


//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being `NULL'.
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars)
{
    size_t n_tokens = 0;

    // Allocate space for tokens.  We don't know how many tokens there
    // are yet --- pessimistically assume that every single character
    // will turn into a token.  (We fix this later.)
    char **tokens = calloc((strlen(s) + 1), sizeof *tokens);
    assert(tokens != NULL);

    while (*s != '\0') {
        // We are pointing at zero or more of any of the separators.
        // Skip all leading instances of the separators.
        s += strspn(s, separators);

        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0') {
            break;
        }

        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        size_t length = strcspn(s, separators);
        size_t length_without_specials = strcspn(s, special_chars);
        if (length_without_specials == 0) {
            length_without_specials = 1;
        }
        if (length_without_specials < length) {
            length = length_without_specials;
        }

        // Allocate a copy of the token.
        char *token = strndup(s, length);
        assert(token != NULL);
        s += length;

        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }

    // Add the final `NULL'.
    tokens[n_tokens] = NULL;

    // Finally, shrink our array back down to the correct size.
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);
    assert(tokens != NULL);

    return tokens;
}


//
// Free an array of strings as returned by `tokenize'.
//
static void free_tokens(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

//
// Change current directory to the specifed directory.
//
static void change_directory(char **words) {
	if (words[1] == NULL) { //If no specified directory, chdir to $HOME
		if (chdir(getenv("HOME")) != 0) {
			perror("chdir");
		} 
		return;
	} else {
	 	if (chdir(words[1]) != 0) {
			int error = errno;
			if (error == 20) { // Error code 20: ENOTDIR
				fprintf(stderr, "cd: %s: Not a directory\n", words[1]);
			} else if (error == 2) { // Error code 2: ENOENT
				fprintf(stderr,"cd: %s: No such file or directory\n", words[1]);
			}
			return;
		}
		
	}
	return;
}

//
// Print current directory to stdout.
//
static void print_working_dir(char **words) {
	char pathname[MAX_LINE_CHARS];
    
	if (getcwd(pathname, sizeof(pathname)) == NULL) {
		perror("getcwd");
		return;
	}
	printf("current directory is '%s'\n", pathname);
	
	return;
}

//
// Print command history, 10 commands if no specified number
//
static void print_history(char **words) {
	char history_location[100] = {0};
	strcpy(history_location, getenv("HOME"));
	strcat(history_location, "/.shuck_history");
	
	FILE *his;
	his = fopen(history_location, "r");
	
	if (his == NULL) {
		return;
	}
	
	int numcmds = 0;
	int n;
	char str[MAX_LINE_CHARS];
	char temp[MAX_LINE_CHARS][MAX_LINE_CHARS];
	
	if (words[1] != NULL) {
		n = atoi(words[1]);
		
		if(words[2] != NULL) { //Case: more than 2 arguments
			fclose(his);
			fprintf(stderr, "history: too many arguments\n");
			return;
		} else if (n == 0) { 
			if (isdigit(words[1][0]) == 0) { //Case: non-numeric argument
				fclose(his);
				fprintf(stderr, "history: %s: numeric argument required\n", words[1]);
				return;
			}
		}
			
	} else {
		n = DEFAULT_HISTORY_SHOWN;
	} 
	
	
	
	while (fgets(str, MAX_LINE_CHARS, his) != NULL) {
		strcpy(temp[numcmds], str);
		numcmds++;
	}
	
	int cmd_number = numcmds - (n);
	
	if (cmd_number < 0) {
		cmd_number = 0;
		n = numcmds;
	}
	
	for (int i = 0; i < n; i++) {
		printf("%d: %s", cmd_number, temp[cmd_number]);
		cmd_number += 1;
	}
	
	fclose(his);
	return;
}

//
// Appends a command to history.
//
static void add_history(char **words) {
	char history_location[100] = {0};
	strcpy(history_location, getenv("HOME"));
	strcat(history_location, "/.shuck_history");
	
	FILE *fp;
	fp = fopen(history_location, "a+");

	int k = 0;
	while (words[k] != NULL) {
		fputs(words[k], fp);
		if (words[k + 1] != NULL) {
			fputc(32, fp);
		}
		k++;
	}
	fputc(10, fp);
	fclose(fp);
}

//
// Print the nth command in history.
//
static void nth_command(char **words, char **path, char **environment) {
	char history_location[100] = {0};
	strcpy(history_location, getenv("HOME"));
	strcat(history_location, "/.shuck_history");
	
	FILE *his;
	his = fopen(history_location, "r");
	
	if (his == NULL) {
		return;
	}
	
	int numcmds = 0;
	int cmd_number;
	char str[MAX_LINE_CHARS];
	char temp[MAX_LINE_CHARS][MAX_LINE_CHARS];
	
	while (fgets(str, MAX_LINE_CHARS, his) != NULL) {
		strcpy(temp[numcmds], str);
		numcmds++;
	}
	
	
	if (words[1] == NULL) { //no specified number
		cmd_number = numcmds - 1;
	} else {
		cmd_number = atoi(words[1]);
		
		if(words[2] != NULL) {
			fclose(his);
			fprintf(stderr, "!: too many arguments\n");
			return;
		} else if (cmd_number == 0) {
			if (isdigit(words[1][0]) == 0) {
				fclose(his);
				fprintf(stderr, "!: %s: numeric argument required\n", words[1]);
				return;
			}
		}
	}
	
	if (numcmds - 1 < cmd_number) {
		fprintf(stderr, "!: invalid history reference\n");
		return;
	}
	
	printf("%s", temp[cmd_number]);

	//Globber checking of previous command if it has special character patterns
	char **keywords = tokenize(temp[cmd_number], (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
	char new_line[1024] = {0};
   	globber_checker(keywords, new_line);
    char **new_keywords = tokenize(new_line, (char *) WORD_SEPARATORS, (char *) SPECIAL_CHARS);
    
	execute_command(new_keywords, path, environment);
	add_history(keywords);
	free_tokens(new_keywords);
	free_tokens(keywords);
	fclose(his);
}

//
// Run an executable command, through posix_spawn
//
static void run_executable(char **words) {
	char *exec_argv[MAX_LINE_CHARS];

	exec_argv[0] = words[0];

	int i = 1;
	while(words[i] != NULL) {
		exec_argv[i] = words[i];
		i++;
	}
	exec_argv[i] = NULL;

	pid_t pid;
	extern char **environ;
	// spawn command as a separate process
	if (posix_spawn(&pid, words[0], NULL, NULL, exec_argv, environ) != 0) {
		perror("spawn");
		exit(1);
	}

	// wait for spawned processes to finish
	int exit_status;
	if (waitpid(pid, &exit_status, 0) == -1) {
		perror("waitpid");
		exit(1);
	}

	printf("%s exit status = %d\n", words[0], WEXITSTATUS(exit_status));
	return;
}

static void search_run_exec(char **words, char **path) {
	char temp[MAX_LINE_CHARS]; 
	int i = 0;
	while (path[i] != NULL) {
		strcpy(temp, path[i]);
		strcat(temp, "/");
		strcat(temp, words[0]);
		
		if (is_executable(temp)) {
			char *exec_argv[MAX_LINE_CHARS];

			exec_argv[0] = words[0];

			int j = 1;
			while(words[j] != NULL) {
				exec_argv[j] = words[j];
				j++;
			}
			exec_argv[j] = NULL;

			pid_t pid;
			extern char **environ;
			// spawn command as a separate process
			if (posix_spawn(&pid, temp, NULL, NULL, exec_argv, environ) != 0) {
				perror("spawn");
				exit(1);
			}

			// wait for spawned processes to finish
			int exit_status;
			if (waitpid(pid, &exit_status, 0) == -1) {
				perror("waitpid");
				exit(1);
			}

			printf("%s exit status = %d\n", temp, WEXITSTATUS(exit_status));
			return;
		} 
		i++;
	}
    fprintf(stderr, "%s: command not found\n", words[0]);
    return;
}

//
// Expand (glob) word patterns to all matches
//
static void globber_checker(char **words, char *new_line) {
	strcpy(new_line, words[0]);
	int i = 1;
  	while (words[i] != NULL) {
  		if (strchr(words[i], '*') != NULL || strchr(words[i], '?') != NULL || 
  			strchr(words[i], '[') != NULL || strchr(words[i], '~') != NULL) {
  			
  			glob_t matches; // holds pattern expansion
    		int glob_status = glob(words[i], GLOB_NOCHECK|GLOB_TILDE, NULL, &matches);
    		
    		if (glob_status == 0) {
    			int num_matches = (int)matches.gl_pathc;
    			
    			for (int j = 0; j < num_matches; j++) {
    				strcat(new_line, " ");
    				strcat(new_line, matches.gl_pathv[j]);
    			}				
    			
    		} else {
    			globfree(&matches);
				fprintf(stderr, "glob returned %d", glob_status);	
			}
			
			globfree(&matches);
  		} else {
  			strcat(new_line, " ");
  			strcat(new_line, words[i]);
  		}
  		i++;
  	}
}

//
// Redirect a file specified as input to a new process
//
static void input_redirection(char **words, char **path) {
	for (int i = 1; words[i] != NULL; i++) {
		if (strcmp(words[i], "<") == 0 || strcmp(words[i], ">") == 0) { // > or < appear elsewhere
			fprintf(stderr, "invalid input redirection\n");
			return;
		}
  	}	
  		
	if (words[2] == NULL) {		 	// No specified command
		fprintf(stderr, "invalid input redirection\n");
		return;
		
	}  else if (strcmp(words[2], "history") == 0 || strcmp(words[2], "!") == 0 || 
		strcmp(words[2], "cd") == 0 || strcmp(words[2], "pwd") == 0) { 	
		
		// if a builtin command is specified 		
		fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", words[2]);
		return;	
	}
	
	//check file exists/readable
	FILE *fp;
	fp = fopen(words[1], "r"); 
	if (fp == NULL) {
		fprintf(stderr, "invalid input redirection\n");
		fclose(fp);
		return;
	}
	fclose(fp);
	  		
	// create a pipe
	int pipe_file_descriptors[2];
	if (pipe(pipe_file_descriptors) == -1) {
		perror("pipe");
		return;
	}

	// create a list of file actions to be carried out on spawned process
	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0) {
		perror("posix_spawn_file_actions_init");
		return;
	}

	// tell spawned process to close unused write end of pipe
	if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[1]) != 0) {
		perror("posix_spawn_file_actions_init");
		return;
	}

	// tell spawned process to replace file descriptor 0 (stdin)
	// with read end of the pipe
	if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[0], 0) != 0) {
		perror("posix_spawn_file_actions_adddup2");
		return;
	}

	char *exec_argv[MAX_LINE_CHARS];
	
	// create a process 
	int j = 2, pos = 0;
	while(words[j] != NULL) {
		exec_argv[pos] = words[j];
		j++;
		pos++;
	}
	exec_argv[pos] = NULL;
	
	//get spawns required argv
	char pathname[MAX_LINE_CHARS]; 
	if (is_executable(words[2])){
		strcpy(pathname, words[2]);
	} else {
		int k = 0;
		while (path[k] != NULL) {
			strcpy(pathname, path[k]);
			strcat(pathname, "/");
			strcat(pathname, words[2]);
			
			if (is_executable(pathname)) {	
				break;
			}
			k++;
		}
			
	}
	
	pid_t pid;
	extern char **environ;
	if (posix_spawn(&pid, pathname, &actions, NULL, exec_argv, environ) != 0) {
		perror("spawn");
		return;
	}

	// close unused read end of pipe
	close(pipe_file_descriptors[0]);

	// create a stdio stream from write-end of pipe
	FILE *f = fdopen(pipe_file_descriptors[1], "w");
	if (f == NULL) {
		perror("fdopen");
		return;
	}

	// send some input
	FILE *read_in;
	read_in = fopen(words[1], "r");  		
	char str[MAX_LINE_CHARS];
	while (fgets(str, MAX_LINE_CHARS, read_in) != NULL) {
		fputs(str, f);
	}
	fclose(read_in);
	
	// close write-end of the pipe
	// without this sort will hang waiting for more input
	fclose(f);
	
	int exit_status;
	if (waitpid(pid, &exit_status, 0) == -1) {
		perror("waitpid");
		return;
	}
	printf("%s exit status = %d\n", pathname , WEXITSTATUS(exit_status));

	// free the list of file actions
	posix_spawn_file_actions_destroy(&actions);

	return;
}

//
// Redirect a ouput from a process to a specified file
// either appending or creating a new file.
//
static void output_redirection(char **words, char **path, int nw) {
	for (int i = 0; words[i] != NULL; i++) {	
		if (i != nw - 1 && i != nw - 2) {
			if (strcmp(words[i], "<") == 0 || strcmp(words[i], ">") == 0) { // > or < appear
				fprintf(stderr, "invalid output redirection\n");
				return;
			}
		}
	}
	
	if (strcmp(words[0], "history") == 0 || strcmp(words[0], "!") == 0 || 
		strcmp(words[0], "cd") == 0 || strcmp(words[0], "pwd") == 0) { 	// if a builtin command is specified 		
		fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", words[0]);
		return;	
	}
	
	//CHECK THAT COMMAND IS SPECIFIED
	// Find exectuable version of command
	int real_cmd = 0;
	char pathname[1024] = {0}; 
	if (is_executable(words[0])){
		real_cmd = 1;
		strcpy(pathname, words[0]);
	} else {
		int k = 0;
		while (path[k] != NULL) {
			strcpy(pathname, path[k]);
			strcat(pathname, "/");
			strcat(pathname, words[0]);
			
			if (is_executable(pathname)) {	
				real_cmd = 1;
				break;
			}
			k++;
		}				
	}
	
	if (real_cmd == 0) {
		fprintf(stderr, "invalid output redirection\n");
		return;
	}
	

  	int pipe_file_descriptors[2];
	if (pipe(pipe_file_descriptors) == -1) {
	    perror("pipe");
	    return;
	}

	// create a list of file actions to be carried out on spawned process
	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0) {
	    perror("posix_spawn_file_actions_init");
	    return;
	}

	// tell spawned process to close unused read end of pipe
	if (posix_spawn_file_actions_addclose(&actions, pipe_file_descriptors[0]) != 0) {
	    perror("posix_spawn_file_actions_init");
	    return;
	}

	// tell spawned process to replace file descriptor 1 (stdout)
	// with write end of the pipe
	if (posix_spawn_file_actions_adddup2(&actions, pipe_file_descriptors[1], 1) != 0) {
	    perror("posix_spawn_file_actions_adddup2");
	    return;
	}
	
	char *exec_argv[MAX_LINE_CHARS];
	
	// Create process argv
	int j = 0;
	while(strcmp(words[j], ">") != 0) {
		exec_argv[j] = words[j];
		j++;
	}
	exec_argv[j] = NULL;
	
	
	// create a process 
	pid_t pid;
	extern char **environ;
	if (posix_spawn(&pid, pathname, &actions, NULL, exec_argv, environ) != 0) {
	    perror("spawn");
	    return;
	}

	// close unused write end of pipe
	close(pipe_file_descriptors[1]);

	// create a stdio stream from read end of pipe
	FILE *f = fdopen(pipe_file_descriptors[0], "r");
	if (f == NULL) {
	    perror("fdopen");
	    return;
	}
	
	//APPEND CASE
	if (strcmp(words[nw - 2], ">") == 0 && strcmp(words[nw - 1], ">") == 0) {
		FILE *fp = fopen(words[nw], "a"); 
		if (fp == NULL) {
			fprintf(stderr, "invalid output redirection\n");
		}
		// read a line from read-end of pipe
		char line[MAX_LINE_CHARS];
		while (fgets(line, MAX_LINE_CHARS, f) != NULL) {
			fprintf(fp, "%s", line);
		}

		fclose(fp);
		
	} else { // NEW FILE CASE
		FILE *fp = fopen(words[nw], "w"); 
		if (fp == NULL) {
			fprintf(stderr, "invalid output redirection\n");
		}
		// read a line from read-end of pipe
		char line[MAX_LINE_CHARS];
		while (fgets(line, MAX_LINE_CHARS, f) != NULL) {
			fprintf(fp, "%s", line);
		}
		fclose(fp);
	}

	// close read-end of the pipe
	// spawned process will now receive EOF if attempts to read input
	fclose(f);

	int exit_status;
	if (waitpid(pid, &exit_status, 0) == -1) {
	    perror("waitpid");
	    return;
	}
	printf("%s exit status = %d\n", pathname, WEXITSTATUS(exit_status));

	// free the list of file actions
	posix_spawn_file_actions_destroy(&actions);

	return;
}

//
// Send input to a process and push the output to a file
//
static void in_out_redirection(char **words, char **path, int nw) {
	for (int i = 0; words[i] != NULL; i++) {	
		if (i != nw - 1 && i != nw - 2) {
			if (strcmp(words[i], ">") == 0) { 
				fprintf(stderr, "invalid output redirection\n");
				return;	
			}
		}
		if (i != 0)	{
			if (strcmp(words[i], "<") == 0) { 
				fprintf(stderr, "invalid input redirection\n");
				return;
			}
		}
	}
	
	if (strcmp(words[2], "history") == 0 || strcmp(words[2], "!") == 0 || 
		strcmp(words[2], "cd") == 0 || strcmp(words[2], "pwd") == 0) { 	// if a builtin command is specified 		
		fprintf(stderr, "%s: I/O redirection not permitted for builtin commands\n", words[0]);
		return;	
	}
	
	//CHECK THAT COMMAND IS SPECIFIED
	// Find exectuable version of command
	int real_cmd = 0;
	char pathname[1024] = {0}; 
	if (is_executable(words[2])){
		real_cmd = 1;
		strcpy(pathname, words[2]);
	} else {
		int k = 0;
		while (path[k] != NULL) {
			strcpy(pathname, path[k]);
			strcat(pathname, "/");
			strcat(pathname, words[2]);
			
			if (is_executable(pathname)) {	
				real_cmd = 1;
				break;
			}
			k++;
		}				
	}
	
	if (real_cmd == 0) {
		fprintf(stderr, "%s: command not found\n", words[2]);
		return;
	}
	
	// create an input pipe
	int input_pfd[2];
	if (pipe(input_pfd) == -1) {
		perror("pipe");
		return;
	}
	
	// create an output pipe
	int output_pfd[2];
	if (pipe(output_pfd) == -1) {
		perror("pipe");
		return;
	}
	
	// create a list of file actions to be carried out on spawned process
	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0) {
		perror("posix_spawn_file_actions_init");
		return;
	}

	// tell spawned process to close unused write end of pipe
	if (posix_spawn_file_actions_addclose(&actions, input_pfd[1]) != 0) {
		perror("posix_spawn_file_actions_init");
		return;
	}
	
	// tell spawned process to close unused read end of pipe
	if (posix_spawn_file_actions_addclose(&actions, output_pfd[0]) != 0) {
		perror("posix_spawn_file_actions_init");
		return;
	}

	// tell spawned process to replace file descriptor 0 (stdin) with read end of pipe
	if (posix_spawn_file_actions_adddup2(&actions, input_pfd[0], 0) != 0) {
		perror("posix_spawn_file_actions_adddup2");
		return;
	}
	
	// tell spawned process to replace file descriptor 1 (stdout) with write end of pipe
	if (posix_spawn_file_actions_adddup2(&actions, output_pfd[1], 1) != 0) {
		perror("posix_spawn_file_actions_adddup2");
		return;
	}
	
	char *exec_argv[MAX_LINE_CHARS];
	int j = 2;
	while (strcmp(words[j], ">") != 0) {
		exec_argv[j-2] = words[j];
		++j;
	}
	exec_argv[j-2] = NULL;
	
	pid_t pid;
	extern char **environ;
	if (posix_spawn(&pid, pathname, &actions, NULL, exec_argv, environ) != 0) {
		perror("spawn");
		return;
	}

	// close unused read end of pipe
	close(input_pfd[0]);
	// close unused write end of pipe
	close(output_pfd[1]);
	
	// create a stdio stream from write-end of pipe
	FILE *fin = fdopen(input_pfd[1], "w");
	if (fin == NULL) {
		perror("fdopen");
		return;
	}
	
	// create a stdio stream from read end of pipe
	FILE *fout = fdopen(output_pfd[0], "r");
	if (fout == NULL) {
		perror("fdopen");
		return;
	}
	
	//send input
	FILE *read_in = fopen(words[1], "r");  		
	char str[MAX_LINE_CHARS];
	while (fgets(str, MAX_LINE_CHARS, read_in) != NULL) {
		fputs(str, fin);
	}
	fclose(read_in);	
	fclose(fin);
	
	//recieve output
	if (strcmp(words[nw - 2], ">") == 0 && strcmp(words[nw - 1], ">") == 0) {
		FILE *fp = fopen(words[nw], "a"); 
		if (fp == NULL) {
			fprintf(stderr, "invalid output redirection\n");
		}
		// read a line from read-end of pipe
		char line[MAX_LINE_CHARS];
		while (fgets(line, MAX_LINE_CHARS, fout) != NULL) {
			fprintf(fp, "%s", line);
		}

		fclose(fp);
	} else {
		FILE *fp = fopen(words[nw], "w"); 
		if (fp == NULL) {
			fprintf(stderr, "invalid output redirection\n");
		}
		// read a line from read-end of pipe
		char line[MAX_LINE_CHARS];
		while (fgets(line, MAX_LINE_CHARS, fout) != NULL) {
			fprintf(fp, "%s", line);
		}
		fclose(fp);
	}

	int exit_status;
	if (waitpid(pid, &exit_status, 0) == -1) {
	    perror("waitpid");
	    return;
	}
	printf("%s exit status = %d\n", pathname, WEXITSTATUS(exit_status));


	// free the list of file actions
	posix_spawn_file_actions_destroy(&actions);
	return;
}

//
// Creates a file path to an exectuable command 
//
static void pathmaker(char *pathname, char *words, char **path) {
	if (is_executable(words)){
		strcpy(pathname, words);
	} else {
		int k = 0;
		while (path[k] != NULL) {
			strcpy(pathname, path[k]);
			strcat(pathname, "/");
			strcat(pathname, words);
			
			if (is_executable(pathname)) {	
				break;
			}
			k++;
		}	
	}
}

//
// Deals with commands which have "|" pipes.
//
static void pipes(char **words, char **path, int numpipes) {
	int cmd = 0;
	int j = 0;
	char subcmd[MAX_LINE_CHARS][MAX_LINE_CHARS];
	char output[10000][MAX_LINE_CHARS];
	
	while (cmd < numpipes + 1) {
		int pos = 0;
		while(words[j] != NULL && strcmp(words[j], "|") != 0) {
			strcpy(subcmd[pos], words[j]);
			pos++;
			j++;
		}
		strcpy(subcmd[pos], "\0");
		
		int num_w = 0; 
	  	while (strcmp(subcmd[num_w], "\0") != 0) {
	  		num_w++;
	  	}
		
		for (int i = 0; strcmp(subcmd[i], "\0") != 0; i++) {	
			if (i != num_w - 2 && i != num_w - 3) {
				if (strcmp(subcmd[i], ">") == 0) { 
					fprintf(stderr, "invalid output redirection\n");
					return;	
				}
			}
			if (i != 0)	{
				if (strcmp(subcmd[i], "<") == 0) { 
					fprintf(stderr, "invalid input redirection\n");
					return;
				}
			}
		}
		
		// create an input pipe
		int input_pfd[2];
		if (pipe(input_pfd) == -1) {
			perror("pipe");
			return;
		}
		
		// create an output pipe
		int output_pfd[2];
		if (pipe(output_pfd) == -1) {
			perror("pipe");
			return;
		}
		
		// create a list of file actions to be carried out on spawned process
		posix_spawn_file_actions_t actions;
		if (posix_spawn_file_actions_init(&actions) != 0) {
			perror("posix_spawn_file_actions_init");
			return;
		}

		// tell spawned process to close unused write end of pipe
		if (posix_spawn_file_actions_addclose(&actions, input_pfd[1]) != 0) {
			perror("posix_spawn_file_actions_init");
			return;
		}
		
		// tell spawned process to close unused read end of pipe
		if (posix_spawn_file_actions_addclose(&actions, output_pfd[0]) != 0) {
			perror("posix_spawn_file_actions_init");
			return;
		}

		// tell spawned process to replace file descriptor 0 (stdin) with read end of pipe
		if (posix_spawn_file_actions_adddup2(&actions, input_pfd[0], 0) != 0) {
			perror("posix_spawn_file_actions_adddup2");
			return;
		}
		
		// tell spawned process to replace file descriptor 1 (stdout) with write end of pipe
		if (posix_spawn_file_actions_adddup2(&actions, output_pfd[1], 1) != 0) {
			perror("posix_spawn_file_actions_adddup2");
			return;
		}
		
			
		char *exec_argv[MAX_LINE_CHARS];
		char pathname[MAX_LINE_CHARS];
		
		if (cmd == 0) { // first command
			if (strcmp(subcmd[0], "<") == 0) {
				// input file
				int p = 2; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p-2] = subcmd[p];
					p++;
				}
				exec_argv[p-2] = NULL;
				
				pathmaker(pathname, subcmd[2], path);
				
			} else if (is_executable(subcmd[0])){
				// running executable 
				int p = 0; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
				strcpy(pathname, subcmd[0]);
			} else if (strrchr(subcmd[0], '/') == NULL) {
				//search and exec
				int p = 0; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
				
				pathmaker(pathname, subcmd[0], path);
			}
			
		} else if (cmd == numpipes) {// Last command
			  	
		  	if (strcmp(subcmd[num_w - 2], ">") == 0) {
		  		//output redirection
		  		int p = 0; 
				while(strcmp(subcmd[p], ">") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
		  		pathmaker(pathname, subcmd[0], path);
		  	} else if (is_executable(subcmd[0])){
				// running executable 
				int p = 0; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
				strcpy(pathname, subcmd[0]);
			} else if (strrchr(subcmd[0], '/') == NULL) {
				//search and exec
				int p = 0; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
			
				pathmaker(pathname, subcmd[0], path);
			}
			  	
		} else { //Inner command
			if (is_executable(subcmd[0])){
				// running executable 
				int p = 0; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
				strcpy(pathname, subcmd[0]);
			} else if (strrchr(subcmd[0], '/') == NULL) {
				//search and exec
				int p = 0; 
				while(strcmp(subcmd[p], "\0") != 0) {
					exec_argv[p] = subcmd[p];
					p++;
				}
				exec_argv[p] = NULL;
				
				pathmaker(pathname, subcmd[0], path);
			}

		}
				
		pid_t pid;
		extern char **environ;
		if (posix_spawn(&pid, pathname, &actions, NULL, exec_argv, environ) != 0) {
			perror("spawn");
			return;
		}

		// close unused read end of pipe
		close(input_pfd[0]);
		// close unused write end of pipe
		close(output_pfd[1]);
		
		// create a stdio stream from write-end of pipe
		FILE *fin = fdopen(input_pfd[1], "w");
		if (fin == NULL) {
			perror("fdopen");
			return;
		}
		
		// create a stdio stream from read end of pipe
		FILE *fout = fdopen(output_pfd[0], "r");
		if (fout == NULL) {
			perror("fdopen");
			return;
		}
			
		// SEND INPUT
		if (cmd == 0) {
			if (strcmp(subcmd[0], "<") == 0) {
				FILE *read_in = fopen(subcmd[1], "r");  		
				char str[MAX_LINE_CHARS];
				while (fgets(str, MAX_LINE_CHARS, read_in) != NULL) {
					fputs(str, fin);
				}
				fclose(read_in);
			}	
			
		} else {
			int s = 0;
			while (strcmp(output[s], "\0") != 0) {
				fputs(output[s], fin);
				++s;
			}
		}
	
		// close write-end of the pipe
		// without this sort will hang waiting for more input
		fclose(fin);
		
		// SEND OUTPUT
		if (cmd == numpipes) {
			if (strcmp(subcmd[num_w - 2], ">") == 0 && strcmp(subcmd[num_w - 3], ">") == 0) {
				FILE *f_out = fopen(subcmd[num_w - 1], "a");
				char line[MAX_LINE_CHARS];
				while (fgets(line, MAX_LINE_CHARS, fout) != NULL) {
					fprintf(f_out, "%s", line);
				}
				fclose(f_out);
			} else if (strcmp(subcmd[num_w - 2], ">") == 0) {
				FILE *f_out = fopen(subcmd[num_w - 1], "w");
				char line[MAX_LINE_CHARS];
				while (fgets(line, MAX_LINE_CHARS, fout) != NULL) {
					fprintf(f_out, "%s", line);
				}
				fclose(f_out);
			} else {
				char line[MAX_LINE_CHARS];
				while (fgets(line, MAX_LINE_CHARS, fout) != NULL) {
					printf("%s", line);
				}
			}	
			
		} else {
			char line[MAX_LINE_CHARS];
			int s = 0;
			while (fgets(line, MAX_LINE_CHARS, fout) != NULL) {
				strcpy(output[s], line);
				++s;
			}
			strcpy(output[s], "\0");
		
		}

		fclose(fout);

		int exit_status;
		if (waitpid(pid, &exit_status, 0) == -1) {
			perror("waitpid");
			return;
		}
		
		if (cmd == numpipes) {
			printf("%s exit status = %d\n", pathname, WEXITSTATUS(exit_status));
		}

		// free the list of file actions
		posix_spawn_file_actions_destroy(&actions);

		cmd++;
		j++;
	}
}
