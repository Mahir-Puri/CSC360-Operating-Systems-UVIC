# Simple Shell Interpreter (SSI)

A Unix shell written in C, built on the `fork`/`exec`/`wait` pattern with foreground and background process management, signal handling, and GNU readline integration.

## Build

Requires the GNU readline library (`libreadline-dev` on Debian/Ubuntu, `readline` via Homebrew on macOS).

```sh
make
```

This produces an executable named `ssi`.

## Usage

```sh
./ssi
```

The prompt format is:

```
username@hostname: /current/working/directory >
```

### Built-in commands

| Command | Description |
|---|---|
| `cd [path]` | Change directory. No argument or `~` goes to `$HOME`. Supports relative and absolute paths. |
| `bg <cmd> [args]` | Run a command in the background. Shell returns to prompt immediately. |
| `bglist` | List all currently running background jobs with their PIDs. |

### Keyboard shortcuts

| Key | Behaviour |
|---|---|
| `Ctrl-D` | Exit the shell (only on an empty prompt). |
| `Ctrl-C` | Kill the current foreground process. If nothing is running, print a new prompt. |

### Example session

```
mahir@hostname: /home/mahir > ls -lh
...
mahir@hostname: /home/mahir > bg ping -c 10 1.1.1.1
mahir@hostname: /home/mahir > bglist
1234:  ping -c 10 1.1.1.1
Total Background jobs:  1
mahir@hostname: /home/mahir > cd /tmp
mahir@hostname: /tmp > ^D
```

## Implementation notes

- **Foreground execution** — `fork()` + `execvp()` + `waitpid()`. The child resets `SIGINT` to `SIG_DFL` so `Ctrl-C` terminates it, while the parent's handler keeps the shell alive.
- **Background execution** — same fork/exec path, but the parent returns immediately after recording the job. Background job termination is detected via `waitpid(WNOHANG)` at the top of each prompt loop.
- **`cd` built-in** — implemented in the shell process itself using `chdir()`. A separate `cd` program cannot change the calling shell's directory.
- **Readline** — provides line editing, history navigation (`↑`/`↓`), and correct `Ctrl-D`/`Ctrl-C` interaction.

## Cleanup

```sh
make clean
```
