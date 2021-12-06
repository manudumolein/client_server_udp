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

#include <stdio.h>
#include <unistd.h>

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include "tlpi_hdr.h"
#include "PJ_RPI.h"

struct itimerval itv;
struct sigaction sa;

int sfd;
ssize_t numBytes;
socklen_t len;
struct sockaddr_un claddr;
int toggle;
int port;

static void toggleIO(int sig)
{
    if (toggle)
    {
        GPIO_SET = (1 << port);
        printf("port on: %d\r\n", port);
        toggle = 0;
    }
    else
    {
        GPIO_CLR = (1 << port);
        printf("port off: %d\r\n", port);
        toggle = 1;
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_un svaddr;

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

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = toggleIO;
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        errExit("sigaction");

    itv.it_value.tv_sec = 3;
    itv.it_value.tv_usec = 0;
    itv.it_interval.tv_sec = 3;
    itv.it_interval.tv_usec = 0;

    if (map_peripheral(&gpio) == -1)
    {
        printf("Failed to map the physical GPIO registers into the virtual memory space.\n");
        return -1;
    }

    for (;;)
    {
        t_data received_data;
        len = sizeof(struct sockaddr_un);
        numBytes = recvfrom(sfd, &received_data, sizeof(received_data), 0, (struct sockaddr *)&claddr, &len);

        if (numBytes != -1)
        {
            port = received_data.IO;
            itv.it_interval.tv_sec = received_data.period;

            INP_GPIO(port);
            OUT_GPIO(port);

            if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
                errExit("setitimer");

            printf("Received %d: %d \n", received_data.IO, received_data.period);
            if (sendto(sfd, &received_data, sizeof(received_data), 0, (struct sockaddr *)&claddr, len) !=
                numBytes)
                fatal("sendto");
        }
    }
}
