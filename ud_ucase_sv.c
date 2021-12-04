/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2020.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Listing 57-6 */

/* ud_ucase_sv.c

   A server that uses a UNIX domain datagram socket to receive datagrams,
   convert their contents to uppercase, and then return them to the senders.

   See also ud_ucase_cl.c.
*/
#include "ud_ucase.h"

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include "tlpi_hdr.h"
volatile int count;
int sfd;
ssize_t numBytes;
socklen_t len;
struct sockaddr_un claddr;

static void toggleIO(int sig)
{
    // if (sendto(sfd, &count, sizeof(count), 0, (struct sockaddr *)&claddr, len) !=
    //     numBytes)
    //     fatal("sendto");
}

int main(int argc, char *argv[])
{
    struct sockaddr_un svaddr;
    int j;
    char buf[BUF_SIZE];

    const char *chipname = "gpiochip0";
    struct gpiod_chip *chip;
    struct gpiod_line *lineRed;
    struct gpiod_line *lineGreen;
    struct gpiod_line *lineYellow;
    int i, val;

    // Open GPIO chip
    chip = gpiod_chip_open_by_name(chipname);

    // Open GPIO lines
    lineRed = gpiod_chip_get_line(chip, 24);
    lineGreen = gpiod_chip_get_line(chip, 25);
    lineYellow = gpiod_chip_get_line(chip, 5);

    // Open LED lines for output
    gpiod_line_request_output(lineRed, "example1", 0);
    gpiod_line_request_output(lineGreen, "example1", 0);
    gpiod_line_request_output(lineYellow, "example1", 0);

    i = 0;

    sfd = socket(AF_UNIX, SOCK_DGRAM, 0); /* Create server socket */
    if (sfd == -1)
        errExit("socket");

    /* Construct well-known address and bind server socket to it */

    /* For an explanation of the following check, see the erratum note for
       page 1168 at http://www.man7.org/tlpi/errata/. */

    if (strlen(SV_SOCK_PATH) > sizeof(svaddr.sun_path) - 1)
        fatal("Server socket path too long: %s", SV_SOCK_PATH);

    if (remove(SV_SOCK_PATH) == -1 && errno != ENOENT)
        errExit("remove-%s", SV_SOCK_PATH);

    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, SV_SOCK_PATH, sizeof(svaddr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr *)&svaddr, sizeof(struct sockaddr_un)) == -1)
        errExit("bind");

    /* Receive messages, convert to uppercase, and return to client */

    for (;;)
    {
        t_data received_data;
        len = sizeof(struct sockaddr_un);
        numBytes = recvfrom(sfd, &received_data, sizeof(received_data), 0,
                            (struct sockaddr *)&claddr, &len);
        if (numBytes == -1)
            errExit("recvfrom");

        printf("Received %d: %d\n", received_data.IO, received_data.period);
        /*FIXME: above: should use %zd here, and remove (long) cast */


        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = toggleIO;
        if (sigaction(SIGALRM, &sa, NULL) == -1)
            errExit("sigaction");

        struct itimerval itv;
        itv.it_value.tv_sec = 0;
        itv.it_value.tv_usec = 1;
        itv.it_interval.tv_sec = 10;
        itv.it_interval.tv_usec = 0;

        if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
            errExit("setitimer");

        gpiod_line_set_value(lineRed, (i & 1) != 0);
        gpiod_line_set_value(lineGreen, (i & 2) != 0);
        gpiod_line_set_value(lineYellow, (i & 4) != 0);
        i++;

        if (sendto(sfd, &received_data, sizeof(received_data), 0, (struct sockaddr *)&claddr, len) !=
            numBytes)
            fatal("sendto");
    }
}
