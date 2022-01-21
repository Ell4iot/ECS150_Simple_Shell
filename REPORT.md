# ECS 150 P1: Simple Shell

Yuhang Wang,  Shuyao Li

## Summary

This program  `simple shell` accepts input from the user under the form of command lines and exectues them. 

### Group work

This assignment is done my me(Yuhang Wang) and my partner(Shuyao Li). We followed the suggested work phase to implement the shell. I did the parts from phase 0 to phase 5, and my partner did phase 6 (extra features), code style checking and the phase 3 (built-in commands). My partner has tried to implement pipe using recursion, and we finally use my version of pipe. (phase 5)

## Implementation

### limitation

The complete message printed for pipe commands shows 

"+ completed ' something' [0]|[0]" even though one of the commands fails.

### Parsing arguments

#### Data structure

When executing a command, we need to access several information, such as

- the command itself, such as `echo` and corresponding arguments.
- if redirection output needed, we need an array containing the name of file to redirect to.
- the number of arguments as well as the file to redirect to.

Thus, as the professor suggested, I use `struct` to create a type named `cmd_info` containing all the information of the command. For example, `command.pass_argument` is a 2D array of commnd's arguments. Then, I create an array of command for possible commands number up to 4.

#### Using strtok to parse

I use the function `strtok()`to break the whole cmd from`fgets` into tokens sepearted by the chosen delimiter. The raw command may contains `|`,`>`, or white space . I choose to use  `strtok()` three times to separate them respectively.

Assuming we have a raw cmd `echo helloworld |grep Hello`, 

Firstly, I store the results of string after splited from `|` into an array. The array[0] for this example will be `echo helloworld`, Then, for each element in the array, I call `strtok()`to further split them from `>`. If no `>` is in the string, it will be passed to next stage without anychange.

Finally, I call  `strtok()` to delete white space for every token. During the process, I detect the possible parsing error, such as no output file after `>`, etc. After deleting white space, I shall put the token into the coreesponding location inside the struct object For example, `strcpy(command[0].pass_argument[0],  "echo");`

Allocated memory will be freed either when an error occurs and the program jump to next cycle of receiving commands,  or be freed after executing the commands.

Here is the basic structure of parsing arguments:

```c
array_1 = split(); // split from '|', split contains strtok()

for (int i = 0; i < total_command_no; i++) {
	 array_2 = split (array_1[i]) // split from '>'
			
	 for (j in array_2 ){
		   array_3 = split(array_2[j]) // split from ' '
			 // store information into the command[i]
	}
		
}
```

### Executing commands

As the ppt stated, we create a fork, and let parent receives the return value and waits for the children to execute the commands . We use `execvp()`, such that the command can be passed in pure command such as `echo`, instead of a command involving the detailed address.

### Built-in commands

Built-in commands can be implemented in a couple of lines of codes. Since built-in commands can have at most 2 arguments and won`t be input with pipeline or redirection, I put the built-in commands detection before the parsing process.

### Output redirection

#### parsing

Only the token right after the `>` is the filename. For example, 

`echo helloworld >file1 arg2 > file2 arg3`

arg2 and arg3 should be treaded as arguments with commands `echo`, I have also implemented this feature.

#### redirection

After testing the output redirection feature from the reference shell, I find that only the last file in the multiple output redirection will be redirected to. Thus, I create a loop to iterate over through my array which contains the filename for multiple output redirection. 

In every iteration, it will open a file and detect whether it is the last file or not (since we have to redirect the output to last file). If it is the last file, I will use `dup2(fd, STDOUT_FILENO)` to direct the output.

The redirection needs to be performed in a child process under the parent shell process. Since we definitely want to go back to `stdout` and prints `sshell:` in the next iteration to receive the next commands. After successfully call `dup2` , the child will terminated and the parent will continue to work.

With that being in mind, I choose to put `execvp()` at the same place with `dup2()` for commands which need to redirect their output. We can do 2 things in 1 fork.

```c
int pid = fork();
if (child) {
  dup2(fd, stdout); // redirect my output
  execvp();   // execute the commands
} else {
  waitpid();   // wait for child 
}
```

### Pipeline commands

As stated in the prompt, we have up to 4 commands with three `|`. In the class, we learned that we can use fork to create a pipe for two commans and concurrently tranmit input or output. Then, I decided to use 2 forks to create 4 process, each represents a command.  Thus, all 3 pipes can be open and close concurrently.

```c
pipe(fd_23)    // open pipe between commands 2 and 3

if (fork() != 0){
  pipe(fd_12);    //open pipe between command 1 and 2
  if (fork() != 0){ // command 1 
		
  }else{ // command 2            
        			
  }
        
} else {
	pipe(fd_34)  // open pipe between commands 3 and 4
	if (fork() != 0){  // command 3
						 
  // if no command, just close the pipe and exit()
	}else{ // command 4
		   				
	}

}
```

Before piping, I open the pipe between command 2 and 3, thus in command 2 or command 3 process, they can access to this pipe without calling `pipe()`

and I create another fork outside this function

```c
int pid = fork();
if (child) {
  call_pipe(); // this is where I call the above function
} else {
  sleep(1);
}
```

It turns out that it is hard for the above parent to `wait` for all 4 process terminated. Take `command1 | command2 | command 3` for exmaple, after command 1 terminated, my parent will wake from `waitpid()`and keep going as all the commands completed. However, at this time, the output from `command 3` has no been printed yet. Thus, I end up having result from `command 3`  appearing in the next iteration wheh the shell attempt to receive message:

```
sshell: "output from previous cycle"
```

After using the sleep to force the parent stop, I have the correct output as the result.

#### Reflection

I tried to use `signal` to let the parent wait for all the 4 child process. I have tried `pause()`, `kill(getpid(),SIG_STOP )` and let child to send `kill(SIGCONT)` or other self-defined macros, but ended up either having the output printed but parent terminated. 

My partner have tried the recursion way but failed. So I finally adopt my 2 fork 4 child process pipe with `sleep()` method.