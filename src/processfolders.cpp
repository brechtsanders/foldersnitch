#ifdef _WIN32
#define __MSVCRT_VERSION__ 0x0601
#define WINVER 0x0500
#include <windows.h>
#include <sddl.h>
#define USE_WIDE_STRINGS
#else
#include <pwd.h>
//#include <grp.h>
#endif
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
//#include <unistd.h>
#include <stdint.h>
#include "sqlitefunctions.h"
#include <mhash.h>
#include "traverse_directory.h"
#include "dataoutput.h"

#define MAX_EXTENSION_LENGTH 12

#define LOOKUP_SID

//#define USE_TEMP_DB 1
#define USE_DB_TRANSACTIONS 1

#define COMMIT_FORCE_CHECK_EVERY 10
#define COMMIT_FORCE_AMOUNT 1000
#define COMMIT_FORCE_INTERVAL 30

#if !defined(USE_TEMP_DB) && !defined(USE_DB_TRANSACTIONS)
#define USE_DB_TRANSACTIONS 1
#endif

#define DEF_INT2STR_HELPER(n) #n
#define DEF_INT2STR(n) DEF_INT2STR_HELPER(n)

#ifdef __MINGW32__
#define PRINTF_INT64_MODIFIER "I64"
#else
#define PRINTF_INT64_MODIFIER "ll"
#endif

//#define STAT(path,data) (stat(path, data) == 0)
//#define STAT_STRUCT struct stat
#ifdef __MINGW32__
#ifdef USE_WIDE_STRINGS
#define STATOK(path,data) (_wstat64(path, data) == 0)
#else
#define STATOK(path,data) (_stat64(path, data) == 0)
#endif
#define STAT_STRUCT struct __stat64
#else
#define STATOK(path,data) (stat64(path, data) == 0)
#define STAT_STRUCT struct stat64
#endif

////////////////////////////////////////////////////////////////////////

void show_progress_movement (int index)
{
  const int progresscharstotal = 4;
  const char progresschars[] = "/-\\|";
  printf("\b%c", progresschars[index % progresscharstotal]);
}

void show_progress_flush ()
{
  printf("\b*");
}

////////////////////////////////////////////////////////////////////////

/*
void show_time (time_t timestamp)
{
  //printf("%lu", (unsigned long)timestamp);
	char buf[20];
	//strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&timestamp));
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&timestamp));
	printf("%s", buf);
}
*/

#ifdef USE_WIDE_STRINGS
const wchar_t* wide_get_extension (const wchar_t* filename)
{
  const wchar_t* p;
  size_t len;
  if (!filename || !*filename)
    return NULL;
  p = filename + wcslen(filename);
  len = 0;
  while (p-- != filename && len++ < MAX_EXTENSION_LENGTH) {
    if (*p == L'.')
      return p;
  }
  return NULL;
}
#else
const char* get_extension (const char* filename)
{
  const char* p;
  size_t len;
  if (!filename || !*filename)
    return NULL;
  p = filename + strlen(filename);
  len = 0;
  while (p-- != filename && len++ < MAX_EXTENSION_LENGTH) {
    if (*p == '.')
      return p;
  }
  return NULL;
}
#endif

#ifdef USE_WIDE_STRINGS
wchar_t* wide_get_file_owner (const wchar_t* path)
{
  wchar_t* result = NULL;
  SECURITY_DESCRIPTOR* secdes;
  PSID ownersid;
  BOOL ownderdefaulted;
  DWORD len = 0;
  GetFileSecurityW(path, OWNER_SECURITY_INFORMATION, NULL, 0, &len);
  if (len > 0) {
    secdes = (SECURITY_DESCRIPTOR*)malloc(len);
    if (GetFileSecurityW(path, OWNER_SECURITY_INFORMATION, secdes, len, &len)) {
      if (GetSecurityDescriptorOwner(secdes, &ownersid, &ownderdefaulted)) {
        SID_NAME_USE accounttype;
        wchar_t* name;
        wchar_t* domain;
        DWORD domainlen;
        len = 0;
        domainlen = 0;
        LookupAccountSidW(NULL, ownersid, NULL, &len, NULL, &domainlen, &accounttype);
        if (len > 0 && domainlen > 0) {
          name = (wchar_t*)malloc(len * sizeof(wchar_t));
          domain = (wchar_t*)malloc(domainlen * sizeof(wchar_t));
          if (LookupAccountSidW(NULL, ownersid, name, &len, domain, &domainlen, &accounttype)) {
            result = (wchar_t*)malloc((domainlen + len + 2) * sizeof(wchar_t));
            memcpy(result, domain, domainlen * sizeof(wchar_t));
            result[domainlen] = '\\';
            wcscpy(result + domainlen + 1, name);
          }
#ifdef LOOKUP_SID
          else {
            wchar_t* sidstring;
            if (ConvertSidToStringSidW(ownersid, &sidstring)) {
              result = wcsdup(sidstring);
              LocalFree(sidstring);
            }
          }
#endif
          free(name);
          free(domain);
        }
      }
    }
    free(secdes);
  }
  //GROUP_SECURITY_INFORMATION
  return result;
}
#else
char* get_file_owner (const char* path)
{
  char* result = NULL;
#ifdef _WIN32
  SECURITY_DESCRIPTOR* secdes;
  PSID ownersid;
  BOOL ownderdefaulted;
  DWORD len = 0;
  GetFileSecurityA(path, OWNER_SECURITY_INFORMATION, NULL, 0, &len);
  if (len > 0) {
    secdes = (SECURITY_DESCRIPTOR*)malloc(len);
    if (GetFileSecurityA(path, OWNER_SECURITY_INFORMATION, secdes, len, &len)) {
      if (GetSecurityDescriptorOwner(secdes, &ownersid, &ownderdefaulted)) {
        SID_NAME_USE accounttype;
        char* name;
        char* domain;
        DWORD domainlen;
        len = 0;
        domainlen = 0;
        LookupAccountSidA(NULL, ownersid, NULL, &len, NULL, &domainlen, &accounttype);
        if (len > 0 && domainlen > 0) {
          name = (TCHAR*)malloc(len);
          domain = (TCHAR*)malloc(domainlen);
          if (LookupAccountSidA(NULL, ownersid, name, &len, domain, &domainlen, &accounttype)) {
            result = (char*)malloc(domainlen + len + 2);
            memcpy(result, domain, domainlen);
            result[domainlen] = '\\';
            strcpy(result + domainlen + 1, name);
          }
#ifdef LOOKUP_SID
          else {
            LPSTR sidstring;
            if (ConvertSidToStringSid(ownersid, &sidstring)) {
              result = strdup(sidstring);
              LocalFree(sidstring);
            }
          }
#endif
          free(name);
          free(domain);
        }
      }
    }
    free(secdes);
  }
  //GROUP_SECURITY_INFORMATION
#else
  struct stat fileinfo;
  if (stat(path, &fileinfo) == 0) {
    struct passwd* pw = getpwuid(fileinfo.st_uid);
    if (pw) {
      result = strdup(pw->pw_name);
    }
    //struct group* gr = getgrgid(fileinfo.st_gid);
  }
#endif
  return result;
}
#endif

#ifdef USE_WIDE_STRINGS
inline int is_link (const wchar_t* path)
{
  DWORD fileattr;
  if ((fileattr = GetFileAttributesW(path)) != 0xFFFFFFFF && fileattr & FILE_ATTRIBUTE_REPARSE_POINT)
    return 1;
  return 0;
}
#else
inline int is_link (const char* path)
{
#ifdef _WIN32
  DWORD fileattr;
  if ((fileattr = GetFileAttributesA(path)) != 0xFFFFFFFF && fileattr & FILE_ATTRIBUTE_REPARSE_POINT)
    return 1;
  return 0;
#else
  struct stat fileinfo;
  if (lstat(path, &fileinfo) == 0 && S_ISLNK(fileinfo.st_mode))
    return 1;
#endif
  return 0;
}
#endif

////////////////////////////////////////////////////////////////////////

typedef struct folder_data {
  uint64_t id;
  struct folder_data* parent;
#ifdef USE_WIDE_STRINGS
  wchar_t* path;
#else
  char* path;
#endif
  unsigned int folderlevel;
  uint64_t foldercount;
  uint64_t filecount;
  uint64_t totalsize;
  uint64_t foldercountrecursive;
  uint64_t filecountrecursive;
  uint64_t totalsizerecursive;
  uint64_t previousfolderid;
} folder_data;

typedef struct file_folder_callback_data {
  sqlite3* sqliteconn;
  uint64_t filecount;
  uint64_t totalsize;
  uint64_t foldercount;
	sqlite3_stmt* sqlstmt_insert_file;
	sqlite3_stmt* sqlstmt_insert_folder;
	sqlite3_stmt* sqlstmt_get_previous_folder_id;
	sqlite3_stmt* sqlstmt_get_previous_file_hashes;
#ifdef USE_DB_TRANSACTIONS
  size_t newsincecommit;
  clock_t lastcommit;
#endif
} file_folder_callback_data;

int file_folder_callback_data_initialize (file_folder_callback_data* file_callback_data, const char* previousdbname)
{
  int status;
  file_callback_data->filecount = 0;
  file_callback_data->totalsize = 0;
  file_callback_data->foldercount = 0;
#ifdef USE_DB_TRANSACTIONS
  file_callback_data->newsincecommit = 0;
  file_callback_data->lastcommit = clock();
#endif
  file_callback_data->sqlstmt_insert_file = NULL;
  if ((status = sqlite3_prepare_v2(file_callback_data->sqliteconn, "INSERT INTO file (id, folder, name, extension, size, created, lastaccess, owner, crc32, sha1, adler32) VALUES (?, ?, ?, LOWER(?), ?, ?, ?, ?, ?, ?, ?)", -1, &(file_callback_data->sqlstmt_insert_file), NULL)) != SQLITE_OK) {
    return 1;
  }
  file_callback_data->sqlstmt_insert_folder = NULL;
  if ((status = sqlite3_prepare_v2(file_callback_data->sqliteconn, "INSERT INTO folder (id, parent, path, owner, filecount, foldercount, totalsize, filecountrecursive, foldercountrecursive, totalsizerecursive) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, &(file_callback_data->sqlstmt_insert_folder), NULL)) != SQLITE_OK) {
    return 2;
  }
  file_callback_data->sqlstmt_get_previous_folder_id = NULL;
  file_callback_data->sqlstmt_get_previous_file_hashes = NULL;
  if (previousdbname) {
    sqlite3_stmt* sqlresult;
    if ((sqlresult = execute_sql_query_with_1_string_parameter(file_callback_data->sqliteconn, "ATTACH DATABASE ? AS old", &status, previousdbname)) != NULL) {
      sqlite3_finalize(sqlresult);
      if ((status = sqlite3_prepare_v2(file_callback_data->sqliteconn, "SELECT id FROM old.folder WHERE path = ?", -1, &(file_callback_data->sqlstmt_get_previous_folder_id), NULL)) == SQLITE_OK) {
        if ((status = sqlite3_prepare_v2(file_callback_data->sqliteconn, "SELECT crc32, sha1, adler32 FROM old.file WHERE folder = ? AND name = ? AND (size = ? AND created = ? AND lastaccess = ?) AND (crc32 IS NOT NULL OR sha1 IS NOT NULL OR adler32 IS NOT NULL)", -1, &(file_callback_data->sqlstmt_get_previous_file_hashes), NULL)) != SQLITE_OK) {
          sqlite3_finalize(file_callback_data->sqlstmt_get_previous_folder_id);
          file_callback_data->sqlstmt_get_previous_folder_id = NULL;
        }
      }
    }
  }
  return 0;
}

void file_folder_callback_data_cleanup (file_folder_callback_data* file_callback_data)
{
  if (file_callback_data->sqlstmt_insert_file) {
    sqlite3_finalize(file_callback_data->sqlstmt_insert_file);
    file_callback_data->sqlstmt_insert_file = NULL;
  }
  if (file_callback_data->sqlstmt_insert_folder) {
    sqlite3_finalize(file_callback_data->sqlstmt_insert_folder);
    file_callback_data->sqlstmt_insert_folder = NULL;
  }
  if (file_callback_data->sqlstmt_get_previous_folder_id) {
    sqlite3_finalize(file_callback_data->sqlstmt_get_previous_folder_id);
    file_callback_data->sqlstmt_get_previous_folder_id = NULL;
  }
  if (file_callback_data->sqlstmt_get_previous_file_hashes) {
    sqlite3_finalize(file_callback_data->sqlstmt_get_previous_file_hashes);
    file_callback_data->sqlstmt_get_previous_file_hashes = NULL;
  }
}

#ifndef USE_DB_TRANSACTIONS
#define file_folder_callback_data_progress(file_callback_data)
#else
#define file_folder_callback_data_progress(file_callback_data)
/*
void file_folder_callback_data_progress (file_folder_callback_data* file_callback_data)
{
  //commit if long enough ago
  if (file_callback_data->newsincecommit++ > 0 && file_callback_data->newsincecommit % COMMIT_FORCE_CHECK_EVERY == 0) {
    if (file_callback_data->newsincecommit >= COMMIT_FORCE_AMOUNT || clock() > file_callback_data->lastcommit + COMMIT_FORCE_INTERVAL * CLOCKS_PER_SEC) {
      show_progress_flush();
      execute_sql_cmd(file_callback_data->sqliteconn, "COMMIT");
      execute_sql_cmd(file_callback_data->sqliteconn, "BEGIN");
      file_callback_data->lastcommit = clock();
      file_callback_data->newsincecommit = 0;
    }
  }
}
*/
#endif

#ifdef USE_WIDE_STRINGS
int folder_callback_before (const wchar_t* fullpath, void* callbackdata, void** folderdata, void* parentfolderdata)
#else
int folder_callback_before (const char* fullpath, void* callbackdata, void** folderdata, void* parentfolderdata)
#endif
{
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)callbackdata;
/*/
  //show folder
  unsigned int i;
  if (parentfolderdata) {
    for (i = ((folder_data*)parentfolderdata)->folderlevel + 1; i > 0; i--)
      printf(" ");
  }
  printf("%s\n", fullpath);
*/
  //show progress
  show_progress_movement(file_callback_data->foldercount);
  //update data
  file_callback_data->foldercount++;
  if (parentfolderdata) {
    ((folder_data*)parentfolderdata)->foldercount++;
  }
  //create new folder data
  *folderdata = malloc(sizeof(folder_data));
  ((folder_data*)*folderdata)->id = file_callback_data->foldercount;
  ((folder_data*)*folderdata)->parent = (folder_data*)parentfolderdata;
#ifdef USE_WIDE_STRINGS
  ((folder_data*)*folderdata)->path = wcsdup(fullpath);
#else
  ((folder_data*)*folderdata)->path = strdup(fullpath);
#endif
  ((folder_data*)*folderdata)->folderlevel = (parentfolderdata ? ((folder_data*)parentfolderdata)->folderlevel + 1 : 0);
  ((folder_data*)*folderdata)->foldercount = 0;
  ((folder_data*)*folderdata)->filecount = 0;
  ((folder_data*)*folderdata)->totalsize = 0;
  ((folder_data*)*folderdata)->foldercountrecursive = 0;
  ((folder_data*)*folderdata)->filecountrecursive = 0;
  ((folder_data*)*folderdata)->totalsizerecursive = 0;
  ((folder_data*)*folderdata)->previousfolderid = 0;
  //stop procedding if the entry is a link
  if (is_link(fullpath))
    return -1;
  //get id of folder in data of previous run
  if (file_callback_data->sqlstmt_get_previous_folder_id) {
    int status;
    sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_get_previous_folder_id;
#ifdef USE_WIDE_STRINGS
    sqlite3_bind_text16(sqlresult, 1, fullpath, -1, NULL);
#else
    sqlite3_bind_text(sqlresult, 1, fullpath, -1, NULL);
#endif
    while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY) {
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    }
    if (status == SQLITE_ROW) {
      ((folder_data*)*folderdata)->previousfolderid = sqlite3_column_int64(sqlresult, 0);
    }
    sqlite3_reset(sqlresult);
  }
  return 0;
}

#ifdef USE_WIDE_STRINGS
int folder_callback_after (const wchar_t* fullpath, void* callbackdata, void** folderdata, void* parentfolderdata)
#else
int folder_callback_after (const char* fullpath, void* callbackdata, void** folderdata, void* parentfolderdata)
#endif
{
  int status;
#ifdef USE_WIDE_STRINGS
  wchar_t* owner;
#else
  char* owner;
#endif
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)callbackdata;
  //update data
  ((folder_data*)*folderdata)->foldercountrecursive += ((folder_data*)*folderdata)->foldercount;
  ((folder_data*)*folderdata)->filecountrecursive += ((folder_data*)*folderdata)->filecount;
  ((folder_data*)*folderdata)->totalsizerecursive += ((folder_data*)*folderdata)->totalsize;
  if (parentfolderdata) {
    ((folder_data*)parentfolderdata)->foldercountrecursive += ((folder_data*)*folderdata)->foldercountrecursive;
    ((folder_data*)parentfolderdata)->filecountrecursive += ((folder_data*)*folderdata)->filecountrecursive;
    ((folder_data*)parentfolderdata)->totalsizerecursive += ((folder_data*)*folderdata)->totalsizerecursive;
  }
  //update database
  if (file_callback_data->sqlstmt_insert_folder) {
    sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_insert_folder;
    sqlite3_bind_int64(sqlresult, 1, ((folder_data*)*folderdata)->id);
    if (parentfolderdata && ((folder_data*)parentfolderdata)->id)
      sqlite3_bind_int64(sqlresult, 2, ((folder_data*)parentfolderdata)->id);
    else
      sqlite3_bind_null(sqlresult, 2);
#ifdef USE_WIDE_STRINGS
    sqlite3_bind_text16(sqlresult, 3, fullpath, -1, NULL);
    if ((owner = wide_get_file_owner(((folder_data*)*folderdata)->path)) != NULL)
      sqlite3_bind_text16(sqlresult, 4, owner, -1, NULL);
#else
    sqlite3_bind_text(sqlresult, 3, fullpath, -1, NULL);
    if ((owner = get_file_owner(((folder_data*)*folderdata)->path)) != NULL)
      sqlite3_bind_text(sqlresult, 4, owner, -1, NULL);
#endif
    else
      sqlite3_bind_null(sqlresult, 4);
    sqlite3_bind_int64(sqlresult, 5, ((folder_data*)*folderdata)->filecount);
    sqlite3_bind_int64(sqlresult, 6, ((folder_data*)*folderdata)->foldercount);
    sqlite3_bind_int64(sqlresult, 7, ((folder_data*)*folderdata)->totalsize);
    sqlite3_bind_int64(sqlresult, 8, ((folder_data*)*folderdata)->filecountrecursive);
    sqlite3_bind_int64(sqlresult, 9, ((folder_data*)*folderdata)->foldercountrecursive);
    sqlite3_bind_int64(sqlresult, 10, ((folder_data*)*folderdata)->totalsizerecursive);
    while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
/////if (status != SQLITE_DONE) printf("%lu (folder: %s)\n", (unsigned long)status, fullpath);/////
    sqlite3_reset(sqlresult);
    free(owner);
    //notify of progress (flush if needed)
    file_folder_callback_data_progress(file_callback_data);
  }
  //clean up folder data
  free(((folder_data*)*folderdata)->path);
  free(*folderdata);
  return 0;
}

#ifdef USE_WIDE_STRINGS
int file_callback (const wchar_t* directory, const wchar_t* filename, const wchar_t* fullpath, void* callbackdata, void* parentfolderdata)
#else
int file_callback (const char* directory, const char* filename, const char* fullpath, void* callbackdata, void* parentfolderdata)
#endif
{
  int status;
#ifdef USE_WIDE_STRINGS
  wchar_t* owner;
#else
  char* owner;
#endif
  STAT_STRUCT statbuf;
  time_t createdtimestamp = 0;
  time_t latesttimestamp = 0;
  int have_previous_crc32 = 0;
  int have_previous_sha1 = 0;
  int have_previous_adler32 = 0;
  uint32_t previous_crc32;
  char previous_sha1[41];
  uint32_t previous_adler32;
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)callbackdata;
  if (STATOK(fullpath, &statbuf)) {
    createdtimestamp = statbuf.st_ctime;
    //determine latest time stamp (between created/modified/accessed)
    latesttimestamp = statbuf.st_atime;
    ////note: st_mtime is sometimes set to a date in the future (especially by digital cameras)
    //if (statbuf.st_mtime > latesttimestamp)
    //  latesttimestamp = statbuf.st_mtime;
    if (statbuf.st_ctime > latesttimestamp)
      latesttimestamp = statbuf.st_ctime;
    //update data
    file_callback_data->filecount++;
    file_callback_data->totalsize += statbuf.st_size;
    if (parentfolderdata) {
      ((folder_data*)parentfolderdata)->filecount++;
      ((folder_data*)parentfolderdata)->totalsize += statbuf.st_size;
    }
    //get hash keys from same file in data of previous run
    if (((folder_data*)parentfolderdata)->previousfolderid != 0) {
      int status;
      sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_get_previous_file_hashes;
      sqlite3_bind_int64(sqlresult, 1, ((folder_data*)parentfolderdata)->previousfolderid);
#ifdef USE_WIDE_STRINGS
      sqlite3_bind_text16(sqlresult, 2, filename, -1, NULL);
#else
      sqlite3_bind_text(sqlresult, 2, filename, -1, NULL);
#endif
      sqlite3_bind_int64(sqlresult, 3, statbuf.st_size);
      sqlite3_bind_int64(sqlresult, 4, createdtimestamp);
      sqlite3_bind_int64(sqlresult, 5, latesttimestamp);
      while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY) {
        WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
      }
      if (status == SQLITE_ROW) {
        if (sqlite3_column_type(sqlresult, 0) != SQLITE_NULL) {
          have_previous_crc32 = 1;
          previous_crc32 = sqlite3_column_int64(sqlresult, 0);
        }
        if (sqlite3_column_type(sqlresult, 1) != SQLITE_NULL) {
          have_previous_sha1 = 1;
          strncpy(previous_sha1, (char*)sqlite3_column_text(sqlresult, 1), 41);
        }
        if (sqlite3_column_type(sqlresult, 2) != SQLITE_NULL) {
          have_previous_adler32 = 1;
          previous_adler32 = sqlite3_column_int64(sqlresult, 2);
        }
      }
      sqlite3_reset(sqlresult);
    }
    //update database
    if (file_callback_data->sqlstmt_insert_file) {
      sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_insert_file;
      sqlite3_bind_int64(sqlresult, 1, file_callback_data->filecount);
      if (parentfolderdata && ((folder_data*)parentfolderdata)->id)
        sqlite3_bind_int64(sqlresult, 2, ((folder_data*)parentfolderdata)->id);
      else
        sqlite3_bind_null(sqlresult, 2);
#ifdef USE_WIDE_STRINGS
      sqlite3_bind_text16(sqlresult, 3, filename, -1, NULL);
      sqlite3_bind_text16(sqlresult, 4, wide_get_extension(filename), -1, NULL);
#else
      sqlite3_bind_text(sqlresult, 3, filename, -1, NULL);
      sqlite3_bind_text(sqlresult, 4, get_extension(filename), -1, NULL);
#endif
      sqlite3_bind_int64(sqlresult, 5, statbuf.st_size);
      if (createdtimestamp)
        sqlite3_bind_int64(sqlresult, 6, createdtimestamp);
      else
        sqlite3_bind_null(sqlresult, 6);
      if (latesttimestamp)
        sqlite3_bind_int64(sqlresult, 7, latesttimestamp);
      else
        sqlite3_bind_null(sqlresult, 7);
#ifdef USE_WIDE_STRINGS
      if ((owner = wide_get_file_owner(fullpath)) != NULL)
        sqlite3_bind_text16(sqlresult, 8, owner, -1, NULL);
#else
      if ((owner = get_file_owner(fullpath)) != NULL)
        sqlite3_bind_text(sqlresult, 8, owner, -1, NULL);
#endif
      else
        sqlite3_bind_null(sqlresult, 8);
      if (have_previous_crc32)
        sqlite3_bind_int64(sqlresult, 9, previous_crc32);
      else
        sqlite3_bind_null(sqlresult, 9);
      if (have_previous_sha1)
        sqlite3_bind_text(sqlresult, 10, previous_sha1, -1, NULL);
      else
        sqlite3_bind_null(sqlresult, 10);
      if (have_previous_adler32)
        sqlite3_bind_int64(sqlresult, 11, previous_adler32);
      else
        sqlite3_bind_null(sqlresult, 11);
      while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
        WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
/////if (status != SQLITE_DONE) printf("SQL error %lu (file: %s)\n", (unsigned long)status, fullpath);////
      sqlite3_reset(sqlresult);
      free(owner);
      //notify of progress (flush if needed)
      file_folder_callback_data_progress(file_callback_data);
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
  const char* dbname = "tempdb.sq3";
  const char* previousdbname = "tempdbold.sq3";
  clock_t starttime;
  int i;
  file_folder_callback_data file_callback_data;
  //open database
	if (sqlite3_open(
#ifdef USE_TEMP_DB
      "",  //temporary database (will use TEMP folder if needed)
      //":memory:",  //in memory database
#else
      dbname,
#endif
      &file_callback_data.sqliteconn) != SQLITE_OK) {
		return 1;
  }

  //prepare database
#ifdef USE_DB_TRANSACTIONS
  execute_sql_cmd(file_callback_data.sqliteconn, "BEGIN");
#endif
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS folder");
  //execute_sql_cmd(file_callback_data.sqliteconn, "DROP INDEX IF EXISTS idx_file_size");
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS file");
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS duplicatefiles");
  execute_sql_cmd(file_callback_data.sqliteconn,
    "CREATE TABLE folder (\n"
    "  id INT NOT NULL PRIMARY KEY,\n"
    "  parent INT,\n"
    "  path VARCHAR(1024) NOT NULL,\n"
    "  owner VARCHAR(255),\n"
    "  filecount INT DEFAULT NULL,\n"
    "  foldercount INT DEFAULT NULL,\n"
    "  totalsize INT DEFAULT NULL,\n"
    "  filecountrecursive INT DEFAULT NULL,\n"
    "  foldercountrecursive INT DEFAULT NULL,\n"
    "  totalsizerecursive INT DEFAULT NULL\n"
    ")");
  execute_sql_cmd(file_callback_data.sqliteconn,
    "CREATE TABLE file (\n"
    "  id INT NOT NULL PRIMARY KEY,\n"
    "  folder INT,\n"
    "  name VARCHAR(255) NOT NULL,\n"
    "  extension VARCHAR(" DEF_INT2STR(MAX_EXTENSION_LENGTH) "),\n"
    "  size INT NOT NULL,\n"
    "  created INT,\n"
    "  lastaccess INT,\n"
    "  owner VARCHAR(255),\n"
    "  crc32 INT DEFAULT NULL,\n"
    "  sha1 VARCHAR(40) DEFAULT NULL,\n"
    "  adler32 INT DEFAULT NULL\n"
    ")");
#ifdef USE_DB_TRANSACTIONS
  execute_sql_cmd(file_callback_data.sqliteconn, "COMMIT");
#endif
  //start timer
  printf("processing folders...  ");
  starttime = clock();
  //process folders
  if (file_folder_callback_data_initialize(&file_callback_data, previousdbname) == 0) {
    //treat each command line argument as a folder to process
#ifdef USE_WIDE_STRINGS
    LPWSTR *wide_argv;
    int wide_argc;
    wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    for (i = 1; i < wide_argc; i++) {
#else
    for (i = 1; i < argc; i++) {
#endif
#ifdef USE_DB_TRANSACTIONS
      execute_sql_cmd(file_callback_data.sqliteconn, "BEGIN");
#endif
#ifdef USE_WIDE_STRINGS
//wide_traverse_directory(L"C:\\PortableApps", &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);/////
//wide_traverse_directory(L"C:\\PortableApps", &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);/////
//wide_traverse_directory(L".", &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);/////
//wide_traverse_directory(L".", NULL, &file_callback_data, NULL, NULL, &file_callback_data, NULL);/////
      wide_traverse_directory(wide_argv[i], &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);
#else
      traverse_directory(argv[i], &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);
#endif
#ifdef USE_DB_TRANSACTIONS
      execute_sql_cmd(file_callback_data.sqliteconn, "COMMIT");
#endif
    }
#ifdef USE_WIDE_STRINGS
    LocalFree(wide_argv);
#endif
    file_folder_callback_data_cleanup(&file_callback_data);
    //show elapsed time
    printf("\relapsed time processing folders: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

#ifdef USE_TEMP_DB
    //copy data to disk
    sqlite3* sqliteconn;
    sqlite3_backup* dbcopy;
    starttime = clock();
    printf("writing data to disk...  ");
    if (sqlite3_open(dbname, &sqliteconn) == SQLITE_OK) {
      if ((dbcopy = sqlite3_backup_init(sqliteconn, "main", file_callback_data.sqliteconn, "main")) != NULL) {
        sqlite3_backup_step(dbcopy, -1);
        sqlite3_backup_finish(dbcopy);
        //show elapsed time
        printf("\relapsed time writing data: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);
      }
    }
#endif
  }

  //clean up database connection
  sqlite3_close(file_callback_data.sqliteconn);

  //show information
  printf("%" PRINTF_INT64_MODIFIER "u files found in %" PRINTF_INT64_MODIFIER "u folders, total size: %" PRINTF_INT64_MODIFIER "u M\n", (uint64_t)file_callback_data.filecount, (uint64_t)file_callback_data.foldercount, (uint64_t)file_callback_data.totalsize / 1024 / 1024);

  return 0;
}

//sqlite3 -csv -header tempdb.sq3 "SELECT folder.path, file.name AS filename, file.size, file.owner, file.crc32, file.sha1, file.adler32 FROM file LEFT JOIN folder ON file.folder = folder.id"
