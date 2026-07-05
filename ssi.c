/*
 * ssi.c - Simple Shell Interpreter
 * CSC 360 Assignment 2
 *
 * A basic shell that runs commands in the foreground and background,
 * handles cd, and deals with Ctrl-C and Ctrl-D properly.
 *
 * I used the fork/exec/wait pattern from lecture and the man pages
 * for waitpid, signal, and readline to figure out the details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_ARGS 128
#define MAX_CMDLEN 1024
#define MAX_BG_JOBS 128

/*
 * struct to store each background job's PID and the command string.
 * I used an array of these since the number of bg jobs is unknown at compile time
 * but 128 is more than enough.
 */
typedef struct
{
    pid_t pid;
    char cmdline[MAX_CMDLEN];
} BgJob;

static BgJob bg_jobs[MAX_BG_JOBS];
static int num_bg_jobs = 0;

/*
 * track the PID of the currently running foreground child.
 * the signal handler needs this to know if something is running or not.
 */
static pid_t fg_pid = -1;

/*
   Signal handling
*/

/*
 * Ctrl-C sends SIGINT to the whole terminal process group (learned this
 * from the lecture on signals and process groups). Both the shell and
 * its child would normally die. The trick is:
 *   - shell installs this handler so it doesn't die
 *   - child calls signal(SIGINT, SIG_DFL) before exec so it DOES die
 *
 * If nothing is running in the foreground, just print a fresh prompt.
 */
static void sigint_handler(int sig)
{
    (void)sig;
    if (fg_pid > 0)
    {
        kill(fg_pid, SIGINT);
    }
    else
    {
        write(STDOUT_FILENO, "\n", 1);
        rl_on_new_line();
        rl_replace_line("", 0);
        rl_redisplay();
    }
}

/*
   Prompt
*/

/*
 * builds the prompt in the format: username@hostname: /cwd >
 * using getcwd, getlogin, and gethostname as suggested in the assignment.
 * getlogin() can return NULL over SSH so I fall back to the USER env var.
 */
static void build_prompt(char *buf, size_t buflen)
{
    char hostname[256];
    char cwd[1024];
    char *username;

    username = getlogin();
    if (username == NULL)
        username = getenv("USER");
    if (username == NULL)
        username = "unknown";

    if (gethostname(hostname, sizeof(hostname)) != 0)
        strncpy(hostname, "unknown", sizeof(hostname) - 1);

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strncpy(cwd, "?", sizeof(cwd) - 1);

    // I had to google this snprintf logic since I wasn't sure how to do it safely. snprintf writes at most buflen-1 chars and always null-terminates.
    snprintf(buf, buflen, "%s@%s: %s > ", username, hostname, cwd);
}

/*
   Input parsing
 */

/*
 * splits the input into tokens using strtok.
 * strtok modifies the string it's given, which is why main passes it
 * a copy of the line rather than the original readline buffer.
 * args[] is NULL-terminated so execvp can use it directly.
 */
static int parse_line(char *line, char **args)
{
    int count = 0;
    char *token;

    token = strtok(line, " \t\n");
    while (token != NULL && count < MAX_ARGS - 1)
    {
        args[count++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[count] = NULL;
    return count;
}

/*
   cd built-in
 */

/*
 * cd has to be a shell built-in because exec replaces the current process
 * image - even if there was a 'cd' program it would change directory in
 * the child process, not in the shell itself. This was discussed in lecture.
 *
 * chdir() does all the work. No argument or "~" means go to $HOME,
 * which I get from getenv("HOME") as suggested in the assignment.
 * Extra arguments after the first are just ignored.
 */
static void handle_cd(char **args)
{
    const char *target_dir;

    if (args[1] == NULL || strcmp(args[1], "~") == 0)
    {
        target_dir = getenv("HOME");
        if (target_dir == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
    }
    else
    {
        target_dir = args[1];
    }

    if (chdir(target_dir) != 0)
        perror("cd");
}

/*
    Foreground execution
*/

/*
 * This is the fork/exec/wait pattern from lecture (slide with the diagram
 * showing parent waiting while child calls exec).
 *
 * fork() gives us 0 in the child and the child's PID in the parent.
 * The child calls execvp which replaces its process image with the program.
 * execvp does PATH lookups automatically (that's what the 'p' stands for).
 * If execvp returns at all, it failed.
 *
 * The waitpid loop handles the case where a signal (SIGINT) interrupts
 * the wait - EINTR means "try again", anything else is a real error.
 */

static void run_foreground(char **args)
{
    pid_t child_pid;
    int exit_status;
    child_pid = fork();
    if (child_pid < 0)
    {
        perror("fork");
        return;
    }
    if (child_pid == 0)
    {
        // in the child: restore SIGINT to default so Ctrl-C kills it
        signal(SIGINT, SIG_DFL);
        execvp(args[0], args);
        // only reaches here if execvp failed
        fprintf(stderr, "%s: %s\n", args[0], strerror(errno));
        exit(1);
    }
    // in the parent: record the child PID for the signal handler
    fg_pid = child_pid;
    while (waitpid(child_pid, &exit_status, 0) == -1)
    {
        if (errno != EINTR)
            break;
    }
    fg_pid = -1;
}

/*
   Background job list helpers
*/
// adds a new job to the bg_jobs array
static void add_bg_job(pid_t pid, const char *cmdline)
{
    if (num_bg_jobs >= MAX_BG_JOBS)
    {
        fprintf(stderr, "ssi: too many background jobs\n");
        return;
    }
    bg_jobs[num_bg_jobs].pid = pid;
    strncpy(bg_jobs[num_bg_jobs].cmdline, cmdline, MAX_CMDLEN - 1);
    bg_jobs[num_bg_jobs].cmdline[MAX_CMDLEN - 1] = '\0';
    num_bg_jobs++;
}

/* removes job at index idx by shifting everything after it left */
static void remove_bg_job(int idx)
{
    int i;
    for (i = idx; i < num_bg_jobs - 1; i++)
        bg_jobs[i] = bg_jobs[i + 1];
    num_bg_jobs--;
}

/*
   Background execution
 */

/*
 * same as foreground but the parent doesn't call waitpid - it just
 * records the job and goes back to the prompt immediately. (what i understood from the assignment specification and the lecture was that the child should still call execvp and run the command, just the parent doesn't wait for it)
 */

static void run_background(char **args, const char *cmdline)
{
    pid_t child_pid;
    child_pid = fork();
    if (child_pid < 0)
    {
        perror("fork");
        return;
    }
    if (child_pid == 0)
    {
        signal(SIGINT, SIG_DFL);
        execvp(args[0], args);
        fprintf(stderr, "%s: %s\n", args[0], strerror(errno));
        exit(1);
    }
    add_bg_job(child_pid, cmdline);
}

/* prints all currently running background jobs */
static void show_bglist(void)
{
    for (int i = 0; i < num_bg_jobs; i++)
        printf("%d:  %s\n", bg_jobs[i].pid, bg_jobs[i].cmdline);
    printf("Total Background jobs:  %d\n", num_bg_jobs);
}

/*
 * called at the top of every loop iteration to check if any background
 * jobs have finished. WNOHANG means "don't block, just return 0 if the
 * child hasn't exited yet". I read about this in the waitpid man page.
 */

static void check_bg_jobs(void)
{
    int exit_status;
    pid_t finished_pid;
    for (int i = 0; i < num_bg_jobs;)
    {
        finished_pid = waitpid(bg_jobs[i].pid, &exit_status, WNOHANG);
        if (finished_pid > 0)
        {
            printf("%d: %s has terminated.\n",
                   bg_jobs[i].pid, bg_jobs[i].cmdline);
            remove_bg_job(i);
        }
        else
        {
            i++;
        }
    }
}

/*
Main loop: print prompt, read line, parse, execute, repeat. Also check for finished background jobs at the start of each iteration.
*/

int main(void)
{
    char prompt[1600];
    char input_copy[MAX_CMDLEN];
    char bg_cmdline[MAX_CMDLEN];
    char *args[MAX_ARGS];
    char *input_line;
    int num_args;

    // install our SIGINT handler so Ctrl-C doesn't kill the shell
    signal(SIGINT, sigint_handler);

    while (1)
    {
        // check for finished background jobs before showing the prompt
        check_bg_jobs();
        build_prompt(prompt, sizeof(prompt));
        input_line = readline(prompt);
        // this is what the exit would be when readline returns null on ctrl-d at an empty line
        if (input_line == NULL)
        {
            printf("\n");
            break;
        }
        if (input_line[0] == '\0')
        {
            free(input_line);
            continue;
        }
        add_history(input_line);
        strncpy(input_copy, input_line, MAX_CMDLEN - 1);
        input_copy[MAX_CMDLEN - 1] = '\0';
        free(input_line);

        num_args = parse_line(input_copy, args);
        if (num_args == 0)
            continue;
        if (strcmp(args[0], "cd") == 0)
        {
            handle_cd(args);
        }
        else if (strcmp(args[0], "bglist") == 0)
        {
            show_bglist();
        }
        else if (strcmp(args[0], "bg") == 0)
        {
            if (args[1] == NULL)
            {
                fprintf(stderr, "bg: missing command\n");
            }
            else
            {
                // build the cmdline string (everything after "bg") to store in bg_jobs
                bg_cmdline[0] = '\0';
                for (int i = 1; args[i] != NULL; i++)
                {
                    if (i > 1)
                        strncat(bg_cmdline, " ", MAX_CMDLEN - strlen(bg_cmdline) - 1);
                    strncat(bg_cmdline, args[i], MAX_CMDLEN - strlen(bg_cmdline) - 1);
                }
                run_background(args + 1, bg_cmdline);
            }
        }
        else
        {
            run_foreground(args);
        }
    }
    return 0;
}