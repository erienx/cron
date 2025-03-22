#include "server.h"
#include "log.h"


mqd_t mq = (mqd_t)-1;
pthread_mutex_t mtx_task = PTHREAD_MUTEX_INITIALIZER;
struct task_t tasks[TASK_LIMIT];

int sec_left(const char *datetime) {
    int year, month, day, hour, minute, second;
    if (sscanf(datetime, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return -1;
    }

    time_t now;
    time(&now);

    struct tm target_time = {0};
    target_time.tm_year = year - 1900;
    target_time.tm_mon = month - 1;
    target_time.tm_mday = day;
    target_time.tm_hour = hour;
    target_time.tm_min = minute;
    target_time.tm_sec = second;

    time_t target_time_seconds = mktime(&target_time);
    if (target_time_seconds == -1) {
        return -1;
    }

    double seconds_diff = difftime(target_time_seconds, now);
    return (int)seconds_diff;
}
void timer_thread(union sigval arg)
{
    struct task_t *task = (struct task_t *)arg.sival_ptr;

    pthread_mutex_lock(&mtx_task);


    if (task->active) {
        pid_t pid = fork();
        if (pid == 0) {
            pthread_mutex_unlock(&mtx_task);
            execl(task->path, task->path, (char *)NULL);

            perror("failed to execute program");
        } else if (pid < 0) {
            perror("fork failed");
        } else {
            printf("task exec started pid:%d\n", pid);
        }
    }

    if (!task->cyclic) {
        task->active = 0;
        timer_delete(task->timer);
    }

    pthread_mutex_unlock(&mtx_task);
}

//void timer_thread(union sigval arg)
//{
//    struct task_t *task = (struct task_t *)arg.sival_ptr;
//    pthread_mutex_lock(&mtx_task);
//    printf("%s %d\n", task->path, task->time);
//
//    if (task->cyclic){
//        pthread_mutex_unlock(&mtx_task);
//        return;
//    }
//
//    task->active = 0;
//    timer_delete(task->timer);
//    pthread_mutex_unlock(&mtx_task);
//}

int task_idx = 0;



int add_task(struct query_t *query)
{
    if(query->path_to_exe == NULL)
        return 1;

    int idx = task_idx;
    if (task_idx == TASK_LIMIT) {
        for (int i = 0; i<TASK_LIMIT; i++){
            if (tasks[i].active == 0){
                idx = i;
                break;
            }
        }
        if (idx == TASK_LIMIT){
            return 2;
        }
    }

    int time = 0;
    pthread_mutex_lock(&mtx_task);
    tasks[idx].cyclic = 0;
    if (query->add_mode == ADD_RELATIVE || query->add_mode == ADD_CYCLIC){
        time = atoi(query->datetime);
    }
    else if (query->add_mode == ADD_ABSOLUTE){
        time = sec_left(query->datetime);
        if (time<1){
            pthread_mutex_unlock(&mtx_task);
            return 3;
        }
    }
    if (query->add_mode == ADD_CYCLIC){
        tasks[idx].cyclic=1;
    }
//    printf("%d\n",time);



    strcpy(tasks[idx].path , query->path_to_exe);
    tasks[idx].time = time;
    tasks[idx].active = 1;


    struct sigevent timer_event2;
    timer_event2.sigev_notify = SIGEV_THREAD;
    timer_event2.sigev_notify_function = timer_thread;
    timer_event2.sigev_value.sival_ptr = &tasks[idx];
    timer_event2.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &timer_event2, &(tasks[idx].timer));

    struct itimerspec value;
    value.it_value.tv_sec = time;
    value.it_value.tv_nsec = 0;
    if (tasks[idx].cyclic) {
        value.it_interval.tv_sec = time;
        value.it_interval.tv_nsec = 0;
    } else {
        value.it_interval.tv_sec = 0;
        value.it_interval.tv_nsec = 0;
    }

    timer_settime(tasks[idx].timer, 0, &value, NULL);
    printf("Task added %d %d\n", idx, tasks[idx].time);
    if (task_idx == idx) {
        task_idx++;
    }
    log_to_file(2, "task index %d has been added", idx);
    pthread_mutex_unlock(&mtx_task);

    return 0;
}

int is_server_running() {
    mqd_t mq_test = mq_open(MQ_NAME, O_WRONLY | O_NONBLOCK);
    if (mq_test == (mqd_t)-1) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }
    mq_close(mq_test);
    return 1;
}
void clean(int sig) {
    if (mq != (mqd_t)-1) {
        mq_close(mq);
        mq_unlink(MQ_NAME);
        printf("\ncleared queue\n");
    }
    exit(0);
}
void handle_list(struct query_t *query, struct query_t *reply_query){

    pthread_mutex_lock(&mtx_task);
    log_to_file(2, "list tasks called, %d currently have %d tasks", task_idx);
    if (task_idx==0){
        strcpy(reply_query->msg, "no tasks are currently running");
        pthread_mutex_unlock(&mtx_task);
        return;
    }
    char buff[256];
    reply_query->msg[0] = '\0';

    for (int i = 0; i < task_idx; i++) {
        sprintf(buff, "task %d: active: %d, path: %s, time: %ds, cyclic: %d\n", i, tasks[i].active, tasks[i].path, tasks[i].time, tasks[i].cyclic);
        strncat(reply_query->msg, buff, sizeof(reply_query->msg) - strlen(reply_query->msg) - 1);
    }
    pthread_mutex_unlock(&mtx_task);
}

void handle_cancel(struct query_t *query, struct query_t *reply_query){
    int ix_to_cancel = query->index_cancel;
    log_to_file(2, "cancel task with index %d called", ix_to_cancel);
    if (ix_to_cancel<0 || ix_to_cancel>=task_idx){
        strcpy(reply_query->msg, "task not cancelled - index doesnt exist");
        return;
    }
    pthread_mutex_lock(&mtx_task);
    if (tasks[ix_to_cancel].active == 0){
        strcpy(reply_query->msg, "task with that index was already cancelled");
        pthread_mutex_unlock(&mtx_task);
        return;
    }
    tasks[ix_to_cancel].active = 0;
    timer_delete(tasks[ix_to_cancel].timer);
    pthread_mutex_unlock(&mtx_task);
    strcpy(reply_query->msg, "task has been cancelled");

}
void handle_add(struct query_t *query, struct query_t *reply_query){
    log_to_file(0, "add task has been called");
    int code = add_task(query);
    if (code==2){
        strcpy(reply_query->msg, "task not added - task limit reached");
        return;
    }
    if (code == 3){
        strcpy(reply_query->msg, "task not added - a past date has been passed");
        return;
    }

    strcpy(reply_query->msg, "task successfully added");
}

void start_server() {
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct query_t);
    attr.mq_flags = 0;

    mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0664, &attr);
    if (mq == (mqd_t)-1) {
        perror("Failed to create message queue");
        exit(1);
    }
    printf("Server started and waiting for client messages...\n");

    while (1) {
        struct query_t query;

        if (mq_receive(mq, (char *)&query, sizeof(struct query_t), NULL) == -1) {
            continue;
        }

        mqd_t mq_reply = mq_open(query.reply_queue_name, O_WRONLY);
        if (mq_reply == (mqd_t)-1) {
            perror("Failed to open reply queue");
            continue;
        }

        struct query_t reply_query = {0};
        strncpy(reply_query.msg, "message received but not processed", sizeof(reply_query.msg));

        printf("reply q name: %s\n", query.reply_queue_name);

        switch (query.command){
            case ADD:
                printf("ADD\n");
                handle_add(&query, &reply_query);
                break;
            case LIST:
                printf("LIST\n");
                handle_list(&query, &reply_query);
                break;
            case CANCEL:
                printf("CANCEL\n");
                handle_cancel(&query, &reply_query);
                break;
            default:
                break;
        }

        if (mq_send(mq_reply, (char *)&reply_query, sizeof(reply_query), 0) == -1) {
            perror("failed to send reply");
        }
        mq_close(mq_reply);
    }

    mq_close(mq);
    mq_unlink(MQ_NAME);
}