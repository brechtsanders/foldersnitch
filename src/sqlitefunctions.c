#ifdef WIN32
//#define __MSVCRT_VERSION__ 0x0601
#include <windows.h>
#endif
#include "sqlitefunctions.h"

sqlite3_stmt* execute_sql_query (sqlite3* sqliteconn, const char* sql, int* status)
{
	sqlite3_stmt* sqlresult = NULL;
	if ((*status = sqlite3_prepare_v2(sqliteconn, sql, -1, &sqlresult, NULL)) == SQLITE_OK) {
    while ((*status = sqlite3_step(sqlresult)) == SQLITE_BUSY) {
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    }
    if (*status == SQLITE_ROW || *status == SQLITE_DONE)
      return sqlresult;
	}
	return NULL;
}

int execute_sql_cmd (sqlite3* sqliteconn, const char* sql)
{
  int status;
	sqlite3_stmt* sqlresult;
  if ((sqlresult = execute_sql_query(sqliteconn, sql, &status)) != NULL)
    sqlite3_finalize(sqlresult);
  return status;
}

sqlite3_stmt* execute_sql_query_with_1_parameter (sqlite3* sqliteconn, const char* sql, int* status, int64_t param1)
{
	sqlite3_stmt* sqlresult = NULL;
	if ((*status = sqlite3_prepare_v2(sqliteconn, sql, -1, &sqlresult, NULL)) == SQLITE_OK) {
    sqlite3_bind_int64(sqlresult, 1, param1);
    while ((*status = sqlite3_step(sqlresult)) == SQLITE_BUSY) {
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    }
    if (*status == SQLITE_ROW || *status == SQLITE_DONE)
      return sqlresult;
	}
	return NULL;
}

/*
int64_t get_single_value_sql_with_1_parameter (sqlite3* sqliteconn, const char* sql, int64_t param1, int64_t errorvalue)
{
  int status;
	sqlite3_stmt* sqlresult;
	int64_t result = errorvalue;
  if ((status = sqlite3_prepare_v2(sqliteconn, sql, -1, &sqlresult, NULL)) == SQLITE_OK) {
    sqlite3_bind_int64(sqlresult, 1, param1);
    while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    if (status == SQLITE_ROW)
      result = sqlite3_column_int64(sqlresult, 0);
    sqlite3_finalize(sqlresult);
  }
  return result;
}
*/

int64_t get_single_value_sql_with_2_parameters (sqlite3* sqliteconn, const char* sql, int64_t param1, int64_t param2, int64_t errorvalue)
{
  int status;
	sqlite3_stmt* sqlresult;
	int64_t result = errorvalue;
  if ((status = sqlite3_prepare_v2(sqliteconn, sql, -1, &sqlresult, NULL)) == SQLITE_OK) {
    sqlite3_bind_int64(sqlresult, 1, param1);
    sqlite3_bind_int64(sqlresult, 2, param2);
    while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    if (status == SQLITE_ROW)
      result = sqlite3_column_int64(sqlresult, 0);
    sqlite3_finalize(sqlresult);
  }
  return result;
}

sqlite3_stmt* execute_sql_query_with_1_string_parameter (sqlite3* sqliteconn, const char* sql, int* status, const char* param1)
{
	sqlite3_stmt* sqlresult = NULL;
	if ((*status = sqlite3_prepare_v2(sqliteconn, sql, -1, &sqlresult, NULL)) == SQLITE_OK) {
    sqlite3_bind_text(sqlresult, 1, param1, -1, NULL);
    while ((*status = sqlite3_step(sqlresult)) == SQLITE_BUSY) {
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    }
    if (*status == SQLITE_ROW || *status == SQLITE_DONE)
      return sqlresult;
	}
	return NULL;
}

