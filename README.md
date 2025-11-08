# Wipro-Capstone-Project
# File Explorer Application (Linux)

## Overview
This project is a Linux-based terminal file explorer built using C++.  
It provides a command-line interface to manage files and directories similar to common Linux utilities like `ls`, `cp`, `mv`, `rm`, and `tree`.

Users can list directories, create files/folders, delete, copy, move, view file contents, change permissions, measure disk usage and more — all from a single executable.

## Features

- Colored directory listing  
- Recursive delete (rmr)  
- Recursive copy (cpr)  
- Disk usage calculation (du)  
- Tree structure view (tree)  
- View file contents (head, tail, cat)  
- Create and edit files  
- Detailed file info including permissions, owner, group, size, timestamp  
- Search files by name  
- Change file permissions (chmod)

## Requirements

- Linux / WSL / Ubuntu  
- g++ compiler supporting C++17  

## How to Compile

Open terminal inside the project folder and run:

g++ -std=gnu++17 -O2 -Wall -Wextra -o filex main.cpp

## How to Run

./filex

## Useful Commands inside the Explorer

| Command | Description |
|--------|-------------|
| ls [path] | list directory contents |
| cd \<path\> | change directory |
| mkfile \<name\> | create a file |
| mkdir \<name\> | create a directory |
| rm \<file\> | delete file |
| rmdir \<dir\> | delete empty directory |
| rmr \<path\> | delete recursively |
| cp \<src\> \<dst\> | copy file |
| cpr \<src\> \<dst\> | copy recursively |
| mv \<src\> \<dst\> | rename/move |
| chmod \<octal\> \<file\> | change permission (e.g., chmod 755 a.txt) |
| head \<file\> [n] | first n lines |
| tail \<file\> [n] | last n lines |
| cat \<file\> | view entire file |
| write \<file\> | append text to file (type END to close) |
| search \<pattern\> [dir] | recursive search |
| du [path] | disk usage of folder |
| tree [path] [depth] | directory structure |
| help | show all commands |
| exit | quit program |

## Example Usage Flow

./filex
pwd
ls
mkfile test.txt
write test.txt
END
head test.txt
mkdir testDir
cpr test.txt testDir/copy.txt
tree .
du .
exit
