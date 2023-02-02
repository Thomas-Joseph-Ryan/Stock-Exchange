1. Describe how your exchange works.
The exchange works by initialising the signal handlers first for sigusr1 and sigusr2 (sigint is for quitting gracefully). 
All structs in the exchange are stored in a linked list with similar structs. First the product file is read in and each 
product is created as a structure with the product name and the price levels which that product is selling at. Then the 
traders are created and launched as child processes. These are communicated with using named pipes with the sigusr1 signal, 
when this signal is received, the pid of the child process is added to a queue in the exchange process. Then the main process 
is forked where the parent continues as the exchange, and the child process becomes a polling process for each of the traders. 
This ensures that when a trader sends the POLLHUP signal when closing their side of the pipe (disconnecting) then the poll 
process sends sigusr2 to the main process which causes it to decrement the number of active traders. After this the market 
opens staying in a loop which pauses until a signal is received, then it checks if the queue which contains pending trader pids
is empty, if not it processes the orders until the queue is empty. Then once all of the traders are disconnected all of the
allocated memory is freed and the market closes.

2. Describe your design decisions for the trader and how it's fault-tolerant.
The trader is fault tolerant as it waits for market open, then pauses again waiting to recieve a market message. Then the number
of strings when the recieved message is split is checked, if this is anything other then 5, then it is definately not a SELL 
message so the trader waits again. This check is done again to see if it is a SELL message and not a buy, then the qty is checked,
if it is over 1000 then the trader disconnects. Then a response is made in order to buy the items which will be sold, afterwhich
the trader writes the message to the pipe once, then sends the sigusr1 signal to the exchange, then sleeps for 3 seconds. If the
sleep returns 0, then it was not interrupted by a signal so the sigusr1 signal is sent to the exchange again as it may have been
lost. If the sleep statement is interrupted the its return value will not be equal to 0 and a response has been received from
the exchange. So the order id is incremented and the trader waits for a market message again. 

3. Describe your tests and how to run them.
