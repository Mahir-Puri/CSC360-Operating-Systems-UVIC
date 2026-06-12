CSC 360 Assignment 2 (P1) - Simple Shell Interpreter (SSI)

Build

    make

This produces an executable called "ssi" in the current directory.

Features implemented
--------------------

1. Prompt (works)
   Shows: username@hostname: /current/working/directory >
   The prompt updates every time the directory changes (e.g. after cd).

2. Foreground execution (works)
   External commands like ls, pwd, date, uname -a are all supported.
   The shell uses fork() and execvp() and waits for the command to finish.
   If the program doesn't exist it prints:
       <name>: No such file or directory

3. cd built-in (works)
   cd          -> goes to home directory ($HOME)
   cd ~        -> same as above
   cd <path>   -> works with absolute and relative paths
   cd ..       -> goes up one directory
   cd ../..    -> goes up two directories
   Only the first argument is used, extra ones are ignored.

4. bg command (works)
   "bg <command> [args]" starts the command in the background.
   The shell immediately returns to the prompt while it runs.

5. bglist command (works)
   Shows all background jobs that are still running:
       <pid>:  <command and args>
       Total Background jobs:  <count>

6. Background job termination notices (works)
   Each time the user enters a command, the shell checks if any
   background jobs have finished (using waitpid with WNOHANG).
   If one has finished it prints:
       <pid>: <command and args> has terminated.
   That job is then removed from bglist.

7. EOF / Ctrl-D (works)
   Pressing Ctrl-D on an empty prompt exits the shell cleanly.
   If there's text already typed, Ctrl-D is ignored (readline handles this).

8. Ctrl-C / SIGINT (works)
   If a foreground program is running, Ctrl-C kills it and brings back
   the prompt. The shell itself does not exit.
   If nothing is running, Ctrl-C just prints a new prompt on a new line.
