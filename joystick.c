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
#include <inttypes.h>
#include <string.h>
#include <linux/joystick.h>
#include <pthread.h>

#include "joystick.h"
#include "joystick_remote.h"

static uint8_t skycontroller_buttons[JOYSTICK_NUM_MODES] = { 8, 9, 2, 0, 1, 3};
static uint8_t xbox360_buttons[JOYSTICK_NUM_MODES] = { 0, 1, 2, 3, 4, 5};
static uint8_t ps3_buttons[JOYSTICK_NUM_MODES] = { 0, 1, 2, 3, 5, 4};
static uint8_t nvdashield_buttons[JOYSTICK_NUM_MODES] = { 0, 1, 2, 3, 4, 5};
static uint8_t custom_buttons[JOYSTICK_NUM_MODES];
static struct joystick_axis skycontroller_axes[JOYSTICK_NUM_AXIS] = {{2, 1}, {3, -1}, {1, -1}, {0, 1}};
static struct joystick_axis xbox360_axes[JOYSTICK_NUM_AXIS] = {{3, 1}, {4, 1}, {1, -1}, {0, 1}};
static struct joystick_axis ps3_axes[JOYSTICK_NUM_AXIS] = { {2, 1}, {3, -1}, {1, -1}, {0, 1}};
static struct joystick_axis nvdashield_axes[JOYSTICK_NUM_AXIS] = {{0, 1}, {1, 1}, {3, 1}, {2, 1}};
static struct joystick_axis custom_axes[JOYSTICK_NUM_AXIS];

#define JOYSTICK_AXIS_MIN -32767.0f
#define JOYSTICK_AXIS_MAX  32767.0f
#define JOYSTICK_PWM_MIN   1100.0f
#define JOYSTICK_PWM_MAX   1900.0f

static const struct joystick_pwms def_pwms = {1500, 1500, 1500, 1500, 1500};
/* these values are in the middle of the ranges that are documented
 * in the arducopter parameter list as Flight Mode 1-6 */
static const uint16_t mode_pwm_values[JOYSTICK_NUM_MODES] = { 1165, 1295, 1425, 1555, 1685, 1815 };

static uint16_t axis_to_pwm(int16_t value)
{
    float value_fl = value;

    return (uint16_t)((value_fl - JOYSTICK_AXIS_MIN)/
            (JOYSTICK_AXIS_MAX - JOYSTICK_AXIS_MIN) *
            (JOYSTICK_PWM_MAX - JOYSTICK_PWM_MIN) + JOYSTICK_PWM_MIN);
}

int mapping_valid(char *mapping)
{
  int arg_counter = 0;
  int expected_args = 13;

  while(*mapping != '\0')
    {
      if(*mapping == ',')
        {
          arg_counter++;
        }

      mapping++;
    }

  return arg_counter == expected_args;
}

int init_convert_mapping(char *mapping)
{
  uint8_t axes[JOYSTICK_NUM_AXIS];
  int8_t direction[JOYSTICK_NUM_AXIS];
  uint8_t buttons[JOYSTICK_NUM_MODES];

  if(!mapping_valid(mapping))
    {
      return -1;
    }

  char *format_string = "%"SCNu8",%"SCNu8",%"SCNu8",%"SCNu8",%"SCNu8",%"SCNu8","
                        "%"SCNu8",%"SCNd8",%"SCNu8",%"SCNd8",%"SCNu8",%"SCNd8","
                        "%"SCNu8",%"SCNd8;
  sscanf(mapping, format_string,
         &buttons[0], &buttons[1], &buttons[2],
         &buttons[3], &buttons[4], &buttons[5],
         &axes[0], &direction[0], &axes[1], &direction[1],
         &axes[2], &direction[2], &axes[3], &direction[3]);

  debug_printf("Mapping: %s \n", mapping);

  debug_printf("Axis mappings\n");
  for(int i=0; i<JOYSTICK_NUM_AXIS; i++)
    {
      debug_printf("axes %s: \t %d ", pwm_explain[i], axes[i]);
      debug_printf("direction %d \n", direction[i]);
    }
  debug_printf("\n");

  debug_printf("Button mappings\n");
  for(int i=0; i<JOYSTICK_NUM_MODES; i++)
    {
      debug_printf("%d ", buttons[i]);
    }
  debug_printf("\n");

  /* Add failure check */
  for(int i=0; i<JOYSTICK_NUM_AXIS; i++)
    {
      custom_axes[i].number = axes[i];
      custom_axes[i].direction = direction[i];
    }

  memcpy(&custom_buttons, buttons, sizeof(custom_buttons));

  return 1;
}

static void joystick_handle_axis(struct joystick *joystick, 
                                 uint8_t number, int16_t value)
{
    debug_printf("joystick_handle_axis : %d, %d\n", number, value);
    pthread_mutex_lock(&joystick->mutex);

    if (number == joystick->axes[JOYSTICK_AXIS_ROLL].number) {
        joystick->pwms.roll =
            axis_to_pwm(joystick->axes[JOYSTICK_AXIS_ROLL].direction * value);
    } else if (number == joystick->axes[JOYSTICK_AXIS_PITCH].number) {
        joystick->pwms.pitch =
            axis_to_pwm(joystick->axes[JOYSTICK_AXIS_PITCH].direction * value);
    } else if (number == joystick->axes[JOYSTICK_AXIS_THROTTLE].number) {
        joystick->pwms.throttle =
            axis_to_pwm(joystick->axes[JOYSTICK_AXIS_THROTTLE].direction * value);
    } else if (number == joystick->axes[JOYSTICK_AXIS_YAW].number) {
        joystick->pwms.yaw =
            axis_to_pwm(joystick->axes[JOYSTICK_AXIS_YAW].direction * value);
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

    /* only take button presses into account */
    if (value != 1)
        goto end;

    if (number == joystick->buttons[JOYSTICK_BUTTON_MODE1]) {
        joystick->pwms.mode = mode_pwm_values[JOYSTICK_BUTTON_MODE1];
    } else if (number == joystick->buttons[JOYSTICK_BUTTON_MODE2]) {
        joystick->pwms.mode = mode_pwm_values[JOYSTICK_BUTTON_MODE2];
    } else if (number == joystick->buttons[JOYSTICK_BUTTON_MODE3]) {
        joystick->pwms.mode = mode_pwm_values[JOYSTICK_BUTTON_MODE3];
    } else if (number == joystick->buttons[JOYSTICK_BUTTON_MODE4]) {
        joystick->pwms.mode = mode_pwm_values[JOYSTICK_BUTTON_MODE4];
    } else if (number == joystick->buttons[JOYSTICK_BUTTON_MODE5]) {
        joystick->pwms.mode = mode_pwm_values[JOYSTICK_BUTTON_MODE5];
    } else if (number == joystick->buttons[JOYSTICK_BUTTON_MODE6]) {
        joystick->pwms.mode = mode_pwm_values[JOYSTICK_BUTTON_MODE6];
    } else {
        debug_printf("joystick_handle_button : unmapped button\n");
    }
end:
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
    uint8_t n_axes, n_buttons;

    memset(joystick, 0, sizeof(struct joystick));
    memcpy(&joystick->pwms, &def_pwms, sizeof(joystick->pwms));

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

    ret = ioctl(joystick->fd, JSIOCGAXES, &n_axes);
    if (ret == -1) {
        perror("joystick_start - JSIOCGAXES");
        return -1;
    }
    debug_printf("Joystick has %d axes\n", n_axes);

    ret = ioctl(joystick->fd, JSIOCGBUTTONS, &n_buttons);
    if (ret == -1) {
        perror("joystick_start - JSIOCGBUTTONS");
        return -1;
    }
    debug_printf("Joystick has %d buttons\n", n_buttons);
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

int joystick_set_type(struct joystick *joystick, char *type, char *mapping)
{
    int ret = 0;

    pthread_mutex_lock(&joystick->mutex);
    if (!strcmp(type, "x") || !strcmp(type, "xbox360")) {
        memcpy(&joystick->buttons, xbox360_buttons, sizeof(joystick->buttons));
        memcpy(&joystick->axes, xbox360_axes, sizeof(joystick->axes));
    } else if (!strcmp(type, "s") || !strcmp(type, "skycontroller")) {
        memcpy(&joystick->buttons, skycontroller_buttons, sizeof(joystick->buttons));
        memcpy(&joystick->axes, skycontroller_axes, sizeof(joystick->axes));
    } else if (!strcmp(type, "ps3") || !strcmp(type, "playstation3")) {
        memcpy(&joystick->buttons, ps3_buttons, sizeof(joystick->buttons));
        memcpy(&joystick->axes, ps3_axes, sizeof(joystick->axes));
    } else if (!strcmp(type, "nvda") || !strcmp(type, "nvidiashield")) {
        memcpy(&joystick->buttons, nvdashield_buttons, sizeof(joystick->buttons));
        memcpy(&joystick->axes, nvdashield_axes, sizeof(joystick->axes));
    } else if (!strcmp(type, "c") || !strcmp(type, "custom")) {
        if(init_convert_mapping(mapping) == -1){
            debug_printf("Joystick mapping isn't valid. Check that all paramaters is supplied\n");
            ret = -1;
        } else {
            memcpy(&joystick->buttons, custom_buttons, sizeof(joystick->buttons));
            memcpy(&joystick->axes, custom_axes, sizeof(joystick->axes));
        }
    } else {
        debug_printf("bad joystick type\n");
        ret = -1;
    }
    pthread_mutex_unlock(&joystick->mutex);
    return ret;
}
