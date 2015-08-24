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

#ifndef _JOYSTICK_H_
#define _JOYSTICK_H_

#define MAX_NAME_LEN 128
#define MAX_NUM_BUTTONS 128
#define MAX_NUM_MODES 6

enum {
    JOYSTICK_AXIS_ROLL,
    JOYSTICK_AXIS_PITCH,
    JOYSTICK_AXIS_THROTTLE,
    JOYSTICK_AXIS_YAW,
    JOYSTICK_NUM_AXIS
};

struct joystick_pwms {
    uint16_t roll;
    uint16_t pitch;
    uint16_t throttle;
    uint16_t yaw;
    uint16_t mode;
};

struct joystick_axis {
    uint8_t number;
    /* 1 or -1 */
    int8_t direction;
};

struct joystick {
    char name[MAX_NAME_LEN];
    uint8_t axes;
    uint8_t buttons;
    int fd;
    pthread_t thread;
    pthread_mutex_t mutex;

    struct joystick_pwms pwms;

    /* buttons mapping */
    uint8_t mode_button[MAX_NUM_BUTTONS];

    /* axis mapping */
    struct joystick_axis axis[JOYSTICK_NUM_AXIS];
    
    /* modes pwm mapping */
    uint16_t mode_pwm[MAX_NUM_MODES];
};

int joystick_start(char *path, struct joystick *joystick);
void joystick_get_pwms(struct joystick *joystick, uint16_t *pwms, uint8_t *len);
void joystick_set_calib(struct joystick *joystick, uint8_t *buttons,
                        uint16_t *pwms, struct joystick_axis *axis);

#endif // _JOYSTICK_H_
