#ifndef IMAGELIST_H
#define IMAGELIST_H

#include "pgmread.h" //for å opprette struct Image


//lenkeliste til images i server
typedef struct image_list {
    struct image_list *next; 
    struct Image *image;
    char* name;
}image_list;

#endif

    
  