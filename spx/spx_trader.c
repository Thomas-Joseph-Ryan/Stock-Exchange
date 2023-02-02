#include "spx_trader.h"

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

//Credit - https://stackoverflow.com/questions/13078926/is-there-a-way-to-count-tokens-in-c
unsigned long int getNofTokens(const char* string){
  char* stringCopy;
  unsigned long int stringLength;
  unsigned long int count = 0;

  stringLength = (unsigned)strlen(string);
  stringCopy = malloc((stringLength+1)*sizeof(char));
  strcpy(stringCopy,string);

  if( strtok(stringCopy, " ") != NULL){
    count++;
    while( strtok(NULL," ;") != NULL )
        count++;
  }

  free(stringCopy);
  return count;
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
	char* msg = (char*)malloc(sizeof(char)*(bytes_read+1));
	strcpy(msg, buf);
	return msg;
}

void write_message(int fd, char* message) {
	/*
		Writes the message to the trader pipe
	*/
	char buf[128];
	int msg_length = sprintf(buf, "%s", message);
	int ret = write(fd, buf, msg_length);
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

    // event loop:

    // wait for exchange open (MARKET message)
    pause();
    char* msg = read_message(read_fd);
    printf("[T%d] recieved message: %s\n", id, msg);
    free(msg);

    int order_id = 0;

    while(1) {
        //Wait for market message
        pause();
        char* msg = read_message(read_fd);
        printf("[T%d] recieved message: %s\n", id, msg);
        printf("Number of tokens = %ld\n", getNofTokens(msg));
        if (getNofTokens(msg) != 5) {
            free(msg);
            continue;
        }
        strtok(msg, " ;");
        char* order_type = strtok(NULL, " ;");
        if (strcmp(order_type, "SELL") != 0) {
            free(msg);
            continue;
        }
        printf("Order type %s", order_type);

        //Confirmed to be a sell order

        char* product = strtok(NULL, " ");
        printf("Product %s", product);
        int qty = atoi(strtok(NULL, " "));
        printf("qty %d", qty);
        int price = atoi(strtok(NULL, ";"));
        printf("price %d", price);
        //Close autotrader if qty too large
        if (qty >= 1000) {
            break;
        }
        // send order
        char response[88];
        snprintf(response, 88, "BUY %d %s %d %d;", order_id, product, qty, price);
        free(msg);
        write_message(write_fd, response);

        while (1) {
            int ret = kill(getppid(), SIGUSR1);
            if (ret == -1) {
                perror("In trader, Kill failed");
            }

            // wait for exchange confirmation (ACCEPTED message)
            ret = sleep(3);
            if (ret == 0) {
                continue;
            }
            msg = read_message(read_fd);
            printf("[T%d] recieved confirmation message: %s\n", id, msg);
            free(msg);
            break;
        }
        order_id ++;

    }
    close(read_fd);
    close(write_fd);
}