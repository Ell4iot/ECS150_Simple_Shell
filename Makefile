sshell: sshell.c
	gcc -Wall -Wextra -Werror -o sshell sshell.c
sshell.o:
	gcc -Wall -Wextra -Werror -o sshell.c
clean:
	rm -f sshell sshell.o