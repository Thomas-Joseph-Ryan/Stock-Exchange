/**
 * comp2017 - assignment 3
 * Thomas Ryan
 * 510460295
 */

#include "spx_exchange.h"

product* product_head;
trader* trader_head;
int number_of_products;
struct pollfd pfds[MAX_CONNECTIONS];
int number_of_traders;
int number_of_active_traders;
int exchange_fees = 0;
pid_t process_queue[MAX_CONNECTIONS];
int front = -1, rear = -1;

/*
	TO DO:
		- Make amend change level to correct level and push to the back (could do this by deleting
		then readding order to where it is supposed to be?)
		- Fix cancel (no clue)
		- Create order matching.
		- Testcases (bugs on ed then own test cases)
		- Own trader binary which reads textfiles and puts them into pipe one line at a time.
		- Auto trader
*/

/******************************************************************/
/* Queue functions - https://www.programiz.com/dsa/circular-queue */
/******************************************************************/

// Check if the queue is full
int isFull() {
  if ((front == rear + 1) || (front == 0 && rear == MAX_CONNECTIONS - 1)) return 1;
  return 0;
}

// Check if the queue is empty
int isEmpty() {
  if (front == -1) return 1;
  return 0;
}

// Adding an element
void enQueue(pid_t element) {
  if (isFull())
    printf("\n Queue is full!! \n");
  else {
    if (front == -1) front = 0;
    rear = (rear + 1) % MAX_CONNECTIONS;
    process_queue[rear] = element;
    // printf("\n Inserted -> %d", element);
  }
}

// Removing an element
int deQueue() {
  int element;
  if (isEmpty()) {
    printf("\n Queue is empty !! \n");
    return (-1);
  } else {
    element = process_queue[front];
    if (front == rear) {
      front = -1;
      rear = -1;
    } 
    // Q has only one element, so we reset the 
    // queue after dequeing it. ?
    else {
      front = (front + 1) % MAX_CONNECTIONS;
    }
    // printf("\n Deleted element -> %d \n", element);
    return (element);
  }
}

/******************/
/* Write and Read */
/******************/

void write_message(trader* trader, char* message) {
	/*
		Writes the message to the trader pipe
	*/
	int write_fd = trader->write;
	char buf[128];
	int msg_length = sprintf(buf, "%s", message);
	int ret = write(write_fd, buf, msg_length);
	if (ret == -1) {
		perror("Write failed");
	}
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

/********************/
/* Level management */
/********************/

void insert_level_node(product* product, level* level, int buylevel) {
	/*
		Sorts the new buy level node into decending price order on insert
	*/
	if (buylevel) {
		struct level* current = product->buy_levels;
		//If head is NULL
		if (current == NULL) {
			product->buy_levels = level;
			return;
		}
		//Otherwise
		if (level->price > current->price) {
			//Greatest buy price so it is the new head
			product->buy_levels = level;
			level->next = current;
			return;
		}
		struct level* prev = current;
		current = current->next;
		while (current != NULL) {
			if (prev->price > level->price && level->price > current->price) {
				prev->next = level;
				level->next = current;
				return;
			}
			prev = current;
			current = current->next;
		}
		prev->next = level;
		return;
	} else {
		struct level* current = product->sell_levels;
		//If head is NULL
		if (current == NULL) {
			product->sell_levels = level;
			return;
		}
		//Otherwise
		if (level->price > current->price) {
			//Greatest sell price so it is the new head.
			product->sell_levels = level;
			level->next = current;
			return;
		}
		struct level* prev = current;
		current = current->next;
		while (current != NULL) {
			if (prev->price > level->price && level->price > current->price) {
				prev->next = level;
				level->next = current;
				return;
			}
			prev = current;
			current = current->next;
		}
		prev->next = level;
		return;
	}

}

void del_level_node(product* product, level* level, int buylevel) {
	if (buylevel) {
		struct level* current = product->buy_levels;
		//Deleting head
		if (current == level) {
			product->buy_levels = current->next;
			product->num_buy_levels--;
			free(current);
			return;
		}
		//Otherwise
		struct level* prev = NULL;
		while (current != level) {
			prev = current;
			current = current->next;
		}
		prev->next = current->next;
		free(current);
		product->num_buy_levels--;
		return;
	} else {
		struct level* current = product->sell_levels;
		//Deleting head
		if (current == level) {
			product->sell_levels = current->next;
			product->num_sell_levels--;
			free(current);
			return;
		}
		//Otherwise
		struct level* prev = NULL;
		while (current != level) {
			prev = current;
			current = current->next;
		}
		prev->next = current->next;
		free(current);
		product->num_sell_levels--;
		return;
	}

}

void del_all_level_node(product* product) {
	level* current = product->buy_levels, *next = NULL;
	while (current != NULL) {
		next = current->next;
		delete_all_orders(current);
		free(current);
		current = next;
	}
	current = product->sell_levels, next = NULL;
	while (current != NULL) {
		next = current->next;
		delete_all_orders(current);
		free(current);
		current = next;
	}
	return;
}


/********************/
/* Order Management */
/********************/

void send_invalid(trader* trader) {
	write_message(trader, "INVALID;");
	kill(trader->pid, SIGUSR1);
	return;
}

void send_accepted(trader* trader, int id) {
	char buf[20]; //Remove magic number
	sprintf(buf, "ACCEPTED %d;", id);
	write_message(trader, buf);
	kill(trader->pid, SIGUSR1);
	return;
}

void send_amended(trader* trader, int id) {
	char buf[20]; //Remove magic number
	sprintf(buf, "AMENDED %d;", id);
	write_message(trader, buf);
	kill(trader->pid, SIGUSR1);
	return;
}

void send_cancelled(trader* trader, int id) {
	char buf[20]; //Remove magic number
	sprintf(buf, "CANCELLED %d;", id);
	write_message(trader, buf);
	kill(trader->pid, SIGUSR1);
	return;
}

int check_id_valid(trader* trader, char* id) {
	if (id == NULL) {
		return -1;
	}
	int id_int = atoi(id);
	if (id_int != trader->order_num || id_int > 999999) {
		return -1;
	} else {
		return id_int;
	}
}

order_pointer* check_order_id_exists(trader* trader, char* s_id) {
	if (s_id == NULL) {
		return NULL;
	}
	int id = atoi(s_id);
	if (id >= trader->order_num) {
		return NULL;
	}
	order_pointer* current = trader->order_pointers;
	while (current != NULL) {
		if (current->id == id) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

void create_order() {
	/*
		Create order object, have linked list of orders which updates when an order is completed,
		this means you can search iteratively through linked list to process orders in the correct
		sequence.
	*/
	pid_t trader_pid = deQueue();
	if (trader_head == NULL) {
		return;
	}
   	trader* current = trader_head;
   	while (current != NULL) {
	   	if (current->pid == trader_pid) {
		   	break;
	   	}
	   	current = current->next;
    }
	trader* trader = current;
	struct order* new_order = (struct order*)malloc(sizeof(struct order));
	new_order->next = NULL;
	new_order->prev = NULL;
	new_order->msg = strtok(read_message(trader->read), ";");
	new_order->trader = trader;
	printf("%s [T%d] Parsing command: <%s>\n", LOG_PREFIX, trader->id, new_order->msg);
	char* cmd = strtok(new_order->msg, " ");
	if (strcmp("BUY", cmd) == 0) {
		/*
			Need to check order id is valid, product is valid, qty is valid and price is valid.
		*/
		int id = check_id_valid(trader, strtok(NULL, " "));
		if (id == -1) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		} 
		new_order->id = id; 

		product* product = check_product_exists(strtok(NULL, " "));
		if (product == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		
		//atoi converts NULL to 0, so error checking for NULL pointer is already handled
		char* temp = strtok(NULL, " ");
		if (temp == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		new_order->qty = atoi(temp);
		if (new_order->qty < 1 || new_order->qty > 999999) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}

		temp = strtok(NULL, " ");
		if (temp == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		new_order->price = atoi(temp);
		if (new_order->price < 1 || new_order->price > 999999) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}

		trader->order_num ++;
		new_order->type = 0;
		new_order->product = product;
		int qty = new_order->qty;
		int price = new_order->price;
		add_order(new_order);
		send_accepted(trader, id);
		broadcast_order(0, product->name, qty, price, trader);
		check_matches(new_order);
		print_orderbook();
		print_positions();
		sleep(0.1);
		//Send match information


	} else if (strcmp("SELL", cmd) == 0) {
		
		int id = check_id_valid(trader, strtok(NULL, " "));
		if (id == -1) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		} 
		new_order->id = id;
		
		product* product = check_product_exists(strtok(NULL, " "));
		if (product == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		
		char* temp = strtok(NULL, " ");
		if (temp == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		new_order->qty = atoi(temp);
		if (new_order->qty < 1 || new_order->qty > 999999) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		
		temp = strtok(NULL, " ");
		if (temp == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		new_order->price = atoi(temp);
		if (new_order->price < 1 || new_order->price > 999999) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}

		trader->order_num ++;
		new_order->type = 1;
		new_order->product = product;
		int qty = new_order->qty;
		int price = new_order->price;
		add_order(new_order);
		send_accepted(trader, id);
		broadcast_order(1, product->name, qty, price, trader);
		check_matches(new_order);
		print_orderbook();
		print_positions();
		sleep(0.1);
		//Send match information

	} else if (strcmp("AMEND", cmd) == 0) {
		order_pointer* to_amend = check_order_id_exists(trader, strtok(NULL, " "));
		if (to_amend == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		if (to_amend->cancelled) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		int id = to_amend->id;

		char* temp = strtok(NULL, " ");
		if (temp == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		int qty = atoi(temp);
		if (qty < 1 || qty > 999999) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		
		temp = strtok(NULL, " ");
		if (temp == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		int price = atoi(temp);
		if (price < 1 || price > 999999) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		
		amend_order(new_order, to_amend->order, qty, price);
		int type = new_order->type;
		char* product_name = new_order->product->name;
		send_amended(trader, id);
		broadcast_order(type, product_name, qty, price, trader);
		check_matches(new_order);
		print_orderbook();
		print_positions();
		sleep(0.1);

	} else if (strcmp("CANCEL", cmd) == 0) {
		
		order_pointer* to_cancel = check_order_id_exists(trader, strtok(NULL, " "));
		
		if (to_cancel == NULL) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}
		if (to_cancel->cancelled) {
			send_invalid(trader);
			free(new_order->msg);
			free(new_order);
			return;
		}

		cancel_order(to_cancel);
		print_orderbook();
		print_positions();
		sleep(0.1);
		free(new_order->msg);
		free(new_order);

	} else {
		send_invalid(trader);
		free(new_order->msg);
		free(new_order);
		return;
	}
	return;

}

void amend_order(order* new_order, order* order, int qty, int price) {
	/*
		Amending an order changes the price and qty of the order, all other
		aspects of order stay the same. As the price can change, the level
		which the order is on can also change, so I create a new clone order,
		then delete the old order and add the new order to the trader/level.
	*/
	new_order->id = order->id;
	new_order->product = order->product;
	new_order->op = order->op;
	order->op->order = new_order;
	new_order->type = order->type;
	new_order->qty = qty;
	new_order->price = price;
	delete_order(order);
	add_order(new_order);
	

}

void add_order(order* order) {
	trader* trader = order->trader;
	product* product = order->product;
	int sell_order = order->type;
	
	add_order_pointer(trader, order);
	
	if (!sell_order) {
		/*
			Add order to product buy level
		*/
		level* current = product->buy_levels;
		int price_match = 0;
		while (current != NULL) {
			if (order->price == current->price) {
				price_match = 1;
				break;
			}
			current = current->next;
		}
		if (price_match) {
			current->num_orders ++;
			current->total_qty = current->total_qty + order->qty;
			add_order_to_level(current, order);
		} else {
			level* new_level = (level*)malloc(sizeof(level));
			new_level->next = NULL;
			new_level->num_orders = 1;
			new_level->orders = NULL;
			new_level->price = order->price;
			new_level->total_qty = order->qty;
			product->num_buy_levels++;
			insert_level_node(product, new_level, 1);
			add_order_to_level(new_level, order);
		}
	} else {
		/*
			Add order to product sell level
		*/
		level* current = product->sell_levels;
		int price_match = 0;
		while (current != NULL) {
			if (order->price == current->price) {
				price_match = 1;
				break;
			}
			current = current->next;
		}
		if (price_match) {
			current->num_orders ++;
			current->total_qty = current->total_qty + order->qty;
			add_order_to_level(current, order);
		} else {
			level* new_level = (level*)malloc(sizeof(level));
			new_level->next = NULL;
			new_level->num_orders = 1;
			new_level->orders = NULL;
			new_level->price = order->price;
			new_level->total_qty = order->qty;
			product->num_sell_levels++;
			insert_level_node(product, new_level, 0);
			add_order_to_level(new_level, order);
		}
	}
}

void add_order_to_level(level* level, order* order) {
	/*
		Add orders to end of linked list, this is in a way sorted by time.
	*/
	order->level = level;
	struct order* current = level->orders;
	if (current == NULL) {
		level->orders = order;
		return;
	}
	struct order* prev = NULL;
	while (current != NULL) {
		prev = current;
		current = current->next;
	}
	prev->next = order;
	order->prev = prev;
	return;
}

void cancel_order(order_pointer* order_p) {
	order_p->cancelled = 1;
	order* order = order_p->order;
	order->level->total_qty = order->level->total_qty - order->qty;
	order->qty = 0;
	order->price = 0;
	broadcast_order(order->type, order->product->name, order->qty, order->price, order->trader);
	int id = order->id;
	send_cancelled(order->trader, id);
	delete_order(order);
}

void delete_all_orders(level* level) {
	order* current = level->orders;
	order* next = NULL;
	while (current != NULL) {
		next = current->next;
		free(current->msg);
		free(current);
		current = next;
	}
	return;
}

void delete_order(order* order) {
	/*
		Have to maintain the level linked list
	*/
	level* level = order->level;
	if (order->prev == NULL) {
		//If order is head of linked list
		if (order->next == NULL) {
			//This is the last order on this price level
			int buy_level = 1;
			if (order->type) {
				buy_level = 0;
			}
			del_level_node(order->product, level, buy_level);
			free(order->msg);
			free(order);
			return;
		}
		level->orders = order->next;
		order->next->prev = NULL;
		level->num_orders--;
		level->total_qty = level->total_qty - order->qty;
		free(order->msg);
		free(order);
	} else {
		order->prev->next = order->next;
		if (order->next != NULL) {
			order->next->prev = order->prev;
		}
		level->num_orders--;
		level->total_qty = level->total_qty - order->qty;
		free(order->msg);
		free(order);
	}
}

/*************/
/* Orderbook */
/*************/

void broadcast_order(int type, char* product, int qty, int price, trader* trader) {
	char msg[88];
	char* order_type = "BUY";
	if (type == 1) {
		order_type = "SELL";
	}
	snprintf(msg, 88, "MARKET %s %s %d %d;", order_type, product, qty, price);
	for (struct trader* t = trader_head; t != NULL; t = t->next) {
		if (trader->pid == t->pid) {
			continue;
		}
		write_message(t, msg);
		kill(t->pid, SIGUSR1);
	}
}


void print_orderbook() {
	/*
		Print sell first highest price to lowest price, then buy orders highest price to lowest price.
		sell prices will always be higher then buy prices as if they were not then a match would be made to 
		fill that order. 
		Orders are filled in price time, so best price first, then by time best price was put through.
	*/
	printf("%s\t--ORDERBOOK--\n", LOG_PREFIX);
	char* msg;
	for (product* p = product_head; p != NULL; p = p->next) {
		printf("%s\tProduct: %s; Buy levels: %d; Sell levels: %d\n", 
		LOG_PREFIX, p->name, p->num_buy_levels, p->num_sell_levels);

		for (level* l = p->sell_levels; l != NULL; l = l->next) {
			if (l->num_orders > 1) {
				msg = "orders";
			} else {
				msg = "order";
			}

			printf("%s\t\tSELL %d @ $%d (%d %s)\n", 
			LOG_PREFIX, l->total_qty, l->price, l->num_orders, msg);
		}
		for (level* l = p->buy_levels; l != NULL; l = l->next) {
			if (l->num_orders > 1) {
				msg = "orders";
			} else {
				msg = "order";
			}

			printf("%s\t\tBUY %d @ $%d (%d %s)\n", 
			LOG_PREFIX, l->total_qty, l->price, l->num_orders, msg);
		}
	}

}


/******************/
/* Order Pointers */
/******************/

void add_order_pointer(trader* trader, order* order) {
	for (order_pointer* op = trader->order_pointers; op != NULL; op = op->next) {
		if (op->id == order->id) {
			return;
		}
	}
	order_pointer* new_ptr = (order_pointer*)malloc(sizeof(order_pointer));
	new_ptr->order = order;
	new_ptr->id = order->id;
	new_ptr->cancelled = 0;
	new_ptr->next = NULL;
	order->op = new_ptr;
	if (trader->order_pointers == NULL) {
		trader->order_pointers = new_ptr;
	} else {
		order_pointer* current = trader->order_pointers;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = new_ptr;
	}
	return;
}

void delete_order_pointers(trader* trader) {
	/*
		Delete the order_pointers of all traders
	*/
	order_pointer* current = trader->order_pointers;
	order_pointer* next = NULL;
	while (current != NULL) {
		next = current->next;
		free(current);
		current = next;
	}
	return;
}

/******************/
/* Order Matching */
/******************/

void check_matches(order* new_order) {
	/*
		For a buy order, go through the sell_levels, lowest to highest and 
		match accordingly. Take the price of the SELL_LEVEL

		In order to avoid adding prev pointers to the levels list, for sell you
		first walk through the list reversing all of the pointers, then on the way
		back you check matching then reverse the pointers again.

		For a sell order go through the buy_levels, highest to lowest and 
		match accordingly. Take the price of the BUY_LEVEL
	*/
	product* p = new_order->product;

	int buy_order = 1;
	if (new_order->type) {
		buy_order = 0;
	}

	if (buy_order) {
		//Reverse the linked list.
		level* prev = NULL;
		level* current = p->sell_levels;
		level* next = NULL;
		while (current != NULL) {
			next = current->next;
			current->next = prev;
			prev = current;
			current = next;
		}

		//Final element = prev
		current = prev;
		p->sell_levels = current;
		prev = NULL;
		next = NULL;

		//Iterate through list and re reverse it.
		int filled = 0;

		while (current != NULL) {
			next = current->next;
			if (!filled) {
				if (current->price <= new_order->price) {
					filled = fill_new_buy_order(new_order, current);
				}
			}
			current = next;
		}

		prev = NULL;
		current = p->sell_levels;
		next = NULL;		
		while (current != NULL) {
			next = current->next;
			current->next = prev;
			prev = current;
			current = next;
		}
		p->sell_levels = prev;

	} else {
		//Start from highest buy level
		int filled = 0;
		level* current = p->buy_levels;
		level* next = NULL;
		while (current != NULL) {
			next = current->next;
			if (!filled) {
				if (current->price >= new_order->price) {
					filled = fill_new_sell_order(new_order, current);
				}
			} else {
				break;
			}
			current = next;
		}
	}
}

int fill_new_sell_order(order* sell, level* buy) {
	/*
		THIS DOES NOT WORK RIGHT NOW
	*/
	int price = buy->price;
	position* sell_pos = find_position(sell);
	order* buy_order = buy->orders;
	while (buy_order != NULL) {
		position* buy_pos = find_position(buy_order);
		if (sell->qty > buy_order->qty) {
			int qty_sold = buy_order->qty;
			int value = qty_sold*price;
			int fee = round(value*0.01);
			exchange_fees += fee;
			sell->qty = sell->qty - qty_sold;
			sell->level->total_qty = sell->level->total_qty - qty_sold;
			sell_pos->funding += value - fee;
			sell_pos->qty_gained = sell_pos->qty_gained - qty_sold;
			buy_pos->funding = buy_pos->funding - value;
			buy_pos->qty_gained += qty_sold;
			order* temp = buy_order->next;
			printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n"
			, LOG_PREFIX, buy_order->id, buy_order->trader->id, sell->id, sell->trader->id, 
			value, fee);
			buy_order->op->cancelled = 1;
			send_fill(buy_order, qty_sold);
			send_fill(sell, qty_sold);
			delete_order(buy_order);
			buy_order = temp;
			continue;
		} else if (buy_order->qty > sell->qty) {
			int qty_sold = sell->qty;
			int value = qty_sold*price;
			int fee = round(value*0.01);
			exchange_fees += fee;
			buy_order->qty = sell->qty - qty_sold;
			buy_order->level->total_qty = buy_order->level->total_qty - qty_sold;
			sell_pos->funding += value - fee;
			sell_pos->qty_gained = sell_pos->qty_gained - qty_sold;
			buy_pos->funding = buy_pos->funding - value;
			buy_pos->qty_gained += qty_sold;
			printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n"
			, LOG_PREFIX, buy_order->id, buy_order->trader->id, sell->id, sell->trader->id, 
			value, fee);
			sell->op->cancelled = 1;
			send_fill(buy_order, qty_sold);
			send_fill(sell, qty_sold);
			delete_order(sell);
			return 1;
		} else {
			int qty_sold = sell->qty;
			int value = qty_sold*price;
			int fee = round(value*0.01);
			exchange_fees += fee;
			sell_pos->funding += value - fee;
			sell_pos->qty_gained = sell_pos->qty_gained - qty_sold;
			buy_pos->funding = buy_pos->funding - value;
			buy_pos->qty_gained += qty_sold;
			printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n"
			, LOG_PREFIX, buy_order->id, buy_order->trader->id, sell->id, sell->trader->id, 
			value, fee);
			buy_order->op->cancelled = 1;
			sell->op->cancelled = 1;
			send_fill(buy_order, qty_sold);
			send_fill(sell, qty_sold);
			delete_order(sell);
			delete_order(buy_order);
			return 1;
		}
	}
	return 0;
}

int fill_new_buy_order(order* buy, level* sell) {
	//Use sell order price
	int price = sell->price;
	position* buy_pos = find_position(buy);
	order* sell_order = sell->orders;
	while (sell_order != NULL) {
		position* sell_pos = find_position(sell_order);
		if (buy->qty > sell_order->qty) {
			int qty_sold = sell_order->qty;
			int value = qty_sold*price;
			int fee = round(value*0.01);
			exchange_fees += fee;
			buy->qty = buy->qty - sell_order->qty;
			buy->level->total_qty = buy->level->total_qty - qty_sold;
			sell_pos->funding += value;
			sell_pos->qty_gained = sell_pos->qty_gained - qty_sold;
			buy_pos->funding = buy_pos->funding - value - fee;
			buy_pos->qty_gained += qty_sold;
			order* temp = sell_order->next;
			printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n"
			, LOG_PREFIX, sell_order->id, sell_order->trader->id, buy->id, buy->trader->id, 
			value, fee);
			sell_order->op->cancelled = 1;
			send_fill(sell_order, qty_sold);
			send_fill(buy, qty_sold);
			delete_order(sell_order);
			sell_order = temp;
			continue;
		} else if (sell_order->qty > buy->qty) {
			int qty_sold = buy->qty;
			int value = qty_sold*price;
			int fee = round(value*0.01);
			exchange_fees += fee;
			sell_order->qty = sell_order->qty - buy->qty;
			sell_order->level->total_qty = sell_order->level->total_qty - qty_sold;
			sell_pos->funding += value;
			sell_pos->qty_gained = sell_pos->qty_gained - qty_sold;
			buy_pos->funding = buy_pos->funding - value - fee;
			buy_pos->qty_gained += qty_sold;
			printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n"
			, LOG_PREFIX, sell_order->id, sell_order->trader->id, buy->id, buy->trader->id, 
			value, fee);
			buy->op->cancelled = 1;
			send_fill(sell_order, qty_sold);
			send_fill(buy, qty_sold);
			delete_order(buy);
			return 1;
		} else {
			int qty_sold = buy->qty;
			int value = qty_sold*price;
			int fee = round(value*0.01);
			exchange_fees += fee;
			sell_pos->funding += value;
			sell_pos->qty_gained = sell_pos->qty_gained - qty_sold;
			buy_pos->funding = buy_pos->funding - value - fee;
			buy_pos->qty_gained += qty_sold;
			printf("%s Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%d.\n"
			, LOG_PREFIX, sell_order->id, sell_order->trader->id, buy->id, buy->trader->id, 
			value, fee);
			sell_order->op->cancelled = 1;
			buy->op->cancelled = 1;
			send_fill(sell_order, qty_sold);
			send_fill(buy, qty_sold);
			delete_order(buy);
			delete_order(sell_order);
			return 1;
		}
	}
	return 0;
}

void send_fill(order* order, int qty) {
	int id = order->id;
	trader* trader = order->trader;
	char buf[30]; //Remove magic number
	sprintf(buf, "FILL %d %d;", id, qty);
	write_message(trader, buf);
	int ret = kill(trader->pid, SIGUSR1);
	if (ret == -1) {
		perror("Kill failed in fill");
	}
	return;
}

/*************/
/* Positions */
/*************/

void create_positons(trader* trader) {
	/*
		Add positions in the same order they are read from the products doc, add to the trader
		positions.
	*/
	position* prev_pos = NULL;
	for (product* product = product_head; product != NULL; product = product->next) {
		position* new_pos = (position*)malloc(sizeof(position));
		new_pos->product_name = product->name;
		new_pos->funding = 0;
		new_pos->qty_gained = 0;
		new_pos->next=NULL;

		if (trader->positions == NULL){
			trader->positions = new_pos;
			prev_pos = new_pos;
			continue;
		} else {
			prev_pos->next = new_pos;
			prev_pos = new_pos;
			continue;
		}
	}
}

position* find_position(order* order) {
	position* current = order->trader->positions;
	while (current != NULL) {
		if (current->product_name == order->product->name) {
			break;
		}
		current = current->next;
	}
	return current;
}

void delete_positions(trader* trader) {
	/*
		Delete the positions of a trader
	*/
	position* current = trader->positions;
	position* next = NULL;
	while (current != NULL) {
		next = current->next;
		free(current);
		current = next;
	}
	return;
}

void print_positions() {
	printf("%s\t--POSITIONS--\n", LOG_PREFIX);
	for (trader* t = trader_head; t != NULL; t = t->next) {
		printf("%s\tTrader %d:", LOG_PREFIX, t->id);
		for (position* p = t->positions; p != NULL; p = p->next) {
			printf(" %s %d ($%d)", p->product_name, p->qty_gained, p->funding);
			if (p->next != NULL) {
				printf(",");
			}
		}
		printf("\n");
	}
}

/*******************/
/* Signal Handling */
/*******************/

static void handle_sigusr1(int sig, siginfo_t *info, void *ucontext) {
    /*
        sig - The number of the signal that caused invocation of the handler

        info - A pointer to a siginfo_t, which is a structure containing further information
               about the signal

        ucontext - Unused by the handeller.

        pid_t si_pid : The sending process ID

		Main issue with handelling everything in signal handler is how to I ensure priority to an order which
		got in first but has not been dealt with before the next signal is called.
    */
   	pid_t trader_pid = info->si_pid;
   	// printf("interrupt caller: %d\n", trader_pid);

	enQueue(trader_pid);
   	return;
}

static void handle_sigint(int sig, siginfo_t *info, void *ucontext) {
	/*
		Quit gracefully
	*/
	printf("Quitting gracefully\n");
	delete_traders(trader_head);
	delete_products(product_head);
	exit(0);
}

static void handle_sigusr2(int sig, siginfo_t *info, void *ucontext) {
	/*
		Quit gracefully
	*/
	number_of_active_traders --;
	return;
}

/**********************/
/* Product Management */
/**********************/

void delete_products(product* head) {
	product* current = head;
	product* next = NULL;
	while (current != NULL) {
		next = current->next;
		del_all_level_node(current);
		free(current);
		current = next;
	}
}

product* read_products(char* file) {
	//TODO: check formatting
	
	char buff[MAX_PRODUCT_LENGTH];

	FILE* f = fopen(file, "r");
	if (f == NULL) {
		perror("Could not open product file");
	}

	product* head = NULL;
	product* prev = NULL;
	int first_line = 1;
	while (fgets(buff, MAX_PRODUCT_LENGTH, f)) {
		if (first_line) {
			first_line = 0;
			number_of_products = atoi(buff);
			continue;
		}
		char* product_name = strtok(buff, " \n");
		if (product_name == NULL) {
			continue;
		}
		//buff is a line at a time with null byte at end
		product* prod = (product*)malloc(sizeof(product));
		if (prod == NULL) {
			perror("Could not allocate product");
		}
		strcpy(prod->name, product_name);
		prod->next = NULL;
		prod->num_buy_levels = 0;
		prod->num_sell_levels = 0;
		prod->buy_levels = NULL;
		prod->sell_levels = NULL;
		if (head == NULL) {
			head = prod;
			prev = prod;
		} else {
			prev->next = prod;
			prev = prod;
		}
	}
	fclose(f);
	return head;
}

product* check_product_exists(char* product) {
	if (product == NULL) {
		return NULL;
	}
	struct product* current = product_head;
	while (current != NULL) {
		if (strcmp(current->name, product) == 0) {
			//product exists
			return current;
		}
		current = current->next;
	}
	return NULL;
}

/*********************/
/* Trader Management */
/*********************/

trader* create_traders(int argc, char **argv) {
	/*
	Create FIFO's for all traders along with all children.
	*/
	char exchange_fifo[FIFO_NAME_MAX];
	char trader_fifo[FIFO_NAME_MAX];
	int j = 0;
	trader* head = NULL;
	trader* prev = NULL;
	pid_t pid;
	int write_fd;
	int read_fd;
	number_of_traders = 0;
	for (int i = 2; i < argc; i++) {
		sprintf(exchange_fifo, FIFO_EXCHANGE, j);
		sprintf(trader_fifo, FIFO_TRADER, j);
		if (-1 == mkfifo(exchange_fifo, S_IRWXU | S_IRWXG | S_IRWXO)) {
			perror("Exchange mkfifo failed");
		} else {
			printf("%s Created FIFO %s\n", LOG_PREFIX, exchange_fifo);
		}
		if (-1 == mkfifo(trader_fifo, S_IRWXU | S_IRWXG | S_IRWXO)) {
			perror("Exchange mkfifo failed");
		} else {
			printf("%s Created FIFO %s\n", LOG_PREFIX, trader_fifo);
		}

		//Create trader objects
		trader* new_trader = (trader*)malloc(sizeof(trader));
		new_trader->next = NULL;
		strcpy(new_trader->exchange_fifo, exchange_fifo);
		strcpy(new_trader->trader_fifo, trader_fifo);
		new_trader->id = j;
		new_trader->connected = 1;
		new_trader->positions = NULL;
		new_trader->order_num = 0;
		new_trader->order_pointers = NULL;
		create_positons(new_trader);


		if (head == NULL) {
			head = new_trader;
			prev = new_trader;
		} else {
			prev->next = new_trader;
			prev = new_trader;
		}
		printf("%s Starting trader %d (%s)\n", LOG_PREFIX, j, argv[j+2]);
		pid = fork();
		if (pid == -1) {
			perror("Fork failed");
		}
		if (pid == 0) {
			//In child
			char* execv_args[3];
			char id[10];
			sprintf(id, "%d", j);
			execv_args[0] = argv[i];
			execv_args[1] = id;
			execv_args[2] = NULL;
			if (execv(argv[i], execv_args) == -1) {
				perror("exec failed");
			}
			exit(0);
		} else {
			//In exchange
			new_trader->pid = pid;
			write_fd = open(new_trader->exchange_fifo, O_WRONLY);
			if (write_fd == -1) {
        		perror("Open write_fifo failed");
    		} else {
				printf("%s Connected to %s\n", LOG_PREFIX, new_trader->exchange_fifo);
			}
			read_fd = open(new_trader->trader_fifo, O_RDONLY);
			if (read_fd == -1) {
        		perror("Open read_fifo failed");
    		}else{
				printf("%s Connected to %s\n", LOG_PREFIX, new_trader->trader_fifo);
			}
			new_trader->write = write_fd;
			new_trader->read = read_fd;
			pfds[number_of_traders].fd = read_fd;
			pfds[number_of_traders].events = POLLHUP;
			pfds[number_of_traders].revents = 0;
			number_of_traders++;
		}
		j++;
	}
	number_of_active_traders = number_of_traders;
	sleep(0.1);
	return head;
}


void delete_traders(trader* head) {
	/*
	Free all trader structs in memory
	*/
	trader* current = head;
	trader* next = NULL;
	while (current != NULL) {
		unlink(current->exchange_fifo);
		unlink(current->trader_fifo);
		close(current->read);
		next = current->next;
		delete_positions(current);
		delete_order_pointers(current);
		free(current);
		current = next; 
	}
}

void poll_loop() {
	while (number_of_active_traders > 0) {
		int ret = poll(pfds, number_of_traders, -1);
		if (ret == -1) {
			if (errno == EINTR) {
				continue;
			}
			perror("Poll error");
		} else if (ret > 0) {
			for (int i = 0; i < number_of_traders; i++) {
				if (pfds[i].revents != 0) {
					trader* current = trader_head;
					while (current != NULL) {
						if (current->id == i && current->connected) {
							current->connected = 0;
							printf("%s Trader %d disconnected\n", LOG_PREFIX, current->id);
							number_of_active_traders--;
							kill(getppid(), SIGUSR2);
						}
						current = current->next;
					}
				}
			}
		}
	}
	return;
}

/*********************/
/* Market Management */
/*********************/

void market_open(trader* head) {
	trader* current = head;
	while (current != NULL) {
		write_message(current, "MARKET OPEN;");
		current = current->next;
	}
	current = head;
	while (current != NULL) {
		int ret = kill(current->pid, SIGUSR1);
		if (ret == -1) {
			perror("Sending SIGUSR1 failed");
		}
		current = current->next;
	}

	return;
}

void market_close() {
	printf("%s Trading completed\n", LOG_PREFIX);
	printf("%s Exchange fees collected: $%d\n", LOG_PREFIX, exchange_fees);	
	delete_traders(trader_head);
	delete_products(product_head);
}

int main(int argc, char **argv) {
	
	//Create the sigaction for SIGUSR1
	struct sigaction sigusr1;
    memset(&sigusr1, 0, sizeof(struct sigaction));
    sigusr1.sa_flags = SA_SIGINFO;
    sigusr1.sa_sigaction = handle_sigusr1;
    if (sigaction(SIGUSR1, &sigusr1, NULL) == -1) {
        perror("exchange sigaction SIGUSR1 failed");
    }
	
	struct sigaction sigint;
    memset(&sigint, 0, sizeof(struct sigaction));
    sigint.sa_flags = SA_SIGINFO;
    sigint.sa_sigaction = handle_sigint;
    if (sigaction(SIGINT, &sigint, NULL) == -1) {
        perror("exchange sigaction SIGINT failed");
    }

	struct sigaction sigusr2;
    memset(&sigusr2, 0, sizeof(struct sigaction));
    sigint.sa_flags = SA_SIGINFO;
    sigint.sa_sigaction = handle_sigusr2;
    if (sigaction(SIGUSR2, &sigint, NULL) == -1) {
        perror("exchange sigaction SIGUSR2 failed");
    }

	printf("%s Starting\n", LOG_PREFIX);

	char* product_file = argv[1];

	//Read products and place them into linked list
	product_head = read_products(product_file);
	printf("%s Trading %d products: ", LOG_PREFIX, number_of_products);
	product* current = product_head;
	while (current != NULL) {
		printf("%s", current->name);
		if (current->next != NULL) {
			printf(" ");
		}
		current = current->next;
	}
	printf("\n");

	//Create traders and named pipes and connect to them
	trader_head = create_traders(argc, argv);

	//Create child process for polling
	int ret = fork();
	if (ret == -1) {
		perror("Fork in main failed");
	} else if (ret == 0) {
		//In child
		/*
			Using child process to poll the traders to check for when they
			disconnect, when they disconnect they send the exchange a SIGUSR2.
			The exchange will decrememnt active traders by 1 and continue until
			active traders is 0, at which point it will quit.
		*/
		poll_loop();
		return 1;
	}

	//Notify traders market is open
	market_open(trader_head);

	//In parent
	while (number_of_active_traders > 0) {
		//Do processing
		pause();
		while (!isEmpty()) {
			create_order();
		}
	}

	//Close market
	market_close();
	return 0;
}
