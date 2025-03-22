#include "common.h"
#include "server.h"
#include "client.h"
#include "log.h"


/*
  sample client-side commands:
 ./main -m relative -t 2 -p test_program
 ./main -m cyclic -t 5 -p test_program
 ./main -m absolute -t 2025-01-01T13:50:20 -p test_program
 ./main -l
 ./main -c 0
 */

int main(int argc, char *argv[]) {


    if (is_server_running()) {
        struct query_t query;
        if (get_query(&query, argc,argv)==1){
            perror("incorrect arguments\n");
            helper();
            exit(0);
        }
        run_client(query);
    } else {
        struct sigaction sa;
        sa.sa_handler = clean;
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);

        initialize_logging();
        start_server();
    }
    return 0;
}
