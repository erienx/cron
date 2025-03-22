#include "client.h"
#include <ctype.h>
#include <time.h>

int is_datetime_valid(const char *datetime) {
    int year, month, day, hour, minute, second;
    //2024-12-31T23:59:59
    if (sscanf(datetime, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return 0;
    }

    if (year < 1900 || year > 9999)
        return 0;
    if (month < 1 || month > 12)
        return 0;
    if (day < 1 || day > 31)
        return 0;
    if (hour < 0 || hour > 23)
        return 0;
    if (minute < 0 || minute > 59)
        return 0;
    if (second < 0 || second > 59)
        return 0;

    if (month == 2) {
        int is_leap_year = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if (is_leap_year){
            if (day>29)
                return 0;
        }
        else{
            if (day>28)
                return 0;
        }

    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        if (day > 30)
            return 0;
    } else {
        if (day > 31)
            return 0;
    }
    return 1;
}

int is_uint(const char *str) {
    int i = 0;
    while (*str){
        if (!isdigit(*str) ){
            return 0;
        }
        str++;
        i++;
    }
    return 1;
}

int get_query(struct query_t *query, int argc, char *argv[]){
    memset(query, 0, sizeof(struct query_t));
    if (argc<=1){
        return 1;
    }
    query->command = ERROR;
    query->add_mode = DOESNT_APPLY;

    int option;

    while ((option = getopt(argc, argv, "m:t:p:lc:")) != -1) {
        switch (option) {
            case 'm': {
                query->command = ADD;
                if (strcmp(optarg, "absolute") == 0) {
                    query->add_mode = ADD_ABSOLUTE;
                } else if (strcmp(optarg, "relative") == 0) {
                    query->add_mode = ADD_RELATIVE;
                } else if (strcmp(optarg, "cyclic") == 0) {
                    query->add_mode = ADD_CYCLIC;
                } else {

                    printf("%s is an invalid argument for -m", optarg);
                    return 1;
                }
                break;
            }
            case 't': {
                strncpy(query->datetime, optarg, DATETIME_LENGTH - 1);
                if (query->add_mode==ADD_ABSOLUTE) {
                    if (!is_datetime_valid(query->datetime)) {
                        printf("datetime format is incorrect\n");
                        return 1;
                    }
                }
                else if (query->add_mode==ADD_RELATIVE || query->add_mode==ADD_CYCLIC){
                    if (!is_uint(query->datetime)){
                        printf("time format is incorrect\n");
                        return 1;
                    }
                    int n = atoi(query->datetime);
                    if (n<1){
                        printf("time has to be positive\n");
                        return 1;
                    }
                }
                else{
                    printf("-t usage without -m\n");
                    return 1;
                }
                break;
            }
            case 'p': {
                strncpy(query->path_to_exe, optarg, PATH_LENGTH - 1);
                break;
            }
            case 'l': {
                query->command = LIST;
                break;
            }
            case 'c': {
                query->command = CANCEL;
                if (!is_uint(optarg)){
                    printf("cancel arg is not an int\n");
                    return 1;
                }
                query->index_cancel = atoi(optarg);
                break;
            }
            default:
                return 1;
        }
    }

    if (query->command == ADD && (query->path_to_exe[0] == '\0' || query->datetime[0] == '\0')) {
        printf("no path or datetime specified for ADD");
        return 1;
    }

    return 0;
}

void run_client(struct query_t query_to_send) {
    mqd_t mq_client_send = mq_open(MQ_NAME, O_WRONLY);
    if (mq_client_send == (mqd_t)-1) {
        perror("Failed to open message queue");
        exit(1);
    }

    char reply_queue_name[QUE_LENGTH];
    snprintf(reply_queue_name, sizeof(reply_queue_name), "/mq_reply_%d", getpid());
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct query_t);
    attr.mq_flags = 0;
    mqd_t mq_client_reply = mq_open(reply_queue_name, O_CREAT | O_RDONLY, 0664, &attr);
    if (mq_client_reply == (mqd_t)-1) {
        perror("Failed to create reply queue");
        mq_close(mq_client_send);
        exit(1);
    }

    strncpy(query_to_send.reply_queue_name, reply_queue_name, sizeof(query_to_send.reply_queue_name));
    strncpy(query_to_send.msg, "hello", sizeof(query_to_send.msg));

    if (mq_send(mq_client_send, (char *)&query_to_send, sizeof(query_to_send), 0) == -1) {
        perror("failed to send message");
        mq_close(mq_client_send);
        mq_close(mq_client_reply);
        mq_unlink(reply_queue_name);
        exit(1);
    }
    mq_close(mq_client_send);

    struct query_t reply_query = {0};
    if (mq_receive(mq_client_reply, (char *)&reply_query, sizeof(reply_query), NULL) == -1) {
        perror("failed to receive reply");
        mq_close(mq_client_reply);
        mq_unlink(reply_queue_name);
        exit(1);
    }

    printf("server reply:\n%s\n", reply_query.msg);

    mq_close(mq_client_reply);
    mq_unlink(reply_queue_name);
}

void helper() {
    printf("help:\n");
    printf("./main -m <absolute/relative/cyclic> -t <exact time/in how many seconds/every how many seconds to execute> -p <path to executable>\n");
    printf("./main -l\n");
    printf("./main -c <index>\n");
}