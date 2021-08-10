# Slodo
A simple, xcb based, linux todo application

Additionally 2 scripts are included for opening/highlighting and closing the todo application, which allows for opening the application with shortcuts

# How to use
## Compilation
Before compiling modify the `TODO_PATH` definition in sloco.c, then compile with `make`

## Normal mode
* j, k move between selected line
* d sets completion, pressing d again on a completed line removes it
* shift + j, k switches around the todo lines
* o enters insert mode
## Insert mode
* Enter exits insert mode

# Dependencies
* xcb
* X11
* xcb-keysyms

# TODO
* Implement support for modifying existing line and removing completion status
* Refactor code into being simpler
