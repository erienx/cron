#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define SIG1 SIGRTMIN
#define SIG2 (SIGRTMIN + 1)
#define SIG3 (SIGRTMIN + 2)


void assign_current_time(char* time_str, int str_size);
void log_to_file(int importance, char* msg, ...);
void dump();
void handler_dump(int signo, siginfo_t* info, void* other);
void handler_toggle_log(int signo, siginfo_t* info, void* other);
void handler_log_level(int signo, siginfo_t* info, void* other);
void* thread_signal(void* arg);
void initialize_logging();
