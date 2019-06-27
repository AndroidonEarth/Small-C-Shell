# Small-C-Shell
A simple shell program written with C. An exercise in process management.

# Synopsis
A basic shell to run command line instructions and return results similar to other shells, but without many of their fancier features.
 
Allows for redirection of standard input and standard output, and supports both foreground and background processes (controllable by the command line and by receiving signals).

Supports three built-in commands: 

    exit
    cd
    status
    
Any commands are simply executed using execvp().

Also supports comments, which are lines beginning with the # character.

# Instructions
Compile the run program with the command line:
 
     gcc -o smallsh smallsh.c
 
 To run the shell, use the command:
 
     smallsh
 
 To exit the shell, use the command:
 
     exit
     
 # Description
 The colon : symbol is the prompt for each command line. The general syntax of a command line is (items in square brackets are optional):
 
    command [arg1 arg2 ...] [< input_file] [> output_file] [&]

# Command line rules
1.) The command must be made up of words separated by spaces.

2.) The special symbols <, >, and & are recognized, but they must be surrounded by spaces like other words.

3.) If the command is to be executed in the background, the last word must be &.

4.) If the & character appears anywhere else, it is treated like normal text.

5.) If the standard input or output is to be redirected, the > or < words followed by a filename word must appear after all the arguments. 

6.) Input redirection can appear before or after output redirection.

7.) Does not support any quoting - arguments with spaces in them are not possible.

8.) The common shell operater pipe | is also not available.

9.) Supports command lines with a maximum length of 2048 characters, and a maximum of 512 arguments.

10.) No error checking is performed on the syntax of the command line.

# Blank lines and comments

Any line that begins with a # character is a comment and is ignored.
 
A blank line does nothing.
 
If a blank line or a comment line is received, the shell simply re-prompts for another command.
 
