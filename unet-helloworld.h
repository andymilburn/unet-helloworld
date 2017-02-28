#ifndef UNET_HELLOWORLD_H
#define UNET_HELLOWORLD_H

#include <stdbool.h>

#include "unet.h"

/* common arguments */
extern bool server_mode;
extern enum protocol protocol;
extern char *endpoint;
extern unsigned int endpoint_index;
extern char *bind_endpoint;
extern unsigned int bind_endpoint_index;

#endif
