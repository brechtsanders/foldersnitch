#ifndef INCLUDED_SQLITEFUNCTIONS_H
#define INCLUDED_SQLITEFUNCTIONS_H

#include <sqlite3.h>
#include <stdint.h>
#include <unistd.h>

#define RETRY_WAIT_TIME 100
#ifdef __WIN32__
#define WAIT_BEFORE_RETRY(ms) Sleep(ms);
#else
#define WAIT_BEFORE_RETRY(ms) usleep(ms * 1000);
#endif

#ifdef __cplusplus
extern "C" {
#endif

sqlite3_stmt* execute_sql_query (sqlite3* sqliteconn, const char* sql, int* status);
int execute_sql_cmd (sqlite3* sqliteconn, const char* sql);
sqlite3_stmt* execute_sql_query_with_1_parameter (sqlite3* sqliteconn, const char* sql, int* status, int64_t param1);
/* int64_t get_single_value_sql_with_1_parameter (sqlite3* sqliteconn, const char* sql, int64_t param1, int64_t errorvalue); */
int64_t get_single_value_sql_with_2_parameters (sqlite3* sqliteconn, const char* sql, int64_t param1, int64_t param2, int64_t errorvalue);
sqlite3_stmt* execute_sql_query_with_1_string_parameter (sqlite3* sqliteconn, const char* sql, int* status, const char* param1);

#ifdef __cplusplus
}
#endif

#endif //INCLUDED_SQLITEFUNCTIONS_H
