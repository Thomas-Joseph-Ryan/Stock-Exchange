#include "spx_trader.h"

#define NUMARGS 2

static void interrupt(int sig, siginfo_t *info, void *ucontext) {
    /*
        sig - The number of the signal that caused invocation of the handler

        info - A pointer to a siginfo_t, which is a structure containing further information
               about the signal

        ucontext - Unused by the handeller.

        pid_t si_pid : The sending process ID
    */
   return;
}

char* read_message(int fd) {
	//Make sure after using this method you free the return value.
	int read_fd = fd;
	char buf[128];
	ssize_t bytes_read = read(read_fd, buf, 128);
	if (bytes_read == -1) {
		perror("Read failed");
	}
	buf[bytes_read] = 0;
	char* msg = (char*)malloc(sizeof(char)*(bytes_read));
	strcpy(msg, buf);
	return msg;
}

void write_message(int fd, char* message) {
	/*
		Writes the message to the trader pipe
	*/
	char buf[128];
	int msg_length = sprintf(buf, "%s", message);
	int ret = write(fd, buf, msg_length+1);
	if (ret == -1) {
		perror("Write failed");
	}
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    int id = atoi(argv[1]);

    // register signal handler
    struct sigaction sig;
    memset(&sig, 0, sizeof(struct sigaction));
    sig.sa_flags = SA_SIGINFO;
    sig.sa_sigaction = interrupt;
    if (sigaction(SIGUSR1, &sig, NULL) == -1) {
        perror("trader sigaction failed");
    }

    // connect to named pipes
    char exchange_fifo[FIFO_NAME_MAX];
	char trader_fifo[FIFO_NAME_MAX];

    sprintf(exchange_fifo, FIFO_EXCHANGE, id);
	sprintf(trader_fifo, FIFO_TRADER, id);

    int read_fd = open(exchange_fifo, O_RDONLY);
    if (read_fd == -1) {
        perror("Open read_fifo failed");
    }
    int write_fd = open(trader_fifo, O_WRONLY);
    if (write_fd == -1) {
        perror("Open write_fifo failed");
    }

    char* input[NUMARGS] = {"SELL 0 Coriander 15 200;", "SELL 1 Basil 3 100"};
    // event loop:
    int i = 0;

    // wait for exchange open (MARKET message)
    pause();
    char* msg = read_message(read_fd);
        printf("[T%d] recieved message: %s\n", id, msg);
        free(msg);

    while(i < NUMARGS) {
        sleep(0.1);
        // send order
        write_message(write_fd, input[i]);
        int ret = kill(getppid(), SIGUSR1);
        if (ret == -1) {
            perror("In trader, Kill failed");
        }
        printf("[T%d] sent message: %s\n", id, input[i]);

        // wait for exchange confirmation (ACCEPTED message)
        while (1) {
            pause();
            msg = read_message(read_fd);
            if (strcmp(strtok(msg, " "), "MARKET") == 0) {
                continue;
            }
            printf("[T%d] recieved confirmation message: %s\n", id, msg);
            free(msg);
            break;
        }
        i++;
    }
    close(read_fd);
    close(write_fd);
}
