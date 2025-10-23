#ifdef WIN32
//#define __MSVCRT_VERSION__ 0x0601
#include <windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "sqlitefunctions.h"
#include "dataoutput.h"
#include "folderreportsversion.h"
#include <dirtrav.h>

#define APPLICATION_NAME "generateuserreports"

#define RETRY_WAIT_TIME 100
#ifdef __WIN32__
#define WAIT_BEFORE_RETRY(ms) Sleep(ms);
#else
#define WAIT_BEFORE_RETRY(ms) usleep(ms * 1000);
#endif

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

////////////////////////////////////////////////////////////////////////

//#define REPORTCLASS DataOutputHTML
//#define REPORTCLASS DataOutputXML
#define REPORTCLASS DataOutputXLSX

void generate_sql_report_with_1_string_parameter (sqlite3* sqliteconn, const char* filename, const char* sql, const char* param1)
{
  int status;
	sqlite3_stmt* sqlresult;
	char* fullfilename = (char*)malloc(strlen(filename) + strlen(REPORTCLASS::GetDefaultExtension()) + 1);
	strcpy(fullfilename, filename);
	strcat(fullfilename, REPORTCLASS::GetDefaultExtension());
  DataOutputBase* dst = new REPORTCLASS(fullfilename);
  if ((sqlresult = execute_sql_query_with_1_string_parameter(sqliteconn, sql, &status, param1)) != NULL) {
    if (status == SQLITE_ROW || status == SQLITE_DONE) {
      const char* s;
      int i;
      int n = sqlite3_column_count(sqlresult);
      int* coltypes = (int*)malloc(sizeof(int) * n);
      for (i = 0; i < n; i++) {
        dst->AddColumn(sqlite3_column_name(sqlresult, i));
        coltypes[i] = sqlite3_column_type(sqlresult, i);
      }
      while (status == SQLITE_ROW) {
        dst->AddRow();
        for (i = 0; i < n; i++) {
          switch (coltypes[i]) {
            case SQLITE_INTEGER :
              //dst->AddData(sqlite3_column_int64(sqlresult, i));
              dst->AddData((int64_t)sqlite3_column_int64(sqlresult, i));
              break;
            //case SQLITE_NULL :
            //  dst->AddData((const char*)NULL);
            //  break;
            default :
              s = (char*)sqlite3_column_text(sqlresult, i);
              dst->AddData(s);
              break;
          }
        }
        while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
          WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
      }
      free(coltypes);
    }
    sqlite3_finalize(sqlresult);
	}
  delete dst;
}

std::string get_user_report_name (const char* user, const char* report)
{
  std::string::size_type i = 0;
  std::string result = user;
  i = 0;
  while ((i = result.find('\\', i)) != std::string::npos) {
    result.replace(i, 1, "-");
    i++;
  }
  //return std::string("user_") + result + "_" + report;
  return std::string(user) + PATH_SEPARATOR + "user_" + result + "_" + report;
}

////////////////////////////////////////////////////////////////////////

void process_owner (sqlite3* sqliteconn, const char* owner, int reportduplicates)
{
  //abort if owner is NULL
  if (!owner)
    owner = "$$NONE$$";
  //create parent folder(s)
  dirtrav_make_full_path(NULL, owner, 0);
  //start timer
  printf("generating reports for user %s\n", owner);
  //write reports
  generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "100_largest_files").c_str(),
    "SELECT\n"
    "  folder.path,\n"
    "  file.name AS filename,\n"
    "  file.extension,\n"
    "  file.size,\n"
    "  file.owner,\n"
    "  DATETIME(file.created, 'unixepoch', 'localtime') AS createddate,\n"
    "  DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM file\n"
    "LEFT JOIN folder ON file.folder = folder.id\n"
    "WHERE file.owner = ?\n"
    "ORDER BY file.size DESC, folder.path, file.name\n"
    "LIMIT 100", owner);
  generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "100_largest_folders").c_str(),
    "SELECT\n"
    "  folder.path,\n"
    "  folder.filecount,\n"
    "  folder.totalsize,\n"
    "  folder.owner,\n"
    "  MIN(size) AS smallestsize,\n"
    "  MAX(size) AS largestsize,\n"
    "  DATETIME(MIN(file.created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(file.created), 'unixepoch', 'localtime') AS lastcreateddate,\n"
    "  DATETIME(MIN(file.lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(file.lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM folder\n"
    "LEFT JOIN file ON folder.id = file.folder\n"
    "WHERE folder.owner = ?\n"
    "GROUP BY folder.id\n"
    "ORDER BY folder.totalsize DESC, folder.filecount DESC\n"
    "LIMIT 100\n", owner);
  generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "distribution_by_extension").c_str(),
    "SELECT\n"
    "  extension,\n"
    "  COUNT(*) AS files,\n"
    "  SUM(size) AS totalsize,\n"
    "  CAST(AVG(size) AS INT) AS averagesize,\n"
    "  MIN(size) AS smallestsize,\n"
    "  MAX(size) AS largestsize,\n"
    "  DATETIME(MIN(created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(created), 'unixepoch', 'localtime') AS lastcreateddate,\n"
    "  DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM file\n"
    "WHERE file.owner = ?\n"
    "GROUP BY extension\n"
    "ORDER BY totalsize DESC, files DESC, extension", owner);
  generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "distribution_by_creation_age").c_str(),
    "SELECT\n"
    "  CASE\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') > DATETIME('now') THEN '0. Future'\n"
    "    WHEN DATE(created, 'unixepoch', 'localtime') >= DATE('now') THEN 'A. today'\n"
    "    WHEN DATE(created, 'unixepoch', 'localtime') >= DATE('now', '-1 day') THEN 'B. yesterday'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month') THEN 'C. this month'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-1 month') THEN 'D. previous month'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-2 months') THEN 'E. 2 months ago'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-6 months') THEN 'F. between 3 and 6 months ago'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-1 year') THEN 'G. between 6 and 12 months ago'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-2 year') THEN 'H. between 1 and 2 years ago'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-3 year') THEN 'I. between 2 and 3 years ago'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-4 year') THEN 'J. between 3 and 4 years ago'\n"
    "    WHEN DATETIME(created, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-5 year') THEN 'K. between 4 and 5 years ago'\n"
    "    ELSE 'Z. Older'\n"
    "  END AS age,\n"
    "  COUNT(*) AS files,\n"
    "  SUM(size) AS totalsize,\n"
    "  MIN(size) AS smallestsize,\n"
    "  CAST(AVG(size) AS INT) AS averagesize,\n"
    "  MAX(size) AS largestsize,\n"
    "  DATETIME(MIN(created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(created), 'unixepoch', 'localtime') AS lastcreateddate,\n"
    "  DATETIME(MIN(created), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(created), 'unixepoch', 'localtime') AS lastcreateddate\n"
    "FROM file\n"
    "WHERE file.owner = ?\n"
    "GROUP BY age\n"
    "ORDER BY age", owner);
  generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "distribution_by_access_age").c_str(),
    "SELECT\n"
    "  CASE\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') > DATETIME('now') THEN '0. Future'\n"
    "    WHEN DATE(lastaccess, 'unixepoch', 'localtime') >= DATE('now') THEN 'A. today'\n"
    "    WHEN DATE(lastaccess, 'unixepoch', 'localtime') >= DATE('now', '-1 day') THEN 'B. yesterday'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month') THEN 'C. this month'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-1 month') THEN 'D. previous month'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-2 months') THEN 'E. 2 months ago'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-6 months') THEN 'F. between 3 and 6 months ago'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-1 year') THEN 'G. between 6 and 12 months ago'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-2 year') THEN 'H. between 1 and 2 years ago'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-3 year') THEN 'I. between 2 and 3 years ago'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-4 year') THEN 'J. between 3 and 4 years ago'\n"
    "    WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-5 year') THEN 'K. between 4 and 5 years ago'\n"
    "    ELSE 'Z. Older'\n"
    "  END AS age,\n"
    "  COUNT(*) AS files,\n"
    "  SUM(size) AS totalsize,\n"
    "  MIN(size) AS smallestsize,\n"
    "  CAST(AVG(size) AS INT) AS averagesize,\n"
    "  MAX(size) AS largestsize,\n"
    "  DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastcreateddate,\n"
    "  DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM file\n"
    "WHERE file.owner = ?\n"
    "GROUP BY age\n"
    "ORDER BY age", owner);
  generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "distribution_by_size").c_str(),
    "SELECT\n"
    "  CASE\n"
    "    WHEN size <= 0 THEN 'A. = 0 KB'\n"
    "    WHEN size < 1024 THEN 'B. < 1 KB'\n"
    "    WHEN size < 1024 * 10 THEN 'C. < 10 KB'\n"
    "    WHEN size < 1024 * 100 THEN 'D. < 100 KB'\n"
    "    WHEN size < 1024 * 1024 THEN 'E. < 1 MB'\n"
    "    WHEN size < 1024 * 1024 * 10 THEN 'F. < 10 MB'\n"
    "    WHEN size < 1024 * 1024 * 100 THEN 'G. < 100 MB'\n"
    "    WHEN size < 1024 * 1024 * 1024 THEN 'H. < 1 GB'\n"
    "    WHEN size < 1024 * 1024 * 1024 * 10 THEN 'I. < 10 GB'\n"
    "    WHEN size < 1024 * 1024 * 1024 * 100 THEN 'J. < 100 GB'\n"
    "    WHEN size < 1024 * 1024 * 1024 * 1024 THEN 'K. < 1 TB'\n"
    "    WHEN size < 1024 * 1024 * 1024 * 1024 * 10 THEN 'L. < 10 TB'\n"
    "    WHEN size < 1024 * 1024 * 1024 * 1024 * 100 THEN 'M. < 100 TB'\n"
    "    WHEN size < 1024 * 1024 * 1024 * 1024 * 1024 THEN 'N. < 1 PB'\n"
    "    ELSE 'Z. >= 1 PT'\n"
    "  END AS sizegroup,\n"
    "  COUNT(*) AS files,\n"
    "  SUM(size) AS totalsize,\n"
    "  CAST(AVG(size) AS INT) AS averagesize,\n"
    "  MIN(size) AS smallestsize,\n"
    "  MAX(size) AS largestsize,\n"
    "  DATETIME(MIN(created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(created), 'unixepoch', 'localtime') AS lastcreateddate,\n"
    "  DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM file\n"
    "WHERE file.owner = ?\n"
    "GROUP BY sizegroup\n"
    "ORDER BY sizegroup", owner);

  //generate reports about duplicates
  if (reportduplicates) {
    generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "duplicate_files").c_str(),
      "SELECT DISTINCT\n"
      "  duplicatefiles.file1 AS id,\n"
      "  folder.path,\n"
      "  file.name AS filename,\n"
      "  file.extension,\n"
      "  file.size,\n"
      "  file.owner,\n"
      "  DATETIME(file.created, 'unixepoch', 'localtime') AS createddate,\n"
      "  DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastaccessdate\n"
      "FROM file\n"
      "JOIN duplicatefiles ON file.id = duplicatefiles.file1 OR file.id = duplicatefiles.file2\n"
      "JOIN folder ON file.folder = folder.id\n"
      "WHERE file.owner = ?\n"
      "ORDER BY file.size DESC, duplicatefiles.file1, folder.path, file.name", owner);
    generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "duplicate_files2").c_str(),
      "SELECT DISTINCT\n"
      "  folder1.path AS path1,\n"
      "  file1.name AS filename1,\n"
      "  file1.extension AS extension1,\n"
      "  file1.size AS size1,\n"
      "  file1.owner AS owner1,\n"
      "  DATETIME(file1.created, 'unixepoch', 'localtime') AS createddate1,\n"
      "  DATETIME(file1.lastaccess, 'unixepoch', 'localtime') AS lastaccessdate1,\n"
      "  folder2.path AS path2,\n"
      "  file2.name AS filename2,\n"
      "  file2.extension AS extension2,\n"
      "  file2.size AS size2,\n"
      "  file2.owner AS owner2,\n"
      "  DATETIME(file2.created, 'unixepoch', 'localtime') AS createddate2,\n"
      "  DATETIME(file2.lastaccess, 'unixepoch', 'localtime') AS lastaccessdate2\n"
      "FROM duplicatefiles\n"
      "JOIN file AS file1 ON duplicatefiles.file1 = file1.id\n"
      "JOIN file AS file2 ON duplicatefiles.file2 = file2.id\n"
      "JOIN folder AS folder1 ON file1.folder = folder1.id\n"
      "JOIN folder AS folder2 ON file2.folder = folder2.id\n"
      "WHERE file1.owner = ?1 OR file2.owner = ?1\n"
      "ORDER BY file1.size DESC, folder1.path, file1.name", owner);
    generate_sql_report_with_1_string_parameter(sqliteconn, get_user_report_name(owner, "folders_with_duplicates").c_str(),
      "SELECT\n"
      "  COUNT(file1.folder) AS duplicates,\n"
      "  SUM(file1.size) AS totalduplicatesize,\n"
      "  folder1.path AS path1,\n"
      "	 folder1.filecount AS path1files,\n"
      "	 folder1.totalsize AS path1totalsize,\n"
      "  folder1.owner AS folder1owner,\n"
      "  folder2.path AS path2,\n"
      "	 folder2.filecount AS path2files,\n"
      "	 folder2.totalsize AS path2totalsize,\n"
      "  folder2.owner AS folder2owner\n"
      "FROM duplicatefiles\n"
      "JOIN file AS file1 ON duplicatefiles.file1 = file1.id OR duplicatefiles.file2 = file1.id\n"
      "JOIN file AS file2 ON duplicatefiles.file1 = file2.id OR duplicatefiles.file2 = file2.id\n"
      "JOIN folder AS folder1 ON file1.folder = folder1.id\n"
      "JOIN folder AS folder2 ON file2.folder = folder2.id\n"
      "WHERE file1.owner = ?1 OR file2.owner = ?1\n"
      "GROUP BY file1.folder, file2.folder\n"
      "HAVING file1.id <> file2.id AND folder1.id < folder2.id\n"
      "ORDER BY totalduplicatesize DESC, duplicates DESC, folder1.path, folder2.path", owner);
  }
}

int main (int argc, char *argv[])
{
  const char* dbname = "tempdb.sq3";
  clock_t starttime;
  sqlite3* sqliteconn;

  //check command line parameters
  if (argc > 1) {
    if (strcmp(argv[1], "-v") == 0) {
      printf("%s version %s\n", APPLICATION_NAME, FOLDERREPORTS_VERSION_STRING);
      return 0;
    } else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-?") == 0) {
      printf(
        "Usage: %s\n"
        "Generates user reports based on data in database %s\n", APPLICATION_NAME, "tempdb.sq3");
      return 0;
    }
  }

  //initialize
	if (sqlite3_open(dbname, &sqliteconn) != SQLITE_OK) {
		return 1;
  }

  //change database settings for optimal performance
  sqlite3_exec(sqliteconn, "PRAGMA encoding = \"UTF-8\"", NULL, NULL, NULL);
  sqlite3_exec(sqliteconn, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
  sqlite3_exec(sqliteconn, "PRAGMA journal_mode=OFF", NULL, NULL, NULL);
  sqlite3_exec(sqliteconn, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  //create indices
  printf("creating indices...");
  starttime = clock();
  execute_sql_cmd(sqliteconn, "BEGIN");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_folder_parent ON folder (parent)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_folder_owner ON folder (owner)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_file_folder ON file (folder)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_file_size ON file (size DESC)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_file_owner ON file (owner)");
  execute_sql_cmd(sqliteconn, "COMMIT");
  printf("\rnew indices created: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

  //check if duplicate reports should be generated
  int reportduplicates = 0;
  int status;
  sqlite3_stmt* sqlresult;
  sqlresult = execute_sql_query (sqliteconn, "SELECT * FROM duplicatefiles WHERE 0=1", &status);
  if (sqlresult) {
    if (status == SQLITE_DONE)
      reportduplicates = 1;
    sqlite3_finalize(sqlresult);
  }

  //generate reports
  starttime = clock();
  if (argc > 1) {
    //process command line parameters
    int i;
    for (i = 1; i < argc; i++)
      process_owner(sqliteconn, argv[i], reportduplicates);
  } else {
    if ((sqlresult = execute_sql_query(sqliteconn, "SELECT DISTINCT owner FROM (SELECT DISTINCT owner FROM file UNION SELECT DISTINCT owner FROM folder)", &status)) != NULL) {
      if (status == SQLITE_ROW || status == SQLITE_DONE) {
        while (status == SQLITE_ROW) {
          process_owner(sqliteconn, (char*)sqlite3_column_text(sqlresult, 0), reportduplicates);
          while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
            WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
        }
      }
      sqlite3_finalize(sqlresult);
    }
  }
  //show elapsed time
  printf("\relapsed time generating reports: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

  //clean up database connection
  sqlite3_close(sqliteconn);

  return 0;
}

/* goal: determine how much space old files occupy */

/*
-- list specific user's files
sqlite3.exe -header tempdb.sq3 "SELECT folder.path,file.name AS filename,file.size,DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id WHERE file.owner='ISIS\lahayel' ORDER BY file.size DESC, folder.path, file.name"
-- list all users
sqlite3.exe -header tempdb.sq3 "SELECT DISTINCT OWNER FROM (SELECT DISTINCT owner FROM file UNION SELECT DISTINCT owner FROM folder)"
*/
//See also: http://en.wikipedia.org/wiki/NTFS_symbolic_link


//SQLITE_FULL

