#include "server.h"


int BUFSIZE = 1518;
unsigned char GLOBAL_SEQ_NR = 0;
int REQ_NR = 0;

image_list *head = NULL; //lenkeliste for alle bilder så man slipper å opprette nye bilder hver iterasjon


void error(int ret, char *msg) {
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char *argv[]) {
    
    if (argc != 4) {
        printf("usage: ./server myport directory-name output-filename\n");
        return 0;
    }

    unsigned short my_port = atoi(argv[1]);
    char* directory = argv[2];   
    char* output_file = argv[3];           

    //metode for å åpne en mappe
    read_directory(directory);

    //metode for å ta imot pakke fra klienten
    receive_and_send_ack(my_port, output_file);
}


//leser gjennom hver fil i mappen og legger de til lenkelisten
void read_directory(char* directory) {

    struct Image *list_img;
    char *file, *file_name, *path, *basename;

    DIR *dir = opendir(directory);
    if (!dir) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;

    //readdir trenger ikke free
    entry = readdir(dir);

    //leser fil-for-fil i big_set
    while (entry) {
        if (entry -> d_type == DT_REG) /*DT_REG = Regular file*/ {

            //leser bildet
            path = "big_set/";
            basename = entry->d_name;
            file_name = malloc(strlen(path) + strlen(basename) + 1);
            strcpy(file_name, path);
            strcat(file_name, basename);

            file = read_picture_file(file_name);
            free(file_name);

            //oppretter Image struct av bildet
            list_img = Image_create(file);
            free(file);

            //legger structen til lenkelisten
            add_node(list_img, basename);

        }
        
        entry = readdir(dir);
    }

    closedir(dir);
}


//leser bildefil og returnerer det som char*
char* read_picture_file(char *file) {

    char *picture_buf, *picture;
    int picture_size;

    FILE *image = fopen(file, "r");
    if (!image) {
        perror("Opening picture-file");
        exit(EXIT_FAILURE);
    }

    picture_size = check_filesize(image);
    picture = malloc(picture_size);

    //leser innholdet i bildefilen og legger det til char arrayet
    picture_buf = malloc(picture_size);
    fread(picture_buf, picture_size, 1, image);
    strncpy(&(picture[0]), picture_buf, picture_size);

    free(picture_buf);
    fclose(image);
    return picture;
}


//legger til et bilde i lenkelisten (bakerst)
void add_node(struct Image *list_img, char *name) {

    //lenkelisten er tom
    if (head == NULL) {
        head = malloc(sizeof(image_list));

        head->next = NULL;
        head->image = list_img;
        head->name = name;
        return;
    }

    image_list *current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    //legger til på slutten
    current->next = malloc(sizeof(image_list));
    current->next->image = list_img;
    current->next->name = name;
    current->next->next = NULL;
}


//tar imot packets fra client og sendes ACK om riktig packet mottas
void receive_and_send_ack(unsigned short my_port, char *output_file) {

    int so, rc;
    char buf[BUFSIZE]; //kan muligens fjernes/endres
    struct sockaddr_in server_address, client_addr;
    char *image_buf, *basename;
    struct Image *img;
    fd_set read_set;
    int packet_size, file_name_len;
    unsigned char flags, seq;
    
    so = socket(AF_INET, SOCK_DGRAM, 0);
    error(so, "socket");

    memset(&server_address, 0, sizeof(server_address));
    memset(&client_addr, 0, sizeof(client_addr));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(my_port);
    server_address.sin_addr.s_addr = INADDR_ANY;


    //motta ip-addresse og port
    rc = bind(so, 
            (struct sockaddr*) &server_address, 
            sizeof(server_address));
    error(rc, "bind");

    //Initialiserer select
    FD_ZERO(&read_set);


    while (1) {
        
        socklen_t plen = sizeof(struct sockaddr_in);
        rc = recvfrom(so, buf, BUFSIZE, 0, (struct sockaddr *) &client_addr, &plen);
        error(rc, "receive");
        buf[rc] = '\0';


        //om flag er 0x4 skal server terminere
        memcpy(&flags, &(buf[6]), sizeof(unsigned char));
        if (flags == 0x4) {
            printf("Terminating\n");
            break;
        }

        //riktig sekvensnummer  
        memcpy(&seq, &(buf[4]), sizeof(unsigned char));
        if (seq == GLOBAL_SEQ_NR) {

            //packet data som må brukes i andre metoder
            memcpy(&packet_size, &(buf[0]), sizeof(int));
            memcpy(&GLOBAL_SEQ_NR, &(buf[4]), sizeof(unsigned char));
            memcpy(&file_name_len, &(buf[12]), sizeof(int));
            
            basename = malloc((sizeof(char) * file_name_len) + 1);
            strncpy(basename, &(buf[16]), file_name_len);
            basename[file_name_len] = 0;
          

            //oppretter char* av bildet
            image_buf = create_pointer(buf, packet_size, file_name_len);

            //oppretter struct Image
            img = Image_create(image_buf);
            free(image_buf);
    
            //sammenlikner bildet med de fra mappen
            compare_images(img, basename, output_file);    
            free(basename);

            GLOBAL_SEQ_NR++; 

            //sender ACK, endrer flagget til 0x2
            int termination_len = sizeof(int) + (sizeof(unsigned char) * 4);
            unsigned char termination_flags = 0x2;
            memcpy(&(buf[6]), &termination_flags, sizeof(unsigned char));

            rc = sendto(so, 
                        buf, 
                        termination_len, 
                        0,
                        (struct sockaddr*) &client_addr,
                        sizeof(struct sockaddr_in)); 

            error(rc, "sendto - ACK");
        }

    }
  
    close(so);

    //frigir minnet
    free_memory();
}


//oppretter char pointer av payload
char* create_pointer(char* buf, int packet_size, int file_name_len) {
    
    char *image_buf;
    int offset = packet_size - 16 - file_name_len;

    image_buf = malloc(offset + 1);
    strncpy(image_buf, &(buf[16 + file_name_len]), offset);

    return image_buf;
}


//sammenlikner bilde man har fått inn med de lokale bildene i lenkelisten
void compare_images(struct Image *img, char *payload_name, char *output_file) {

    image_list *current = head;
    int ret;
    char* string;
    char* unknown = "UNKNOWN";

    //Itererer gjennom lenkelisten
    while (current != NULL) {

        //sammenlikner bildet man får inn med bildet fra big_set
        ret = Image_compare(img, current->image);

        //returverdien fra Image_compare er en match 
        if(ret == 1) {

            string = format_output(payload_name, payload_name);
            write_to_file(output_file, string);
            free(string);
            
            Image_free(img);
            return;
        }

        current = current->next;
    }

    //om ingen av bildene er en match
    string = format_output(payload_name, unknown);
    
    write_to_file(output_file, string);
    free(string);

    Image_free(img);
}


//sjekker nøyaktig filstørrelse
int check_filesize(FILE* file) {

    int rc;

    fseek(file, 0, SEEK_END);
    rc = ftell(file); 
    fseek(file, 0, SEEK_SET); 
    
    return rc;
}


//formatterer utskriften 
char* format_output(char *name1, char *name2) {

    char *string;

    string = malloc(strlen(name1) + strlen(name2) + (sizeof(char) * 7) + 1);
    strcpy(string, "<");
    strcat(string, name1);
    strcat(string, "> <");
    strcat(string, name2);
    strcat(string, ">\n");
    printf("\n%s\n", string);

    return string;
}


//skriver til fil
void write_to_file(char *output_file, char *name) {

    FILE *output = fopen(output_file, "a");
    if (!output) {
        perror("Error opening output-file \n");
        exit(EXIT_FAILURE);
    }

    //fputs appender til filen
    fputs(name, output);

    fclose(output);
}


//frigir allokert minne
void free_memory() {
    
    //frigjør lenkeliste structs og deres char* 
    image_list *current = head;
    while (current != NULL) {
        image_list *temp = current; 
        current = current->next;
        free(temp->image->data);
        free(temp->image);
        free(temp);
    }
}


//testing
/*packet data som må brukes i andre metoder
memcpy(&packet_size, &(buf[0]), sizeof(int));
memcpy(&GLOBAL_SEQ_NR, &(buf[4]), sizeof(unsigned char));
memcpy(&GLOBAL_SEQ_NR_ACK, &(buf[5]), sizeof(unsigned char));
memcpy(&flags, &(buf[6]), sizeof(unsigned char));
memcpy(&unused, &(buf[7]), sizeof(unsigned char));

memcpy(&REQ_NR, &(buf[8]), sizeof(int));
memcpy(&file_name_len, &(buf[12]), sizeof(int));

basename = malloc((sizeof(char) * file_name_len) + 1);
strncpy(basename, &(buf[16]), file_name_len);
basename[file_name_len] = 0;


printf("\nPacket size: %d\n", packet_size);
printf("SEQ: %d\n", GLOBAL_SEQ_NR);
printf("SEQ_ACK: %d\n", GLOBAL_SEQ_NR_ACK);
printf("flags: %d\n", flags);
printf("unused: %d\n", unused);
printf("requests: %d\n", REQ_NR);
printf("filename len: %d\n", file_name_len);
printf("Basename: %s\n", basename);
*/
