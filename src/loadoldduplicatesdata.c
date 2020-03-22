#ifdef WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "sqlitefunctions.h"

//#define USE_DB_TRANSACTIONS 1

void show_progress_movement (int index)
{
  const int progresscharstotal = 4;
  const char progresschars[] = "/-\\|";
  printf("\b%c", progresschars[index % progresscharstotal]);
}

void list_sql (sqlite3* sqliteconn, const char* sql)
{
  int status;
	sqlite3_stmt* sqlresult;
  if ((sqlresult = execute_sql_query(sqliteconn, sql, &status)) != NULL) {
    if (status == SQLITE_ROW || status == SQLITE_DONE) {
      int i;
      int n = sqlite3_column_count(sqlresult);
      int* coltypes = (int*)malloc(sizeof(int) * n);
      for (i = 0; i < n; i++) {
        coltypes[i] = sqlite3_column_type(sqlresult, i);
      }
      while (status == SQLITE_ROW) {
        for (i = 0; i < n; i++) {
          switch (coltypes[i]) {
            case SQLITE_INTEGER :
              printf("%li\t", (long)sqlite3_column_int64(sqlresult, i));
              break;
            case SQLITE_NULL :
              printf("NULL\t");
              break;
            default :
              printf("%s\t", (char*)sqlite3_column_text(sqlresult, i));
              break;
          }
        }
        printf("\n");
        while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
          WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
      }
      free(coltypes);
    }
    sqlite3_finalize(sqlresult);
	}
}

////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
  const char* dbnameold = "tempdb.20131129.sq3";
  const char* dbnamenew = "tempdb.20131224.sq3";
  int status;
  clock_t starttime;
  sqlite3* sqliteconn;
	sqlite3_stmt* sqlresult;
	sqlite3_stmt* sqlstmt_update_file;
  //initialize
#ifdef USING_RHASH
  rhash_library_init();
#endif
  //create memory database and attach both databases
	if (sqlite3_open(":memory:", &sqliteconn) != SQLITE_OK) {
		return 1;
  }
  if ((sqlresult = execute_sql_query_with_1_string_parameter(sqliteconn, "ATTACH DATABASE ? AS old", &status, dbnameold)) != NULL)
    sqlite3_finalize(sqlresult);
  if ((sqlresult = execute_sql_query_with_1_string_parameter(sqliteconn, "ATTACH DATABASE ? AS new", &status, dbnamenew)) != NULL)
    sqlite3_finalize(sqlresult);

  //prepare update statement
  if ((status = sqlite3_prepare_v2(sqliteconn, "UPDATE new.file SET crc32=?, sha1=? WHERE id=?", -1, &sqlstmt_update_file, NULL)) != SQLITE_OK) {
    return 2;
  }

  //get hash keys from existing files with hash keys that still have the same size and modification time
  starttime = clock();
#ifdef USE_DB_TRANSACTIONS
  execute_sql_cmd(sqliteconn, "BEGIN");
#endif
  printf("reusing hash keys for unchanged files...  ");
  if ((sqlresult = execute_sql_query(sqliteconn,
    "SELECT\n"
    "  fi1.id,\n"
    "  fi1.crc32,\n"
    "  fi1.sha1\n"
    "FROM old.folder AS fo1\n"
    "JOIN old.file AS fi1 ON fo1.id = fi1.folder"
    "  AND (fi1.crc32 IS NOT NULL OR fi1.sha1 IS NOT NULL)\n"
    "JOIN new.folder AS fo2 ON fo1.path = fo2.path\n"
    "JOIN new.file AS fi2 ON fo2.id = fi2.folder AND fi1.name = fi2.name"
    "  AND (fi2.crc32 IS NULL AND fi2.sha1 IS NULL)"
    "  AND (fi1.size = fi2.size)"
    "  AND (MAX(fi1.created, fi1.lastaccess) = MAX(fi2.created, fi2.lastaccess))", &status)) != NULL) {
    if (status == SQLITE_ROW || status == SQLITE_DONE) {
      int index = 0;
      while (status == SQLITE_ROW) {
        if (sqlite3_column_type(sqlstmt_update_file, 1) != SQLITE_NULL)
          sqlite3_bind_int64(sqlstmt_update_file, 1, sqlite3_column_int64(sqlstmt_update_file, 2));
        else
          sqlite3_bind_null(sqlstmt_update_file, 1);
        if (sqlite3_column_type(sqlstmt_update_file, 2) != SQLITE_NULL)
          sqlite3_bind_text(sqlstmt_update_file, 2, sqlite3_column_text(sqlstmt_update_file, 3), -1, NULL);
        else
          sqlite3_bind_null(sqlstmt_update_file, 2);
        sqlite3_bind_int64(sqlstmt_update_file, 3, sqlite3_column_int64(sqlstmt_update_file, 1));
        while ((status = sqlite3_step(sqlstmt_update_file)) == SQLITE_BUSY)
          WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
if (status!= SQLITE_DONE) printf("Status: %i\n",status);
  /////if (status != SQLITE_DONE) printf("SQL error %lu (file: %s)\n", (unsigned long)status, fullpath);////
        sqlite3_reset(sqlstmt_update_file);


        if (index++ % 1000 == 0) {
          show_progress_movement(index / 1000);
        }
        while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
          WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
      }
    }
    sqlite3_finalize(sqlresult);
	}
#ifdef USE_DB_TRANSACTIONS
  execute_sql_cmd(sqliteconn, "COMMIT");
#endif
  printf("\relapsed time cross referencing data: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

  //clean up database connection
  sqlite3_finalize(sqlstmt_update_file);
  sqlite3_close(sqliteconn);

  return 0;
}

/*
  printf("*** removed folders ***\n");
  list_sql(sqliteconn,
    "SELECT\n"
    "  old.folder.path\n"
    "FROM old.folder\n"
    "LEFT JOIN new.folder ON old.folder.path = new.folder.path\n"
    "WHERE (new.folder.path IS NULL)\n"
    //"AND (old.folder.path LIKE 'D:\\Home\\v%')\n"
    "ORDER BY old.folder.path, new.folder.path\n"
    "LIMIT 20"
  );
  printf("*** added folders ***\n");
  list_sql(sqliteconn,
    "SELECT\n"
    "  new.folder.path\n"
    "FROM new.folder\n"
    "LEFT JOIN old.folder ON new.folder.path = old.folder.path\n"
    "WHERE (old.folder.path IS NULL)\n"
    //"AND (new.folder.path LIKE 'D:\\Home\\v%')\n"
    "ORDER BY new.folder.path, old.folder.path\n"
    "LIMIT 20"
  );
*/
/*
  printf("*** removed files ***\n");
  list_sql(sqliteconn,
    "SELECT\n"
    "  fo.path,\n"
    "  fi.name AS filename,\n"
    "  fi.extension,\n"
    "  fi.size,\n"
    "  fi.owner,\n"
    "  DATETIME(fi.created, 'unixepoch', 'localtime') AS createddate\n"
    "FROM folder AS fo\n"
    "LEFT JOIN old.file AS fi ON fo.id = fi.folder\n"
    "LEFT JOIN folder AS fo2 ON fo.path = fo2.path\n"
    "LEFT JOIN new.file AS fi2 ON fo2.id = fi2.folder AND fi.name = fi2.name\n"
    "WHERE fi.id IS NOT NULL\n"
    "AND (fo2.id IS NULL OR fi2.id IS NULL)\n"
    "ORDER BY fo.path, fi.name\n"
    "LIMIT 10");
*/
/*
  printf("*** creating indexes ***\n");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS old.idx_folder_path ON folder (path)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS old.idx_file_name ON file (name)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS new.idx_folder_path ON folder (path)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS new.idx_file_name ON file (name)");
*/
/*
  printf("*** drop indexes ***\n");
  execute_sql_cmd(sqliteconn,
    "DROP INDEX IF EXISTS old.idx_folder_path");
  execute_sql_cmd(sqliteconn,
    "DROP INDEX IF EXISTS old.idx_file_name");
  execute_sql_cmd(sqliteconn,
    "DROP INDEX IF EXISTS new.idx_folder_path");
  execute_sql_cmd(sqliteconn,
    "DROP INDEX IF EXISTS new.idx_file_name");
*/
/*
  list_sql(sqliteconn,
    "SELECT\n"
    "  fo1.path,\n"
    "  fi1.name AS filename,\n"
    "  fi1.extension,\n"
    "  fi1.size,\n"
    "  fi1.owner,\n"
    "  DATETIME(fi1.created, 'unixepoch', 'localtime') AS createddate,\n"
    "  DATETIME(fi1.lastaccess, 'unixepoch', 'localtime') AS lastaccessdate,\n"
    "  fi1.id,\n"
    "  fi1.crc32,\n"
    "  fi1.sha1\n"
    "FROM old.folder AS fo1\n"
    "JOIN old.file AS fi1 ON fo1.id = fi1.folder"
    "  AND (fi1.crc32 IS NOT NULL OR fi1.sha1 IS NOT NULL)\n"
//"AND fi1.size >= 1024 AND fi1.size < 2048\n"
    "JOIN new.folder AS fo2 ON fo1.path = fo2.path\n"
    "JOIN new.file AS fi2 ON fo2.id = fi2.folder AND fi1.name = fi2.name"
    "  AND (fi2.crc32 IS NULL AND fi2.sha1 IS NULL)"
    "  AND (fi1.size = fi2.size)"
    "  AND (MAX(fi1.created, fi1.lastaccess) = MAX(fi2.created, fi2.lastaccess))\n"
    "LIMIT 10");
*/
