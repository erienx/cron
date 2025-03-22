#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define MQ_NAME "/mq_que"
#define QUE_LENGTH 128
#define MSG_LENGTH (2048 - QUE_LENGTH)
#define PATH_LENGTH 128
#define DATETIME_LENGTH 128

enum command_t {
    ADD, LIST, CANCEL, ERROR
};
enum add_mode_t {
    ADD_ABSOLUTE, ADD_RELATIVE, ADD_CYCLIC, DOESNT_APPLY
};
struct query_t {
    char reply_queue_name[QUE_LENGTH];
    enum command_t command;
    enum add_mode_t add_mode;
    int index_cancel;
    char path_to_exe[PATH_LENGTH];
    char datetime[DATETIME_LENGTH];
    char msg[MSG_LENGTH];
};
