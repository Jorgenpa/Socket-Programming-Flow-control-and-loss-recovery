#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>

#include "packet.h"
#include "send_packet.h"

extern int BUFSIZE;
extern int BUFSIZE_ACK;
extern unsigned char GLOBAL_SEQ_NR;
extern unsigned char GLOBAL_SEQ_NR_ACK;
extern int REQ_NR; //for debugging

extern struct packet** packet_array;
extern int count;
extern linked_list *head;


void error(int ret, char *msg);
void read_from_list(const char* file);
char* create_packet(char* file);
void go_back_n(const char* server_ip, unsigned short dest_port);
char* create_termination_packet();
void add_node(struct packet* ny_packet);
int remove_node();
int check_filesize(FILE* file);
void free_memory();

#endif