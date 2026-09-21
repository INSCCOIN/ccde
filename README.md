# ccde

Small C IDE for the SharkDeck (framebuffer). Editor + `make`/`gcc` + error jump + run.

```bash
cd /home/working/ccde
rm -f *.o ccde
make
./ccde
./ccde /home/working/cNote/note.c
```

Opens the project directory of the file you pass (or `/home/working`). **File → Open** lists `.c` / `.h` / `Makefile` there.

Build uses `Makefile` if present, otherwise `gcc -O2 -Wall -Wextra -o <dirname> *.c -lm`.

| | |
|--|--|
| Esc | File / Build / Settings tabs |
| Ctrl+S | save |
| Ctrl+O | open picker |
| **F5** | build |
| **F6** | run binary |
| Enter on an error line | jump to `file:line` |
| Tab | indent in editor; switch panes in output |
| Ctrl+X | quit (save prompt if dirty) |

Needs `/dev/fb0`.
