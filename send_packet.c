#define _XOPEN_SOURCE   //må inkluderes for at drand48() skal fungere

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>

#include "send_packet.h"

static float loss_probability = 0.0f;

void set_loss_probability( float x )
{
    loss_probability = x;
    srand48(time(0));
}

ssize_t send_packet( int sock, const char* buffer, size_t size, int flags, const struct sockaddr* addr, socklen_t addrlen )
{
    float rnd = drand48(); 
    //float rnd = ((double) rand() / RAND_MAX);

    if( !(buffer[6] & 0x4) && /* Ignore termination packets. */
	    (rnd < loss_probability) )
    {
        fprintf(stderr, "Randomly dropping a packet\n");
        return size;
    }

    return sendto( sock,
                   buffer,
                   size,
                   flags,
                   addr,
                   addrlen );
}
