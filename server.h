#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include "imagelist.h"


extern unsigned char GLOBAL_SEQ_NR;
extern int REQ_NR; 
extern int BUFSIZE;

extern image_list *head;


void error(int ret, char *msg);
char* create_pointer(char* buf, int packet_size, int file_name_len);
void read_directory(char* directory);
char* read_picture_file(char *file);
void add_node(struct Image *list_img, char *name);
void receive_and_send_ack(unsigned short my_port, char *output_file);
int check_filesize(FILE* file);
void compare_images(struct Image *img, char *payload_name, char *output_file);
char* format_output(char *name1, char *name2);
void write_to_file(char *output_file, char *name);
void free_memory();

#endif 