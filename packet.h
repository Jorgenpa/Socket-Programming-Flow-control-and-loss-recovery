#ifndef PACKET_H
#define PACKET_H


//til packets i client
typedef struct packet {
    int seq; 
    char data[1518]; //ethernet packet size
}packet;

//lenkeliste med packets i client (sliding window)
typedef struct linked_list {
    struct linked_list *next; 
    struct packet *packet_pointer;
}linked_list;

#endif