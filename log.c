#include "log.h"
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mtx_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_dump = PTHREAD_MUTEX_INITIALIZER;

volatile sig_atomic_t flag_dump = 0;
volatile sig_atomic_t flag_toggle_log = 0;
volatile sig_atomic_t flag_log_level = 0;

atomic_int current_lvl = ATOMIC_VAR_INIT(0);
atomic_int log_status = ATOMIC_VAR_INIT(1);

FILE* file_for_logging = NULL;

void assign_current_time(char* time_str,int str_size){
    time_t raw_time;
    struct tm *timeinfo;

    time (&raw_time);
    timeinfo = localtime (&raw_time);
    strftime(time_str, str_size, "%d-%m-%Y %H:%M:%S", timeinfo);
}

void log_to_file(int importance, char* msg, ...){
    if (atomic_load(&log_status)==0){
        return;
    }
    if (importance < atomic_load(&current_lvl)){
        return;
    }

    pthread_mutex_lock(&mtx_log);
    if (file_for_logging == NULL){
        file_for_logging = fopen("logs.txt", "a");
        if (file_for_logging == NULL){
            perror("couldn't open file");
            pthread_mutex_unlock(&mtx_log);
            return;
        }
    }

    char time_str[32];
    assign_current_time(time_str, sizeof(time_str));
    va_list args;
    va_start(args,msg);

    fprintf(file_for_logging,"%s  <priority: %d> message: ", time_str, importance);
    vfprintf(file_for_logging,msg,args);
    fprintf(file_for_logging, "\n");
    fflush(file_for_logging);

    va_end(args);
    pthread_mutex_unlock(&mtx_log);
}
void dump(){
    pthread_mutex_lock(&mtx_dump);
    char time_str[32];
    assign_current_time(time_str,sizeof(time_str));
    char filename[64];
    sprintf(filename,"dump-%s.txt", time_str);
    FILE *f = fopen(filename, "w");
    if (!f){
        perror("dump file couldnt be opened");
        pthread_mutex_unlock(&mtx_dump);
        return;
    }
    fprintf(f, "Dump, current app info:\n");
    fprintf(f, "logging level: %d\nlog toggle: %d\n", atomic_load(&current_lvl), atomic_load(&log_status));
    fclose(f);
    log_to_file(1,"dump file created");
    pthread_mutex_unlock(&mtx_dump);
}

void handler_dump(int signo, siginfo_t* info, void* other) {
    flag_dump = 1;
}

void handler_toggle_log(int signo, siginfo_t* info, void* other) {
    flag_toggle_log = 1;
}

void handler_log_level(int signo, siginfo_t* info, void* other) {
    flag_log_level = 1;
}


void* thread_signal(void* arg) {
    sigset_t set;
    sigfillset(&set);

    sigdelset(&set, SIG1);
    sigdelset(&set, SIG2);
    sigdelset(&set, SIG3);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    while (1) {
        if (flag_dump) {
            log_to_file(0,"dump generation requested");
            dump();
            flag_dump = 0;
        }

        if (flag_toggle_log) {
            log_to_file(0,"flag toggle requested");
            int new_status = !atomic_load(&log_status);
            atomic_store(&log_status, new_status);

            log_to_file(1,"toggle changed to %d", new_status);
            flag_toggle_log = 0;
        }

        if (flag_log_level) {
            log_to_file(0,"log level change requested");
            int new_lvl = (atomic_load(&current_lvl)+1)%3;
            atomic_store(&current_lvl, new_lvl);
            log_to_file(2,"log level changed to %d", new_lvl);
            flag_log_level = 0;
        }

        sleep(1);
    }

    return NULL;
}

void initialize_logging() {


    atomic_store(&current_lvl, 0);
    printf("%d\n",getpid());
    log_to_file(2,"started the program with pid: %d", getpid());

    pthread_t sig_th;
    pthread_create(&sig_th, NULL, thread_signal, NULL);

    struct sigaction act;
    sigset_t set;
    sigfillset(&set);

    act.sa_sigaction = handler_dump;
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIG1, &act, NULL);

    act.sa_sigaction = handler_toggle_log;
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIG2, &act, NULL);

    act.sa_sigaction = handler_log_level;
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIG3, &act, NULL);

    act.sa_handler = SIG_IGN;
    for (int i = SIG3 + 1; i <= SIGRTMAX; i++) {
        sigaction(i, &act, NULL);
    }

    sigdelset(&set, SIG1);
    sigdelset(&set, SIG2);
    sigdelset(&set, SIG3);
    sigdelset(&set, SIGINT);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
}
