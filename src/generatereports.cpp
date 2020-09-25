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
#include "folderreportsversion.h"
#include "dataoutput.h"

#define APPLICATION_NAME "generatereports"

#define RETRY_WAIT_TIME 100
#ifdef __WIN32__
#define WAIT_BEFORE_RETRY(ms) Sleep(ms);
#else
#define WAIT_BEFORE_RETRY(ms) usleep(ms * 1000);
#endif

////////////////////////////////////////////////////////////////////////

//#define REPORTCLASS DataOutputHTML
//#define REPORTCLASS DataOutputXML
#define REPORTCLASS DataOutputXLSX

void generate_sql_report (sqlite3* sqliteconn, const char* filename, const char* sql)
{
  int status;
	sqlite3_stmt* sqlresult;
	char* fullfilename = (char*)malloc(strlen(filename) + strlen(REPORTCLASS::GetDefaultExtension()) + 1);
	strcpy(fullfilename, filename);
	strcat(fullfilename, REPORTCLASS::GetDefaultExtension());
  DataOutputBase* dst = new REPORTCLASS(fullfilename);
  if ((sqlresult = execute_sql_query(sqliteconn, sql, &status)) != NULL) {
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

////////////////////////////////////////////////////////////////////////

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
        "Generates reports based on data in database %s\n", APPLICATION_NAME, "tempdb.sq3");
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

  //start timer
  printf("generating reports...");
  starttime = clock();
  //write reports
  generate_sql_report(sqliteconn, "100_largest_files",
    "SELECT\n"
    "  folder.path,\n"
    "  file.name AS filename,\n"
    "  file.extension,\n"
    "  file.size,\n"
    "  file.owner,\n"
    "  DATETIME(file.created, 'unixepoch', 'localtime') AS createddate,\n"
    "  DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate\n"
    "FROM file\n"
    "LEFT JOIN folder ON file.folder = folder.id\n"
    "ORDER BY file.size DESC, folder.path, file.name\n"
    "LIMIT 100");
  generate_sql_report(sqliteconn, "100_largest_folders",
    "SELECT\n"
    "  folder.path,\n"
    "  folder.filecount,\n"
    "  folder.totalsize,\n"
    "  folder.owner,\n"
    "  MIN(file.size) AS smallestsize,\n"
    "  MAX(file.size) AS largestsize,\n"
    "  DATETIME(MIN(file.created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(file.created), 'unixepoch', 'localtime') AS lastdcreateddate,\n"
    "  DATETIME(MIN(file.lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(file.lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM folder\n"
    "LEFT JOIN file ON folder.id = file.folder\n"
    "GROUP BY folder.id\n"
    "ORDER BY folder.totalsize DESC, folder.filecount DESC\n"
    "LIMIT 100\n");
  generate_sql_report(sqliteconn, "distribution_by_extension",
    "SELECT\n"
    "  extension,\n"
    "  COUNT(*) AS files,\n"
    "  SUM(size) AS totalsize,\n"
    "  SUM(size) AS totalsize,\n"
    "  CAST(AVG(size) AS INT) AS averagesize,\n"
    "  MIN(size) AS smallestsize,\n"
    "  MAX(size) AS largestsize,\n"
    "  DATETIME(MIN(created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(created), 'unixepoch', 'localtime') AS lastdcreateddate,\n"
    "  DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM file\n"
    "GROUP BY extension\n"
    "ORDER BY totalsize DESC, files DESC, extension");
  generate_sql_report(sqliteconn, "distribution_by_creation_age",
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
    "GROUP BY age\n"
    "ORDER BY age");
  generate_sql_report(sqliteconn, "distribution_by_access_age",
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
    "GROUP BY age\n"
    "ORDER BY age");
  generate_sql_report(sqliteconn, "distribution_by_size",
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
    "GROUP BY sizegroup\n"
    "ORDER BY sizegroup");
  generate_sql_report(sqliteconn, "files_by_owner",
    "SELECT\n"
    "  owner,\n"
    "  COUNT(*) AS files,\n"
    "  SUM(size) AS totalsize,\n"
    "  COUNT(DISTINCT folder) AS differentfolders,\n"
    "  DATETIME(MIN(created), 'unixepoch', 'localtime') AS firstcreateddate,\n"
    "  DATETIME(MAX(created), 'unixepoch', 'localtime') AS lastcreateddate,\n"
    "  DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstaccessdate,\n"
    "  DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastaccessdate\n"
    "FROM file\n"
    "GROUP BY owner\n"
    "ORDER BY totalsize DESC");
  generate_sql_report(sqliteconn, "folders_by_owner",
    "SELECT\n"
    "  owner,\n"
    "  COUNT(*) AS folders,\n"
    "  SUM (totalsize) AS totalfilesize,\n"
    "  SUM (totalsizerecursive) AS totalfilesizerecursive\n"
    "FROM folder\n"
    "GROUP BY owner\n"
    "ORDER BY totalfilesize DESC, folders DESC");
  //show elapsed time
  printf("\relapsed time generating reports: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

  //generate reports about duplicates
  int reportduplicates = 0;
  int status;
  sqlite3_stmt* sqlresult;
  sqlresult = execute_sql_query (sqliteconn, "SELECT * FROM duplicatefiles WHERE 0=1", &status);
  if (sqlresult) {
    if (status == SQLITE_DONE)
      reportduplicates = 1;
    sqlite3_finalize(sqlresult);
  }
  if (!reportduplicates) {
    printf("not generating reports about duplicate files (run findduplicates first)\n");
  } else {
    printf("generating reports about duplicate files...");
    starttime = clock();
    generate_sql_report(sqliteconn, "duplicate_files",
      "SELECT DISTINCT\n"
      "  duplicatefiles.file1 AS id,\n"
      "  folder.path,\n"
      "  file.name AS filename,\n"
      "  file.extension,\n"
      "  file.size,\n"
      "  file.owner,\n"
      "  DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate\n"
      "FROM file\n"
      "JOIN duplicatefiles ON file.id = duplicatefiles.file1 OR file.id = duplicatefiles.file2\n"
      "JOIN folder ON file.folder = folder.id\n"
      "ORDER BY file.size DESC, duplicatefiles.file1, folder.path, file.name");
/* Invalid counts (duplicate files within same folders result in wrong totals)
    generate_sql_report(sqliteconn, "folders_containing_duplicates",
      "SELECT\n"
      "  folder.path,\n"
      "  COUNT(file.folder) AS duplicates,\n"
      "  SUM(file.size) AS totalduplicatesize,\n"
      "  CAST(100 * COUNT(file.folder) / folder.filecount AS INT) AS percentduplicates,\n"
      "  CAST(100 * SUM(file.size) / folder.totalsize AS INT) AS percentsize\n"
      "FROM duplicatefiles\n"
      "JOIN file ON duplicatefiles.file1 = file.id OR duplicatefiles.file2 = file.id\n"
      "JOIN folder ON file.folder = folder.id\n"
      "GROUP BY file.folder\n"
      "HAVING duplicates > 0\n"
      "ORDER BY totalduplicatesize DESC, duplicates DESC, folder.path");
*/
    generate_sql_report(sqliteconn, "folders_with_duplicates",
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
      "GROUP BY file1.folder, file2.folder\n"
      "HAVING file1.id <> file2.id AND folder1.id < folder2.id\n"
      "ORDER BY totalduplicatesize DESC, duplicates DESC, folder1.path, folder2.path");
    printf("\relapsed time generating reports about duplicate files: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);
  }

  //clean up database connection
  sqlite3_close(sqliteconn);

  return 0;
}

/* goal: determine how much space old files occupy */

/*
-- show root folder information
sqlite3 -header tempdb.sq3 "SELECT * FROM folder WHERE id = 1"
-- show contents of root folder
sqlite3 -header tempdb.sq3 "SELECT * FROM file WHERE folder = 1"
-- list top 20 folders with most entries
sqlite3 -header tempdb.sq3 "SELECT path, (foldercount + filecount) AS entries, foldercount, filecount FROM folder ORDER BY entries DESC LIMIT 20"
-- list top 20 folders with most data
sqlite3 -header tempdb.sq3 "SELECT path, totalsize, filecount FROM folder ORDER BY totalsize DESC LIMIT 20"
-- list all folders
sqlite3 -header tempdb.sq3 "SELECT path, id, parent FROM folder ORDER BY path"
-- list all files
sqlite3 -header tempdb.sq3 "SELECT folder.path, file.name, file.extension, file.size, DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id ORDER BY folder.path, file.name"
-- list files with negative size (should be empty)
sqlite3 -header tempdb.sq3 "SELECT folder.path, file.name, file.extension, file.size, DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id WHERE size < 0 ORDER BY folder.path, file.name"
-- list files with timestamp in the future (should be empty)
sqlite3 -header tempdb.sq3 "SELECT folder.path, file.name, file.extension, file.size, DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id WHERE lastfiledate > DATETIME('now', 'localtime') ORDER BY folder.path, file.name"
-- list by extension
sqlite3 -header tempdb.sq3 "SELECT extension, COUNT(*) AS files, SUM(size) AS totalsize, DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstdate, DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastdate FROM file GROUP BY extension ORDER BY totalsize DESC, extension"
-- list by chunks of time
sqlite3 -header tempdb.sq3 "SELECT COUNT(*) AS files, DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstdate FROM file GROUP BY lastaccess / (60 * 60 * 24 * 30) ORDER BY firstdate DESC"
-- list historical distribution
sqlite3 -header tempdb.sq3 "SELECT CASE
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') > DATETIME('now') THEN '0. Future'
  WHEN DATE(lastaccess, 'unixepoch', 'localtime') >= DATE('now') THEN 'A. today'
  WHEN DATE(lastaccess, 'unixepoch', 'localtime') >= DATE('now', '-1 day') THEN 'B. yesterday'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month') THEN 'C. this month'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-1 month') THEN 'D. previous month'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-2 months') THEN 'E. 2 months ago'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-6 months') THEN 'F. between 3 and 6 months ago'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-1 year') THEN 'G. between 6 and 12 months ago'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-2 year') THEN 'H. between 1 and 2 years ago'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-3 year') THEN 'I. between 2 and 3 years ago'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-4 year') THEN 'J. between 3 and 4 years ago'
  WHEN DATETIME(lastaccess, 'unixepoch', 'localtime') >= DATETIME('now', 'start of month', '-5 year') THEN 'K. between 4 and 5 years ago'
  ELSE 'Z. Older'
 END AS age, COUNT(*) AS files, SUM(size) AS totalsize, DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstdate, DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastdate FROM file GROUP BY age"
-- list size distribution
sqlite3 -header tempdb.sq3 "SELECT CASE
  WHEN size <= 0 THEN 'A. = 0 KB'
  WHEN size < 1024 THEN 'B. < 1 KB'
  WHEN size < 1024 * 10 THEN 'C. < 10 KB'
  WHEN size < 1024 * 100 THEN 'D. < 100 KB'
  WHEN size < 1024 * 1024 THEN 'E. < 1 MB'
  WHEN size < 1024 * 1024 * 10 THEN 'F. < 10 MB'
  WHEN size < 1024 * 1024 * 100 THEN 'G. < 100 MB'
  WHEN size < 1024 * 1024 * 1024 THEN 'H. < 1 GB'
  WHEN size < 1024 * 1024 * 1024 * 10 THEN 'I. < 10 GB'
  WHEN size < 1024 * 1024 * 1024 * 100 THEN 'J. < 100 GB'
  WHEN size < 1024 * 1024 * 1024 * 1024 THEN 'K. < 1 TB'
  WHEN size < 1024 * 1024 * 1024 * 1024 * 10 THEN 'L. < 10 TB'
  WHEN size < 1024 * 1024 * 1024 * 1024 * 100 THEN 'M. < 100 TB'
  ELSE 'Z. > 100 TB'
 END AS sizegroup, COUNT(*) AS files, SUM(size) AS totalsize, DATETIME(MIN(lastaccess), 'unixepoch', 'localtime') AS firstdate, DATETIME(MAX(lastaccess), 'unixepoch', 'localtime') AS lastdate FROM file GROUP BY sizegroup"
-- list folders with duplicates
SELECT
  folder.path,
  COUNT(file.folder) AS duplicates,
  SUM(file.size) AS totalduplicatesize
FROM duplicatefiles
JOIN file ON duplicatefiles.file1 = file.id OR duplicatefiles.file2 = file.id
JOIN folder ON file.folder = folder.id
GROUP BY file.folder
HAVING duplicates > 0
ORDER BY duplicates DESC, folder.path;
-- list folders side by side with duplicates
SELECT
  folder1.path AS path1,
  folder2.path AS path2,
  COUNT(file1.folder) AS duplicates,
  SUM(file1.size) AS totalduplicatesize
FROM duplicatefiles
JOIN file AS file1 ON duplicatefiles.file1 = file1.id OR duplicatefiles.file2 = file1.id
JOIN file AS file2 ON duplicatefiles.file1 = file2.id OR duplicatefiles.file2 = file2.id
JOIN folder AS folder1 ON file1.folder = folder1.id
JOIN folder AS folder2 ON file2.folder = folder2.id
GROUP BY file1.folder, file2.folder
HAVING file1.id <> file2.id AND folder1.id < folder2.id
ORDER BY duplicates DESC, folder1.path, folder2.path;

-- list top 100 biggest files
sqlite3 -header tempdb.sq3 "SELECT folder.path, file.name, file.extension, file.size, DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id ORDER BY file.size DESC, folder.path, file.name LIMIT 100"
-- list top 100 most common file names
sqlite3 -header tempdb.sq3 "SELECT name, COUNT(*) AS duplicates, SUM(size) AS totalsize, MIN(size) AS smallestsize, MAX(size) AS largestsize FROM file GROUP BY name HAVING duplicates > 1 ORDER BY duplicates DESC LIMIT 100"
-- duplicate filenames+sizes
sqlite3 -header tempdb.sq3 "SELECT name, size, COUNT(*) AS duplicates FROM file GROUP BY name, size HAVING duplicates > 1 ORDER BY size DESC, duplicates DESC"
sqlite3 -header tempdb.sq3 "SELECT file1.name, file1.size, COUNT(*) AS duplicates FROM file AS file1 JOIN file AS file2 ON file1.id <> file2.id AND file1.name = file2.name AND file1.size = file2.size WHERE file1.size > 0 GROUP BY file1.name, file1.size ORDER BY file1.size DESC, file1.name"
-- duplicate filenames+sizes
sqlite3 -header tempdb.sq3 "SELECT name, COUNT(*) AS duplicates FROM file GROUP BY name HAVING duplicates > 1 ORDER BY duplicates DESC, name"
-- duplicate sizes
sqlite3 -header tempdb.sq3 "SELECT size, COUNT(*) AS duplicates FROM file WHERE size > 0 GROUP BY size HAVING duplicates > 1 ORDER BY size DESC"
sqlite3 -header tempdb.sq3 "SELECT name, size FROM file WHERE size IN (SELECT size FROM file WHERE size > 0 GROUP BY size HAVING COUNT(*) > 1) ORDER BY size DESC, name"
sqlite3 -header tempdb.sq3 "SELECT folder.path, file.name, file.extension, file.size, DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id WHERE file.size IN (SELECT size FROM file WHERE size > 0 GROUP BY size HAVING COUNT(*) > 1) ORDER BY file.size DESC, folder.path, file.name"

sqlite3 -header tempdb.sq3 "SELECT duplicatefiles.file1, folder.path, file.name, file.extension, file.size, DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file JOIN duplicatefiles ON file.id = duplicatefiles.file1 OR file.id = duplicatefiles.file2 LEFT JOIN folder ON file.folder = folder.id ORDER BY duplicatefiles.file1, folder.path, file.name"

sqlite3 -header tempdb.sq3 "SELECT owner, COUNT(file.id) AS totalfiles, SUM(file.size) AS totalsize FROM file GROUP BY owner"

-- list specific user's files
sqlite3 -header tempdb.sq3 "SELECT folder.path,file.name AS filename,file.size,DATETIME(file.lastaccess, 'unixepoch', 'localtime') AS lastfiledate FROM file LEFT JOIN folder ON file.folder = folder.id WHERE file.owner='ISIS\lahayel' ORDER BY file.size DESC, folder.path, file.name"

-- list recent files
sqlite3 -header tempdb.sq3 "SELECT * FROM file WHERE DATE(lastaccess, 'unixepoch', 'localtime') >= DATE('now', '-1 day') ORDER BY owner, size DESC"
-- list owners of files created today and total size of new files
sqlite3 -header tempdb.sq3 "SELECT owner, SUM(size) FROM file WHERE DATE(MAX(created, lastaccess), 'unixepoch', 'localtime') >= DATE('now', '-1 day') GROUP BY owner ORDER BY SUM(size) DESC"
*/
//See also: http://en.wikipedia.org/wiki/NTFS_symbolic_link


//SQLITE_FULL
