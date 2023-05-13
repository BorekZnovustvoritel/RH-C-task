# Logging Deamon in C

## How to use:
- Clone the repository, `cd` to into it.
- `sudo systemctl stop rsyslog`
- `sudo systemctl stop systemd-journald`
- `gcc main.c -o main`
- `sudo ./main /tmp/log1 /tmp/log2` (or any other locations, add as many as you want)
- Open another terminal window.
- Send messages from the other window via the `logger message` command.
- When you want to stop the program, stop it with `Ctrl+C`. Message count and the most used message will be displayed.
- **NOTE:** The exit message is also the part of the counted message.
