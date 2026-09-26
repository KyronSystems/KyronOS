# Terminal

The terminal is text-only and will use VGA-compatible output with a blue primary palette, magenta highlights, and cyan information. The shell supports `help`, `clear`, `version`, `about`, `echo`, `ls`, `cd`, `pwd`, `go back`, `go home`, `go root`, `add user`, `switch user`, `cat`, `mkdir`, `rmdir`, `touch`, `write`, `append`, `rm`, `devices`, `mem`, `reboot`, and `shutdown`.

Users have home directories under `/home`. The active user's home displays as `~`, so a prompt can appear as `[kyron]:~#` or `[kyron]:~/projects#`. New accounts are created with `add user <name>` and selected with `switch user <name>`.

The current bootstrap only provides console output; keyboard input and command dispatch are deliberately not represented as completed functionality.
