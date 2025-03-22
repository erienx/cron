#pragma once
#include "common.h"


int get_query(struct query_t *query, int argc, char *argv[]);
void run_client(struct query_t queryy);
void helper();
int is_datetime_valid(const char *datetime);
int is_uint(const char *str);