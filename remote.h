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

#ifndef _REMOTE_H_
#define _REMOTE_H_
#include "RCInput_UDP_Protocol.h"

struct remote {
    int fd;
    struct addrinfo *res;
};

int remote_start(char *remote_host, struct remote *remote);
void remote_send_pwms(struct remote *remote, uint16_t *pwms,
                      uint8_t len, uint64_t micro64);
#endif // _REMOTE_H_
