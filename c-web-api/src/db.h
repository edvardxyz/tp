#pragma once

#include <sqlite3.h>
#include <jansson.h>

json_t * fetch_query_results(const char * sql, const char * fmt, ...);
int init_db();
extern sqlite3 * db;

