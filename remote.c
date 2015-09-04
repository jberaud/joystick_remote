/*
    This file is part of joystick_remote.

    joystick_remote is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or any later version.

    joystick_remote is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with joystick_remote.  
    If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "joystick.h"
#include "remote.h"
#include "joystick_remote.h"

int remote_start(char *remote_host, struct remote *remote)
{
    char *remote_addr, *remote_port;
    struct addrinfo info;
    int ret;

    memset(remote, 0, sizeof(*remote));
    remote_addr = remote_host;

    remote_port = strchr(remote_host, ':');
    if (remote_port == NULL) {
        fprintf(stderr, "remote_start : no port specified\n");
        goto err_arg;
    }

    *remote_port = '\0';
    remote_port++;
    debug_printf("remote addr : %s, remote port : %s\n", remote_addr, remote_port);

    memset(&info, 0, sizeof(info));
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_DGRAM;
    info.ai_protocol = 0;
    info.ai_flags = AI_ADDRCONFIG;
    ret = getaddrinfo(remote_addr, remote_port, &info, &remote->res);
    if (ret != 0) {
        fprintf(stderr, "remote_start - getaddrinfo : %s\n", gai_strerror(ret));
        goto err_arg;
    }

    remote->fd = socket(remote->res->ai_family,
                        remote->res->ai_socktype,
                        remote->res->ai_protocol);
    if (remote->fd == -1) {
        perror("remote_start - socket");
        goto err_addrinfo;
    }

    return 0;
err_addrinfo:
    freeaddrinfo(remote->res);
err_arg:
    return -1;
}

void remote_send_pwms(struct remote *remote, uint16_t *pwms,
                      uint8_t len, uint64_t micro64)
{
    struct rc_udp_packet msg;
    int ret;

    /* to check compatibility */
    memset(&msg, 0, sizeof(msg));
    msg.version = RCINPUT_UDP_VERSION;
    msg.timestamp_us = micro64;
    msg.sequence++;

    if (len > sizeof(msg.pwms)) {
        fprintf(stderr, "remote_send_pwms : bad len %d\n", len);
        return;
    }
    memcpy(&msg.pwms, pwms, len);
    ret = sendto(remote->fd, &msg, sizeof(msg), 0,
            remote->res->ai_addr, remote->res->ai_addrlen);
    if (ret == -1) {
        perror("remote_send_pwms - socket");
        return;
    }
    return;
}
