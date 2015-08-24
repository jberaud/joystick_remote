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
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <linux/joystick.h>
#include <pthread.h>

#include "joystick.h"
#include "joystick_remote.h"

#define JOYSTICK_AXIS_MIN -32767.0f
#define JOYSTICK_AXIS_MAX  32767.0f
#define JOYSTICK_PWM_MIN   1100.0f
#define JOYSTICK_PWM_MAX   1900.0f

static uint16_t axis_to_pwm(int16_t value)
{
    float value_fl = value;

    return (uint16_t)((value_fl - JOYSTICK_AXIS_MIN)/
            (JOYSTICK_AXIS_MAX - JOYSTICK_AXIS_MIN) *
            (JOYSTICK_PWM_MAX - JOYSTICK_PWM_MIN) + JOYSTICK_PWM_MIN);
}

static void joystick_handle_axis(struct joystick *joystick, 
                                 uint8_t number, int16_t value)
{
    debug_printf("joystick_handle_axis : %d, %d\n", number, value);
    pthread_mutex_lock(&joystick->mutex);

    if (number == joystick->axis[JOYSTICK_AXIS_ROLL].number) {
        joystick->pwms.roll = 
            axis_to_pwm(joystick->axis[JOYSTICK_AXIS_ROLL].direction * value);
    } else if (number == joystick->axis[JOYSTICK_AXIS_PITCH].number) {
        joystick->pwms.pitch = 
            axis_to_pwm(joystick->axis[JOYSTICK_AXIS_PITCH].direction * value);
    } else if (number == joystick->axis[JOYSTICK_AXIS_THROTTLE].number) {
        joystick->pwms.throttle =
            axis_to_pwm(joystick->axis[JOYSTICK_AXIS_THROTTLE].direction * value);
    } else if (number == joystick->axis[JOYSTICK_AXIS_YAW].number) {
        joystick->pwms.yaw = 
            axis_to_pwm(joystick->axis[JOYSTICK_AXIS_YAW].direction * value);
    }
    else {
        debug_printf("joystick_handle_axis : unmapped axis\n");
    }
    pthread_mutex_unlock(&joystick->mutex);
}

static void joystick_handle_button(struct joystick *joystick,
                                   uint8_t number, int16_t value)
{
    debug_printf("joystick_handle_button : %d, %d\n", number, value);
    pthread_mutex_lock(&joystick->mutex);

    if (value == 1) {
        /* button pressed */
        uint8_t mode_button = joystick->mode_button[number];

        /* registered button */
        joystick->pwms.mode = joystick->mode_pwm[mode_button];
    }

    pthread_mutex_unlock(&joystick->mutex);
}

static void *joystick_thread(void *arg)
{
    struct js_event event;
    struct pollfd pollfd;
    struct joystick *joystick = (struct joystick *) arg;
    int ret;
    
    debug_printf("starting joystick event listener\n");

    pollfd.fd = joystick->fd;
    pollfd.events = POLLIN | POLLHUP;

    while (1) {
        /* wait for an event on the joystick, no timeout */
        ret = poll(&pollfd, 1, -1);
        if (ret == -1) {
            perror("joystick_thread - poll");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "joystick_thread : unexpected timeout\n");
            break;
        } else if (pollfd.revents & POLLHUP) {
            fprintf(stderr, "joystick disconnected\n");
            break;
        }
        ret = read(joystick->fd, &event, sizeof(event));
        if (ret < 0) {
            perror("joystick_thread - read\n");
            break;
        }

        /* remove init flag in order not to differentiate between
         * initial virtual events and joystick events */
        event.type &= ~JS_EVENT_INIT;

        switch (event.type) {
        case JS_EVENT_AXIS:
            joystick_handle_axis(joystick, event.number, event.value);
            break;
        case JS_EVENT_BUTTON:
            joystick_handle_button(joystick, event.number, event.value);
            break;
        default:
            fprintf(stderr, "joystick_thread : unexpected event %d\n", event.type);
        }
    }
    
    exit(EXIT_SUCCESS);

    return NULL;
}

int joystick_start(char *path, struct joystick *joystick)
{
    int ret;
    pthread_attr_t attr;

    memset(joystick, 0, sizeof(struct joystick));

    joystick->fd = open(path, O_RDONLY);

    if (joystick->fd == -1) {
        perror("joystick_start - open");
        return -1;
    }

    ret = ioctl(joystick->fd, JSIOCGNAME(sizeof(joystick->name)),
                &joystick->name);
    if (ret == -1) {
        perror("joystick_start - JSIOCGNAME");
        return -1;
    }
    debug_printf("Joystick : %s\n", joystick->name);

    ret = ioctl(joystick->fd, JSIOCGAXES, &joystick->axes);
    if (ret == -1) {
        perror("joystick_start - JSIOCGAXES");
        return -1;
    }
    debug_printf("Joystick has %d axes\n", joystick->axes);

    ret = ioctl(joystick->fd, JSIOCGBUTTONS, &joystick->buttons);
    if (ret == -1) {
        perror("joystick_start - JSIOCGBUTTONS");
        return -1;
    }
    debug_printf("Joystick has %d buttons\n", joystick->buttons);
    ret = pthread_mutex_init(&joystick->mutex, NULL);
    if (ret != 0) {
        perror("joystick_start - pthread_mutex_init");
        return -1;
    }
    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        perror("joystick_start - pthread_attr_init");
        return -1;
    }
    ret = pthread_create(&joystick->thread, &attr, &joystick_thread, joystick);
    if (ret != 0) {
        perror("joystick_start - pthread_create");
        return -1;
    }

    return 0;
}

void joystick_get_pwms(struct joystick *joystick, uint16_t *pwms, uint8_t *len)
{
    pthread_mutex_lock(&joystick->mutex);
    *len = sizeof(joystick->pwms);
    memcpy(pwms, &joystick->pwms, *len);
    pthread_mutex_unlock(&joystick->mutex);

    return;
}

void joystick_set_calib(struct joystick *joystick, uint8_t *buttons,
                        uint16_t *mode_pwm, struct joystick_axis *axis)
{
    pthread_mutex_lock(&joystick->mutex);
    memcpy(&joystick->mode_button, buttons, sizeof(joystick->mode_button));
    memcpy(&joystick->mode_pwm, mode_pwm, sizeof(joystick->mode_pwm));
    memcpy(&joystick->axis, axis, sizeof(joystick->axis));
    pthread_mutex_unlock(&joystick->mutex);
}
    
