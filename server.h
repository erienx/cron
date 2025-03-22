#pragma once
#include "common.h"
#include <pthread.h>

#define TASK_LIMIT 10

struct task_t {
    char path[128];
    int time;
    timer_t timer;
    int active;
    int cyclic;
};

void start_server();
int is_server_running();
void clean(int sig);
int add_task(struct query_t *query);
void timer_thread(union sigval arg);
int sec_left(const char *datetime);


