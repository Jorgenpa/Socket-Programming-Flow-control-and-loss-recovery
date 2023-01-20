#include "client.h"

int BUFSIZE = 1518;
int BUFSIZE_ACK = 8;
unsigned char GLOBAL_SEQ_NR = 0;
unsigned char GLOBAL_SEQ_NR_ACK = 0;
int REQ_NR = 0;

struct packet** packet_array = NULL; //array av packets
int num_packets = 0;

linked_list *head = NULL; //pointer til en foreløpig tom lenkeliste av packet pointers



void error(int ret, char *msg) {
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char const *argv[]) {

    if (argc != 5) {
        printf("usage: ./client server-addr destport image-filename drop-percentage\n");
        return 0;
    }

    const char* server_ip = argv[1];
    unsigned short dest_port = atoi(argv[2]);
    const char* file = argv[3];             
    float lp = atof(argv[4]);    

    //setter loss-probability
    set_loss_probability(lp);

    //leser listefilen over bilder, som igjen lager pakker av dem og legger de til en lenkeliste
    read_from_list(file);

    //åpner opp for kommunikasjon mellom client og server
    go_back_n(server_ip, dest_port);    
}


//leser fra listefilen
void read_from_list(const char* file) {
    
    int list_size, packet_size;
    int index = 0;
    char c;
    char *packet_with_payload, *path;

    FILE *images = fopen(file, "r");
    if (!images) {
        perror("Opening list-file");
        exit(EXIT_FAILURE);
    }

    list_size = check_filesize(images);
    char list_buf[list_size + 1];

    //finner antall linjer (filer)
    for (c = getc(images); c != EOF; c = getc(images)) 
        if (c == '\n')  
            num_packets++; 
    fseek(images, 0, SEEK_SET);

    //allokerer minne til packet-arrayet
    packet_array = malloc(sizeof(struct packet*) * num_packets);

    //leser linje for linje i filen
    while (fgets(list_buf, sizeof(list_buf), images) != NULL) {
        path = list_buf;
        strtok(list_buf, "\n");

        //oppretter char* packet med payloaden
        packet_with_payload = create_packet(path);
        memcpy(&packet_size, &(packet_with_payload[0]), sizeof(int));


        //legger packet-pekere til et globalt packet-array
        struct packet* ny_packet = malloc(sizeof(struct packet));
        ny_packet -> seq = index;
        memcpy(ny_packet -> data, packet_with_payload, packet_size);

        packet_array[index] = ny_packet;

        index++;
        free(packet_with_payload);
    }

    fclose(images);
}


//leser bildefil og oppretter packet med payload
char* create_packet(char* file) {
    
    char *payload_buf, *packet_with_payload;                      
    int picture_size, packet_size, file_name_len;
    char* basename = file;
    unsigned char flags = 0x1;
    unsigned char unused = 0x7f;


    FILE *image = fopen(file, "r");
    if (!image) {
        perror("Opening picture-file");
        exit(EXIT_FAILURE);
    }
    
    //fjerner "big_set/" fra filnavnet for å få riktig lengde 
    basename += 8;
    file_name_len = strlen(basename) + 1; //legge til nullbyte
    picture_size = check_filesize(image);

    //packet
    packet_size = sizeof(int) + (sizeof(unsigned char) * 4) + (sizeof(int) * 2) + (sizeof(char) * file_name_len) + picture_size;
    packet_with_payload = malloc(packet_size);

    memcpy(&(packet_with_payload[0]), &packet_size, sizeof(int));
    memcpy(&(packet_with_payload[4]), &GLOBAL_SEQ_NR, sizeof(unsigned char));
    memcpy(&(packet_with_payload[5]), &GLOBAL_SEQ_NR_ACK, sizeof(unsigned char));
    memcpy(&(packet_with_payload[6]), &flags, sizeof(unsigned char));
    memcpy(&(packet_with_payload[7]), &unused, sizeof(unsigned char));

    //legger til payload
    memcpy(&(packet_with_payload[8]), &REQ_NR, sizeof(int));
    memcpy(&(packet_with_payload[12]), &file_name_len, sizeof(int));
    strncpy(&(packet_with_payload[16]), basename, sizeof(char) * file_name_len);

    //leser innholdet i bildefilen og legger det til char arrayet
    payload_buf = malloc(picture_size);
    fread(payload_buf, picture_size, 1, image);
    strncpy(&(packet_with_payload[16 + file_name_len]), payload_buf, picture_size);
    
    GLOBAL_SEQ_NR++;

    free(payload_buf);
    payload_buf = NULL;

    fclose(image);
    return packet_with_payload;
}


//go-back-n implementasjon for å sende og motta packets
void go_back_n(const char* server_ip, unsigned short dest_port) {

    int so, rc, timer, len;
    struct in_addr server_ip_address;
    struct sockaddr_in server_address;
    char* termination_packet;

    //go-back-n
    int sending_window_size = 7;
    struct packet *packet_pointer, *packet_temp;
    int seq_first = 0;
    int seq_N = 0;  //sekvensnummeret til den packet man er på
    int temp_seq, temp_len, ret;
    
    //receive ACK
    char buf[BUFSIZE_ACK]; 
    struct timeval timeout; 
    fd_set read_set;
    unsigned char seq;  



    //setter ip-adressen til server
    inet_pton(AF_INET, server_ip, &server_ip_address);

    so = socket(AF_INET, SOCK_DGRAM, 0);
    error(so, "socket");

    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(dest_port);
    server_address.sin_addr = server_ip_address;

    //Initialiserer timer
    FD_ZERO(&read_set);
 

    while (1) {
    
        //sjekk om vinduet er fullt, hvis ikke kan man legge til ny pakke og sende
        if (seq_N - seq_first < sending_window_size && seq_N < num_packets) {
            
            //opprett packet 
            packet_pointer = malloc(sizeof(struct packet));
            memcpy(packet_pointer->data, packet_array[seq_N]->data, BUFSIZE);
            packet_pointer->seq = seq_N;

            memcpy(&len, &packet_pointer->data[0], sizeof(int));

            //lagre packet i lenkelisten
            add_node(packet_pointer);

            rc = send_packet(so, 
            //rc = sendto(so,
                            packet_pointer->data, 
                            len, 
                            0,
                            (struct sockaddr*) &server_address,
                            sizeof(struct sockaddr_in)); 

            error(rc, "sendto");
        
            //starter timer med select timeout
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            FD_SET(so, &read_set);
            timer = select(FD_SETSIZE, &read_set, NULL, NULL, &timeout);
            error(timer, "select");

            if (seq_N < num_packets-1) {
                seq_N++;
            }
        }
        

        //mottar ACK fra server
        if (timer && FD_ISSET(so, &read_set)) {

            socklen_t plen = sizeof(struct sockaddr_in);
            rc = recvfrom(so, buf, BUFSIZE_ACK, MSG_WAITALL, (struct sockaddr *) &server_address, &plen);
            error(rc, "receive");
            buf[rc] = '\0';

            memcpy(&seq, &(buf[4]), sizeof(unsigned char));  

            //mottar ACK fra første pakke, fjerner den fra lenkelisten
            if (seq == seq_first) {
                ret = remove_node();

                if (ret == -1) {
                    error(ret, "remove node");
                }

                //man har nådd slutten, sende termination packet;
                if (seq == num_packets-1) {
                    printf("Terminating\n");
                    break;
                }

                seq_first++;

                //stopper timer 
                //timeout.tv_sec = 0;
                //timeout.tv_usec = 0;
            }
        }


        //select timer ut, sende hele vinduet på nytt
        if(timer == 0) {

            printf("Timeout på %d!\n", seq_first);

            temp_seq = seq_first;

            while(temp_seq < seq_N) {

                //gjøre plass til temp
                packet_temp = malloc(sizeof(struct packet));
                memcpy(packet_temp->data, packet_array[temp_seq]->data, BUFSIZE);
                packet_temp->seq = temp_seq;

                memcpy(&temp_len, &packet_temp->data[0], sizeof(int));

                rc = send_packet(so, 
                //rc = sendto(so,
                                packet_temp->data, 
                                temp_len, 
                                0,
                                (struct sockaddr*) &server_address,
                                sizeof(struct sockaddr_in)); 

                error(rc, "sendto");
            
                //starter timer med select timeout
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                FD_SET(so, &read_set);
                timer = select(FD_SETSIZE, &read_set, NULL, NULL, &timeout);
                error(timer, "select");
                
                temp_seq++;
                free(packet_temp);
            }
        }

    }

    //sender termineringspakke
    termination_packet = create_termination_packet();

    rc = send_packet(so, 
                    termination_packet, 
                    BUFSIZE_ACK, 
                    0,
                    (struct sockaddr*) &server_address,
                    sizeof(struct sockaddr_in)); 

    error(rc, "sendto");
    free(termination_packet);

    close(so);

     //frigi minne
    free_memory();
}


//oppretter pakke som stenger/avslutter koblingen mellom client og server
char* create_termination_packet() {

    char *termination_packet;
    unsigned char termination_flag = 0x4;
    int termination_len = 8;
    unsigned char unused = 0x7f;

    termination_packet = malloc(BUFSIZE_ACK);
    memcpy(&(termination_packet[0]), &termination_len, sizeof(int));
    memcpy(&(termination_packet[4]), &GLOBAL_SEQ_NR, sizeof(unsigned char));
    memcpy(&(termination_packet[5]), &GLOBAL_SEQ_NR_ACK, sizeof(unsigned char));
    memcpy(&(termination_packet[6]), &termination_flag, sizeof(unsigned char));
    memcpy(&(termination_packet[7]), &unused, sizeof(unsigned char));

    return termination_packet;
}


//legger til en node bakerst i lenkelisten
void add_node(struct packet* ny_packet) {

    //lenkelisten er tom
    if (head == NULL) {
        head = malloc(sizeof(linked_list));

        head->next = NULL;
        head->packet_pointer = ny_packet;

        REQ_NR++;
        return;
    }

    linked_list *current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    //legger til på slutten
    current->next = malloc(sizeof(linked_list));
    current->next->packet_pointer = ny_packet;
    current->next->next = NULL;
}


//fjerner første node
int remove_node() {

    int ret = -1;
    linked_list *next = NULL;

    //lenkelisten er tom
    if (head == NULL) {
        return -1;
    }

    next = head->next;
    ret = head->packet_pointer->seq;
    free(head->packet_pointer);
    free(head);
    head = next;

    return ret;
}


//sjekker nøyaktig filstørrelse
int check_filesize(FILE* file) {

    int rc;

    fseek(file, 0, SEEK_END);
    rc = ftell(file); 
    fseek(file, 0, SEEK_SET);
    
    return rc;
}


//frigjør allokert minne
void free_memory() {

    //frigjør lenkeliste structs og deres char* 
    linked_list *current = head;
    while (current != NULL) {
        linked_list *temp = current; 
        current = current->next;
        free(temp->packet_pointer);
        free(temp);
    }

    //frigir allokert minne til packets
    for (int i = 0; i < num_packets; i++) {
        free(packet_array[i]);
    }

    //frigjør packet-arrayet
    free(packet_array);
}


//testing
        /*
        char *basename;
        unsigned char flags, unused;
        int file_name_len;
        memcpy(&GLOBAL_SEQ_NR, &(packet[4]), sizeof(unsigned char));
        memcpy(&GLOBAL_SEQ_NR_ACK, &(packet[5]), sizeof(unsigned char));
        memcpy(&flags, &(packet[6]), sizeof(unsigned char));
        memcpy(&unused, &(packet[7]), sizeof(unsigned char));

        //payload
        memcpy(&REQ_NR, &(packet[8]), sizeof(int));
        memcpy(&file_name_len, &(packet[12]), sizeof(int));

        
        basename = malloc((sizeof(char) * file_name_len) + 1);
        strncpy(basename, &(packet[16]), file_name_len);
        basename[file_name_len] = 0;
        
        
        printf("Packet size: %d\n", len);
        printf("SEQ: %d\n", GLOBAL_SEQ_NR);
        printf("SEQ_ACK: %d\n", GLOBAL_SEQ_NR_ACK);
        printf("flags: %d\n", flags);
        printf("unused: %d\n", unused);
        printf("requests: %d\n", REQ_NR);
        printf("filename len: %d\n", file_name_len);
        printf("Basename: %s\n", basename);
        */