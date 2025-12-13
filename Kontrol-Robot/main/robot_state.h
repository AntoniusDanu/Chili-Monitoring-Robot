#ifndef ROBOT_STATE_H
#define ROBOT_STATE_H

typedef enum {
    ROBOT_RUN = 0,
    ROBOT_STOP,
    ROBOT_ERROR
} robot_state_t;

extern volatile robot_state_t robot_state;

#endif

