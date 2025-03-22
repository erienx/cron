#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char** argv) {
	pid_t pid = atoi(argv[1]);
	int signo = atoi(argv[2]);
	union sigval value;
	value.sival_int = atoi(argv[3]);

	sigqueue(pid, signo, value);

	return EXIT_SUCCESS;
}

