/*
 * unet-drive
 *
 * Simple load driver using unet sockets



 Send only to immediate connection
 Immediate connection set at startup
 commandline:  LoadDrive <node ID> <target node ID> <delay>
 target node==0 means no sending
 frequency in microseconds

 sent message:  sending node id + seq number + time sent (HH:MM:SS-YYYYMMDD)

 Point-to-point
 Star (many-to-one)


 log:
 On send log:  time sent + sending node ID + seq number + sent message
 On receive log:  time received + receiving node ID + sent message

 time sent
 time received + round-trip-time
 message size

 notification of dropped messages (timeout after x seconds)

 Knobs
 rate of sending


 phase 2
 Notiication of lost connections (from kernel)
 send to specific target (as opposed to immediate neighbor)


 Confirm existence in kernel of logging
 APCR
 APCA
 Handshake steps
 Route table size
 +/- to route table
 Bad packet received


 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#include "unet-common.h"

bool server_mode = false;
const char *server_id = "app.chat"; // should probably change this to app.drive but ok for now
uint32_t message_type = 1200;	/* hardcoded */

static const char usage_synopsis[] = "unet-drive [options] <server-address>";
static const char usage_short_opts[] = "m:i:hv";
static struct option const usage_long_opts[] = {
    { "mt",			required_argument, NULL, 'm'},
    { "id",			required_argument, NULL, 'i'},
    { "help",		no_argument,       NULL, 'h'},
    { "version",		no_argument,       NULL, 'v'},
    { NULL,			no_argument,       NULL, 0x0},
};

static const char * const usage_opts_help[] = {
    "\n\tMessage type (default is 1200)",
    "\n\tApplication id (default is app.chat)",
    "\n\tPrint this help and exit",
    "\n\tPrint version and exit",
    NULL,
};

static void usage(const char *errmsg)
{
    print_usage(errmsg, usage_synopsis, usage_short_opts,
                usage_long_opts, usage_opts_help);
}


char currentTimeStr[26];

char *currentTimeString()
{
    time_t timer;
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);

    strftime(currentTimeStr, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return (char *)currentTimeStr;
}

#define MAX_DATA_SIZE 8192
// to avoid any (admittedly tiny) additional overhead from malloc,
// we'll just use static buffers


char stringBuffer[MAX_DATA_SIZE];
char *randomStringOfSize(int dataSize)
{
    for(int i = 0; i < dataSize; i++) {
        stringBuffer[i] = (random() % 64) + 32;
    }
    stringBuffer[dataSize] = 0;
    return stringBuffer;
}

long testDataSequenceNumber = 0;

char outboundTestDataBuffer[MAX_DATA_SIZE];
char outboundTestDataBufferCopy[MAX_DATA_SIZE];
char inboundTestDataBuffer[MAX_DATA_SIZE];


char *nextTestDataString(int dataSize)
{
    sprintf(outboundTestDataBuffer,"Seq: %ld Time: %s Data: %s",
            testDataSequenceNumber++,
            currentTimeString(),
            randomStringOfSize(dataSize)
            );

    outboundTestDataBuffer[dataSize] = 0; // truncate the random data so we have correct size

    return outboundTestDataBuffer;
}

int main(int argc, char *argv[])
{
    int s, err, opt, optidx, len;
    struct sockaddr_unet server_sa, peer_sa, self_sa, in_sa;
    char *server_ua_txt = NULL, *peer_ua_txt = NULL, *self_ua_txt = NULL, *p;
    socklen_t slen;
    fd_set rfds;
    bool connected = false;
    char line[256], buf[65536];

    printf("uNET DRIVE start\n");
    fflush(stdout);
    exit(EXIT_FAILURE);


    while ((opt = getopt_long(argc, argv, usage_short_opts,
                              usage_long_opts, &optidx)) != EOF) {
        switch (opt) {
            case 'm':
                message_type = atoi(optarg);
                break;
            case 'i':
                server_id = optarg;
                break;
            case 'v':
                printf("Version: %s\n", PACKAGE_VERSION);
                exit(EXIT_SUCCESS);
            case 'h':
                usage(NULL);
            default:
                usage("unknown option");
        }
    }

    if (optind < argc)
        server_mode = false;

    memset(&server_sa, 0, sizeof(server_sa));
    server_sa.sunet_family = AF_UNET;
    server_sa.sunet_addr.message_type = message_type;
    err = unet_str_to_addr(server_id, strlen(server_id), &server_sa.sunet_addr.addr);
    if (err == -1) {
        fprintf(stderr, "bad server id (%s) provided (%d:%s)\n",
                server_id, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    s = socket(AF_UNET, SOCK_DGRAM, 0);
    if (s == -1) {
        perror("Failed to open unet socket (is unet enabled in your kernel?)");
        exit(EXIT_FAILURE);
    }

    if (server_mode) {

        server_ua_txt = unet_addr_to_str(&server_sa.sunet_addr.addr);
        if (!server_ua_txt) {
            perror("failed on unet_addr_to_str()");
            exit(EXIT_FAILURE);
        }
        printf("server binding to '%s'\n", server_ua_txt);

        free(server_ua_txt);

        server_ua_txt = NULL;

            err = bind(s, (struct sockaddr *)&server_sa, sizeof(server_sa));
            if (err == -1) {
                fprintf(stderr, "failed to bind using %s server_id (%d:%s)\n",
                        server_id, errno, strerror(errno));
                exit(EXIT_FAILURE);
            }

        connected = false;
    } else {

        len = asprintf(&server_ua_txt, "%s:%s", argv[optind], server_id);


        server_sa.sunet_family = AF_UNET;
        server_sa.sunet_addr.message_type = message_type;
        err = unet_str_to_addr(server_ua_txt, strlen(server_ua_txt), &server_sa.sunet_addr.addr);
        if (err == -1) {
            fprintf(stderr, "bad full server address (%s) provided (%d:%s)\n",
                    server_ua_txt, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        err = connect(s, (struct sockaddr *)&server_sa, sizeof(server_sa));
        if (err == -1) {
            fprintf(stderr, "failed to connect to full server address (%s) (%d:%s)\n",
                    server_ua_txt, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* now get sockname to get the full address */
        memset(&peer_sa, 0, sizeof(peer_sa));
        slen = sizeof(peer_sa);
        err = getpeername(s,(struct sockaddr *)&peer_sa, &slen);
        if (err == -1) {
            perror("failed on getpeername()");
            exit(EXIT_FAILURE);
        }

        peer_ua_txt = unet_addr_to_str(&peer_sa.sunet_addr.addr);
        if (!peer_ua_txt) {
            perror("failed on unet_addr_to_str()");
            exit(EXIT_FAILURE);
        }

        connected = true;
    }

    /* now get sockname to get the full address */
    memset(&self_sa, 0, sizeof(self_sa));
    slen = sizeof(self_sa);
    err = getsockname(s, (struct sockaddr *)&self_sa, &slen);
    if (err == -1) {
        perror("failed on getsockname()");
        exit(EXIT_FAILURE);
    }

    self_ua_txt = unet_addr_to_str(&self_sa.sunet_addr.addr);
    if (!self_ua_txt) {
        perror("failed on unet_addr_to_str()");
        exit(EXIT_FAILURE);
    }

    printf("Welcome to unet-drive; %s '%s'\n",
           server_mode ? "listening for clients in" : "using server",
           server_mode ? self_ua_txt : server_ua_txt);
    printf("\r%s > ", self_ua_txt);
    fflush(stdout);

    struct timeval lastPacketSendTime;
    gettimeofday(&lastPacketSendTime, NULL);

    long desiredPacketIntervalInMicroSeconds = 4000000;
    int desiredPacketSize = 64;

    FD_ZERO(&rfds);
    for (;;) {
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(s, &rfds);

        err = select(s + 1, &rfds, NULL, NULL, NULL);
        if (err == -1) {
            perror("select() failed");
            exit(EXIT_FAILURE);
        }
        /* no data (probably EAGAIN) */
        if (err == 0)
            continue;

        /* line read */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            p = fgets(line, sizeof(line) - 1, stdin);
            if (p) {
                line[sizeof(line) - 1] = '\0';
                len = strlen(line);
                while (len > 0 && line[len-1] == '\n')
                    len--;
                line[len] = '\0';


// here we detect single-letter commands from stdint
// q = quit
// s = start/continue sending data (using current settings)
// x = pause sending data


                if(strlen(line) == 1) {
                  printf("command\n");
                  switch (line[0]) {
                    case 'q':
                    printf("quit\n");
                    break;
                  }
                }


            }

            printf("%s > ", self_ua_txt);
            fflush(stdout);

        } else if (FD_ISSET(s, &rfds)) { // not sure why this is else...stdin takin priority over socket traffic?
            /* first server packet */

            slen = sizeof(in_sa);
            len = recvfrom(s, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&in_sa, &slen);
            if (len > 0) {
                buf[len] = '\0';

                slen = sizeof(in_sa);

                if (!connected) { // this is our first traffic from this peer, so we connect


                    memcpy(&peer_sa, &in_sa, sizeof(in_sa));

                    peer_ua_txt = unet_addr_to_str(&peer_sa.sunet_addr.addr);
                    if (!peer_ua_txt) {
                        perror("failed on unet_addr_to_str()");
                        exit(EXIT_FAILURE);
                    }

                    err = connect(s, (struct sockaddr *)&peer_sa, sizeof(peer_sa));
                    if (err == -1) {
                        fprintf(stderr, "failed to connect to peer address (%s) (%d:%s)\n",
                                peer_ua_txt, errno, strerror(errno));
                        exit(EXIT_FAILURE);
                    }

                    fprintf(stderr, "\nconnection from (%s)\n", peer_ua_txt);

                    connected = true;
                }

                /* do no allow more than one connection */
                if (!unet_addr_eq(&peer_sa.sunet_addr.addr, &in_sa.sunet_addr.addr))
                    continue;

                printf("\r%*s\r%s> len:%d\n", 80, "", peer_ua_txt, len);

                printf("%s > ", self_ua_txt);
                fflush(stdout);
            }

            // HERE we handle the automatic packet sending
            struct timeval currentTime, timeSinceLastPacket;
            gettimeofday(&currentTime, NULL);
            timersub(&currentTime, &lastPacketSendTime, &timeSinceLastPacket);
            long int microSecsSinceLastPacket = timeSinceLastPacket.tv_sec * 1000000 + timeSinceLastPacket.tv_usec;

            if(microSecsSinceLastPacket >= desiredPacketIntervalInMicroSeconds) {
              if (!connected) {
                  perror("not connected, so can't send packet\n");
                  continue;
              }

              char *outboundPacketString = nextTestDataString(desiredPacketSize);
              len = send(s, outboundPacketString, strlen(outboundPacketString), 0);
              if (len == -1) {
                  perror("failed to send\n");
                  exit(EXIT_FAILURE);
              } else {
                printf("sent %ld\n",strlen(outboundPacketString));
              }
              gettimeofday(&lastPacketSendTime, NULL);
            }

        } // bottom of socket ready and no pending stdin input
    } // bottom of eternal for-loop

    close(s);

    if (server_ua_txt)
        free(server_ua_txt);
    if (peer_ua_txt)
        free(peer_ua_txt);
    if (self_ua_txt)
        free(self_ua_txt);

    return 0;
}
