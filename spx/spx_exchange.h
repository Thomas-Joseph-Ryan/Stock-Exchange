#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"
#include <math.h>

#define LOG_PREFIX "[SPX]"
#define MAX_PRODUCT_LENGTH 17

typedef struct product product;
typedef struct trader trader;
typedef struct order order;
typedef struct level level;
typedef struct position position;
typedef struct order_pointer order_pointer;

struct product {
    char name[MAX_PRODUCT_LENGTH];
    product* next;
    int num_buy_levels;
    level* buy_levels;
    int num_sell_levels;
    level* sell_levels;
};

struct level {
    int price;
    int num_orders;
    int total_qty;
    order* orders;
    level* next;
};


struct trader {
    int id;         /* Trader id */
    trader* next;   /* Linked list data structure */
    pid_t pid;      /* pid of trader process */
    char exchange_fifo[FIFO_NAME_MAX];  /* Exchange fifo name */
	char trader_fifo[FIFO_NAME_MAX];    /* Trader fifo name */
    int read;       /* Read file descriptor */
    int write;      /* Write file descriptor */
    int connected;  /* 1 for connected 0 for disconnected */
    int order_num;  /* Order number the trader is up to */
    position* positions;    /* The positions the trader is in */
    order_pointer* order_pointers;
};

struct position {
    char* product_name;
    int qty_gained;
    int funding;
    position* next;
};

enum order_type {
    BUY=0,
    SELL=1,
    APPEND=2,
    CANCEL=3
};

struct order_pointer {
    order* order;
    int id;
    int cancelled;
    order_pointer* next;
};

struct order {
    int id;
    struct order* next;
    struct order* prev;
    struct trader* trader;
    enum order_type type;
    int qty;
    int price;
    char* msg;
    level* level;
    product* product;
    order_pointer* op;
};

/*************************/
/* Function declarations */
/*************************/

/******************************************************************/
/* Queue functions - https://www.programiz.com/dsa/circular-queue */
/******************************************************************/
int isFull();
int isEmpty();
void enQueue(pid_t element);
int deQueue();
/******************/
/* Write and Read */
/******************/
void write_message(trader* trader, char* message);
char* read_message(int fd);
/********************/
/* Level management */
/********************/
void insert_level_node(product* product, level* level, int buylevel);
void del_level_node(product* product, level* level, int buylevel);
void del_all_level_node(product* product);
/********************/
/* Order Management */
/********************/
void send_invalid(trader* trader);
void send_accepted(trader* trader, int id);
void send_amended(trader* trader, int id);
void send_cancelled(trader* trader, int id);
int check_id_valid(trader* trader, char* id);
order_pointer* check_order_id_exists(trader* trader, char* s_id);
void create_order();
void amend_order(order* new_order, order* order, int qty, int price);
void add_order(order* order);
void add_order_to_level(level* level, order* order);
void cancel_order(order_pointer* order_p);
void delete_all_orders(level* level);
void delete_order(order* order);
/*************/
/* Orderbook */
/*************/
void broadcast_order(int type, char* product, int qty, int price, trader* trader);
void broadcast_fill();
void print_orderbook();
/******************/
/* Order Pointers */
/******************/
void add_order_pointer(trader* trader, order* order);
void delete_order_pointers(trader* trader);
/******************/
/* Order Matching */
/******************/
void check_matches(order* new_order);
int fill_new_sell_order(order* sell, level* buy);
int fill_new_buy_order(order* buy, level* sell);
void send_fill();
void create_positons(trader* trader);
/*************/
/* Positions */
/*************/
void create_positons(trader* trader);
position* find_position(order* order);
void delete_positions(trader* trader);
void print_positions();
/*******************/
/* Signal Handling */
/*******************/
static void handle_sigusr1(int sig, siginfo_t *info, void *ucontext);
static void handle_sigint(int sig, siginfo_t *info, void *ucontext);
static void handle_sigusr2(int sig, siginfo_t *info, void *ucontext);
/**********************/
/* Product Management */
/**********************/
void delete_products(product* head);
product* read_products(char* file);
product* check_product_exists(char* product);
/*********************/
/* Trader Management */
/*********************/
trader* create_traders(int argc, char **argv);
void delete_traders(trader* head);
void poll_loop();
void market_open(trader* head);
void market_close();



#endif
