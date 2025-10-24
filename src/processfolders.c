#ifdef _WIN32
#define __MSVCRT_VERSION__ 0x0601
#define WINVER 0x0500
#include <windows.h>
#include <sddl.h>
#ifndef USE_WIDE_STRINGS
#define USE_WIDE_STRINGS
#endif
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
#include <inttypes.h>
#include "sqlitefunctions.h"
#include "folderreportsversion.h"
#include <mhash.h>
#define BUILD_DIRTRAV_STATIC
#ifdef USE_WIDE_STRINGS
#include <dirtravw.h>
#define DIRTRAVCHAR wchar_t
#define DIRTRAVSTRLEN wcslen
#define DIRTRAVSTRCPY wcscpy
#define DIRTRAVSTRDUP wcsdup
#define DIRTRAVSTRCMP wcscmp
#define DIRTRAVPRINTF wprintf
#define DIRTRAVTEXT_(s) L##s
#define DIRTRAVTEXT(s) DIRTRAVTEXT_(s)
#define DIRTRAVFN(fn) dirtravw_##fn
#define SQLITE3FN(fn) sqlite3_##fn##16
#else
#include <dirtrav.h>
#define DIRTRAVCHAR char
#define DIRTRAVSTRLEN strlen
#define DIRTRAVSTRCPY strcpy
#define DIRTRAVSTRDUP strdup
#define DIRTRAVSTRCMP strcmp
#define DIRTRAVPRINTF printf
#define DIRTRAVTEXT(s) s
#define DIRTRAVFN(fn) dirtrav_##fn
#define SQLITE3FN(fn) sqlite3_##fn
#endif

#define APPLICATION_NAME "processfolders"

#define LOOKUP_SID

#define USE_TEMP_DB 1
#define USE_DB_TRANSACTIONS 1

#define COMMIT_FORCE_CHECK_EVERY 10
#define COMMIT_FORCE_AMOUNT 5000
#define COMMIT_FORCE_INTERVAL 30

#if !defined(USE_TEMP_DB) && !defined(USE_DB_TRANSACTIONS)
#define USE_DB_TRANSACTIONS 1
#endif

#ifdef __MINGW32__
#define PRINTF_INT64_MODIFIER "I64"
#else
#define PRINTF_INT64_MODIFIER "ll"
#endif

/*
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
*/

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

typedef struct folder_data {
  uint64_t id;
  struct folder_data* parent;
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
  if ((status = sqlite3_prepare_v2(file_callback_data->sqliteconn, "INSERT INTO file (id, folder, name, extension, size, created, lastaccess, ownerid, crc32, sha1, adler32) VALUES (?, ?, ?, LOWER(?), ?, ?, ?, ?, ?, ?, ?)", -1, &(file_callback_data->sqlstmt_insert_file), NULL)) != SQLITE_OK) {
    return 1;
  }
  file_callback_data->sqlstmt_insert_folder = NULL;
  if ((status = sqlite3_prepare_v2(file_callback_data->sqliteconn, "INSERT INTO folder (id, parent, path, ownerid, filecount, foldercount, totalsize, filecountrecursive, foldercountrecursive, totalsizerecursive) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, &(file_callback_data->sqlstmt_insert_folder), NULL)) != SQLITE_OK) {
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

int folder_callback_before (DIRTRAVFN(entry) info)
{
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)info->callbackdata;
	const DIRTRAVCHAR* fullpath = DIRTRAVFN(prop_get_path)(info);
  folder_data** folderdata = (folder_data**)&(info->folderlocaldata);
  const folder_data* parentfolderdata = (info->parentinfo ? info->parentinfo->folderlocaldata : NULL);
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
  *folderdata = (folder_data*)malloc(sizeof(folder_data));
  (*folderdata)->id = file_callback_data->foldercount;
  (*folderdata)->parent = (folder_data*)parentfolderdata;
  (*folderdata)->folderlevel = (parentfolderdata ? ((folder_data*)parentfolderdata)->folderlevel + 1 : 0);
  (*folderdata)->foldercount = 0;
  (*folderdata)->filecount = 0;
  (*folderdata)->totalsize = 0;
  (*folderdata)->foldercountrecursive = 0;
  (*folderdata)->filecountrecursive = 0;
  (*folderdata)->totalsizerecursive = 0;
  (*folderdata)->previousfolderid = 0;
  //stop processing if the entry is a link
  if (DIRTRAVFN(prop_is_link)(info))
    return -1;
  //get id of folder in data of previous run
  if (file_callback_data->sqlstmt_get_previous_folder_id) {
    int status;
    sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_get_previous_folder_id;
    SQLITE3FN(bind_text)(sqlresult, 1, fullpath, -1, NULL);
    while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY) {
      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
    }
    if (status == SQLITE_ROW) {
      ((folder_data*)*folderdata)->previousfolderid = sqlite3_column_int64(sqlresult, 0);
    }
    sqlite3_clear_bindings(sqlresult);
    sqlite3_reset(sqlresult);
  }
  return 0;
}

int folder_callback_after (DIRTRAVFN(entry) info)
{
  int status;
  DIRTRAVCHAR* ownerid;
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)info->callbackdata;
	const DIRTRAVCHAR* fullpath = DIRTRAVFN(prop_get_path)(info);
  folder_data** folderdata = (folder_data**)&(info->folderlocaldata);
  const folder_data* parentfolderdata = (info->parentinfo ? info->parentinfo->folderlocaldata : NULL);
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
    SQLITE3FN(bind_text)(sqlresult, 3, fullpath, -1, NULL);
    if ((ownerid = DIRTRAVFN(prop_get_ownerid)(info)) != NULL)
      SQLITE3FN(bind_text)(sqlresult, 4, ownerid, -1, NULL);
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
    sqlite3_clear_bindings(sqlresult);
    sqlite3_reset(sqlresult);
    free(ownerid);
    //notify of progress (flush if needed)
    file_folder_callback_data_progress(file_callback_data);
  }
  //clean up folder data
  free(*folderdata);
  return 0;
}

int file_callback (DIRTRAVFN(entry) info)
{
  int status;
  DIRTRAVCHAR* ownerid;
  int64_t filesize;
  time_t createdtimestamp = 0;
/*
  time_t modifiedtimestamp = 0;
*/
  time_t latesttimestamp = 0;
  int have_previous_crc32 = 0;
  int have_previous_sha1 = 0;
  int have_previous_adler32 = 0;
  uint32_t previous_crc32;
  char previous_sha1[41];
  uint32_t previous_adler32;
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)info->callbackdata;
  const DIRTRAVCHAR* filename = DIRTRAVFN(prop_get_name)(info);
  const folder_data* parentfolderdata = (info->parentinfo ? info->parentinfo->folderlocaldata : NULL);
  filesize = (int64_t)DIRTRAVFN(prop_get_size)(info);
  createdtimestamp = DIRTRAVFN(prop_get_create_time)(info);
  //determine latest time stamp (between created/modified/accessed)
  //note: modified timestamp is sometimes set to a date in the future (especially by digital cameras)
  latesttimestamp = DIRTRAVFN(prop_get_access_time)(info);
/*
  modifiedtimestamp = DIRTRAVFN(prop_get_modify_time)(info);
  if (modifiedtimestamp > latesttimestamp)
    latesttimestamp = modifiedtimestamp ;
*/
  if (createdtimestamp > latesttimestamp)
    latesttimestamp = createdtimestamp;
  //update data
  file_callback_data->filecount++;
  file_callback_data->totalsize += filesize;
  if (parentfolderdata) {
    ((folder_data*)parentfolderdata)->filecount++;
    ((folder_data*)parentfolderdata)->totalsize += filesize;
    //get hash keys from same file in data of previous run
    if (((folder_data*)parentfolderdata)->previousfolderid != 0) {
      int status;
      sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_get_previous_file_hashes;
      sqlite3_bind_int64(sqlresult, 1, ((folder_data*)parentfolderdata)->previousfolderid);
      SQLITE3FN(bind_text)(sqlresult, 2, filename, -1, NULL);
      sqlite3_bind_int64(sqlresult, 3, filesize);
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
          strncpy(previous_sha1, (char*)sqlite3_column_text(sqlresult, 1), 40);
          previous_sha1[40] = 0;
        }
        if (sqlite3_column_type(sqlresult, 2) != SQLITE_NULL) {
          have_previous_adler32 = 1;
          previous_adler32 = sqlite3_column_int64(sqlresult, 2);
        }
      }
      sqlite3_clear_bindings(sqlresult);
      sqlite3_reset(sqlresult);
    }
  }
  //update database
  if (file_callback_data->sqlstmt_insert_file) {
    sqlite3_stmt* sqlresult = file_callback_data->sqlstmt_insert_file;
    sqlite3_bind_int64(sqlresult, 1, file_callback_data->filecount);
    if (parentfolderdata && ((folder_data*)parentfolderdata)->id)
      sqlite3_bind_int64(sqlresult, 2, ((folder_data*)parentfolderdata)->id);
    else
      sqlite3_bind_null(sqlresult, 2);
    SQLITE3FN(bind_text)(sqlresult, 3, filename, -1, NULL);
    SQLITE3FN(bind_text)(sqlresult, 4, DIRTRAVFN(prop_get_extension)(info), -1, NULL);
    sqlite3_bind_int64(sqlresult, 5, filesize);
    if (createdtimestamp)
      sqlite3_bind_int64(sqlresult, 6, createdtimestamp);
    else
      sqlite3_bind_null(sqlresult, 6);
    if (latesttimestamp)
      sqlite3_bind_int64(sqlresult, 7, latesttimestamp);
    else
      sqlite3_bind_null(sqlresult, 7);
    if ((ownerid = DIRTRAVFN(prop_get_ownerid)(info)) != NULL)
      SQLITE3FN(bind_text)(sqlresult, 8, ownerid, -1, NULL);
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
    sqlite3_clear_bindings(sqlresult);
    sqlite3_reset(sqlresult);
    free(ownerid);
    //notify of progress (flush if needed)
    file_folder_callback_data_progress(file_callback_data);
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////

#ifdef USE_WIDE_STRINGS
#  define DIRCHAR wchar_t
#  define DIRSTRLEN wcslen
#  define DIRSTRCPY wcscpy
#  define DIRSTRDUP wcsdup
#  define DIRWINFN(s) s##W
#else
#  define DIRCHAR char
#  define DIRSTRLEN strlen
#  define DIRSTRCPY strcpy
#  define DIRSTRDUP strdup
#  define DIRWINFN(s) s##A
#endif

static inline DIRCHAR* sid_to_string (PSID sid)
{
  DIRCHAR* result;
  SID_NAME_USE accounttype;
  DIRCHAR* name;
  DIRCHAR* domain;
  DWORD len = 0;
  DWORD domainlen = 0;
  //if (DIRWINFN(LookupAccountSid)(NULL, sid, NULL, &len, NULL, &domainlen, &accounttype) == 0) {
  if (DIRWINFN(LookupAccountSid)(DIRTRAVTEXT("SERVER"), sid, NULL, &len, NULL, &domainlen, &accounttype) == 0) {

    DWORD errorcode = GetLastError();
    DIRCHAR* errormessage;
    if (DIRWINFN(FormatMessage)(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (DIRCHAR*)&errormessage, 0, NULL) != 0) {
      DIRTRAVPRINTF(DIRTRAVTEXT("Error: %ls\n"), errormessage);
      LocalFree(errormessage);
    }
  }


  if (len > 0 && domainlen > 0) {
    if (!(name = (DIRCHAR*)malloc(len * sizeof(DIRCHAR)))) {
      return NULL;
    }
    if (!(domain = (DIRCHAR*)malloc(domainlen * sizeof(DIRCHAR)))) {
      free(name);
      return NULL;
    }
    //if (DIRWINFN(LookupAccountSid)(NULL, sid, name, &len, domain, &domainlen, &accounttype)) {
    if (DIRWINFN(LookupAccountSid)(DIRTRAVTEXT("SERVER"), sid, name, &len, domain, &domainlen, &accounttype)) {
      if ((result = (DIRCHAR*)malloc((domainlen + len + 2) * sizeof(DIRCHAR))) == NULL) {
        free(name);
        free(domain);
        return NULL;
      } else {
        memcpy(result, domain, domainlen * sizeof(DIRCHAR));
        result[domainlen] = '\\';
        DIRSTRCPY(result + domainlen + 1, name);
      }
    }
    free(name);
    free(domain);
  } else {
    result = NULL;
/*
    DWORD errorcode = GetLastError();
    DIRCHAR* errormessage;
    if (DIRWINFN(FormatMessage)(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (DIRCHAR*)&errormessage, 0, NULL) != 0) {
      DIRPRINTF(DIRTEXT("Error: %s\n"), errormessage);
      LocalFree(errormessage);
    }
*/
  }
  if (!result) {
    DIRCHAR* sidstring;
    if (DIRWINFN(ConvertSidToStringSid)(sid, &sidstring)) {
      result = DIRSTRDUP(sidstring);
      LocalFree(sidstring);
    }
  }
  return result;
}

DIRCHAR* userid_to_name (const DIRCHAR* userid)
{
#ifdef _WIN32
  DIRCHAR* result = NULL;
  PSID sid;
  if (DIRWINFN(ConvertStringSidToSid)(userid, &sid) == 0)
    return NULL;
  result = sid_to_string(sid);
  LocalFree(sid);
  return result;
#else
  struct passwd* pw;
  char* p = userid;
  long long id = strtoll(userid, &p, 10);
  if (p == userid || id < 0 || id > (1 << sizeof(pid_t)) - 1)
    return NULL;
  return uid_to_string(id);
#endif
}

#include <winnetwk.h>
#include <npapi.h>

////////////////////////////////////////////////////////////////////////

int file_folder_lookup_users (file_folder_callback_data* file_callback_data)
{
  int status;
	sqlite3_stmt* sqlresult;
  if ((sqlresult = execute_sql_query(file_callback_data->sqliteconn, "SELECT DISTINCT ownerid FROM (SELECT DISTINCT ownerid FROM folder UNION SELECT DISTINCT ownerid FROM file)", &status)) != NULL) {
    if (status == SQLITE_ROW || status == SQLITE_DONE) {
      DIRTRAVCHAR* userid;
      DIRTRAVCHAR* username;
      while (status == SQLITE_ROW) {
        userid = (const DIRTRAVCHAR*)SQLITE3FN(column_text)(sqlresult, 0);
DIRTRAVPRINTF(DIRTRAVTEXT("SID: "));/////
DIRTRAVPRINTF(userid);/////
DIRTRAVPRINTF(DIRTRAVTEXT("\n"));/////
//username = DIRTRAVFN(userid_to_name)(userid);
username = userid_to_name(userid);
DIRTRAVPRINTF(DIRTRAVTEXT("Name: "));/////
DIRTRAVPRINTF(username);/////
DIRTRAVPRINTF(DIRTRAVTEXT("\n"));/////
        dirtravw_free(username);
        while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
          WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
      }
    }
    sqlite3_finalize(sqlresult);
	}

/*
	DIRWINFN(REMOTE_NAME_INFO) remotepathinfo;
	DWORD remotepathinfolen = sizeof(remotepathinfo);
	if (DIRWINFN(WNetGetUniversalName)(DIRTRAVTEXT("\\\\SERVER\\Users\\Brecht"), REMOTE_NAME_INFO_LEVEL, &remotepathinfo, &remotepathinfolen) == NO_ERROR) {
DIRTRAVPRINTF(remotepathinfo.lpConnectionName);
	} else {
    DWORD errorcode = GetLastError();
    DIRCHAR* errormessage;
    if (DIRWINFN(FormatMessage)(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (DIRCHAR*)&errormessage, 0, NULL) != 0) {
      DIRTRAVPRINTF(DIRTRAVTEXT("Error: %Ls\n"), errormessage);
      LocalFree(errormessage);
    }
	}
DIRTRAVPRINTF(DIRTRAVTEXT("\n"));/////
*/

	REMOTE_NAME_INFOW remotepathinfo;
	DWORD remotepathinfolen = sizeof(remotepathinfo);
	DWORD errcode;
	if ((errcode = NPGetUniversalName(DIRTRAVTEXT("\\\\SERVER\\Users\\Brecht"), REMOTE_NAME_INFO_LEVEL, &remotepathinfo, &remotepathinfolen)) == WN_SUCCESS) {
DIRTRAVPRINTF(remotepathinfo.lpConnectionName);
	} else {
    DWORD errorcode = GetLastError();
    DIRCHAR* errormessage;
    if (DIRWINFN(FormatMessage)(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (DIRCHAR*)&errormessage, 0, NULL) != 0) {
      DIRTRAVPRINTF(DIRTRAVTEXT("Error: %Ls\n"), errormessage);
      LocalFree(errormessage);
    }
	}
DIRTRAVPRINTF(DIRTRAVTEXT("\n"));/////

	return 0;
}

////////////////////////////////////////////////////////////////////////

#ifdef USE_WIDE_STRINGS
int wmain(int argc, wchar_t* argv[]);

int main (int argc, char* argv[])
{
  int result;
  int wide_argc;
  LPWSTR* wide_argv;
  wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
  result = wmain(wide_argc, wide_argv);
  LocalFree(wide_argv);
  return result;
}

int wmain(int argc, wchar_t* argv[])
#else
int main (int argc, char* argv[])
#endif
{
  const char* dbname = "tempdb.sq3";
  const char* previousdbname = "tempdbold.sq3";
  clock_t starttime;
  int i;
  file_folder_callback_data file_callback_data;

  //check command line parameters
  if (argc > 1) {
    if (DIRTRAVSTRCMP(argv[1], DIRTRAVTEXT("-v")) == 0) {
      printf("%s version %s\n", APPLICATION_NAME, FOLDERREPORTS_VERSION_STRING);
      return 0;
    } else if (DIRTRAVSTRCMP(argv[1], DIRTRAVTEXT("-h")) == 0 || DIRTRAVSTRCMP(argv[1], DIRTRAVTEXT("-?")) == 0) {
      printf(
        "Usage: %s PATH...\n"
        "Builds a database of all files and folders under the specified paths(s)\n"
        "and stores this information in %s.\n"
        "If this file already exists it is first renamed to %s.\n", APPLICATION_NAME, "tempdb.sq3", "tempdbold.sq3");
      return 0;
    }
  }

  //elevate access
  DIRTRAVFN(elevate_access)();

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

  //change database settings for optimal performance
  sqlite3_exec(file_callback_data.sqliteconn, "PRAGMA encoding = \"UTF-8\"", NULL, NULL, NULL);
  sqlite3_exec(file_callback_data.sqliteconn, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
  sqlite3_exec(file_callback_data.sqliteconn, "PRAGMA journal_mode=OFF", NULL, NULL, NULL);
  sqlite3_exec(file_callback_data.sqliteconn, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  //prepare database
#ifdef USE_DB_TRANSACTIONS
  execute_sql_cmd(file_callback_data.sqliteconn, "BEGIN");
#endif
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS folder");
  //execute_sql_cmd(file_callback_data.sqliteconn, "DROP INDEX IF EXISTS idx_file_size");
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS file");
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS user");
  execute_sql_cmd(file_callback_data.sqliteconn, "DROP TABLE IF EXISTS duplicatefiles");
  execute_sql_cmd(file_callback_data.sqliteconn,
    "CREATE TABLE folder ("
    " id INT NOT NULL PRIMARY KEY,"
    " parent INT,"
    " path TEXT NOT NULL,"
    " ownerid TEXT,"
    " filecount INT DEFAULT NULL,"
    " foldercount INT DEFAULT NULL,"
    " totalsize INT DEFAULT NULL,"
    " filecountrecursive INT DEFAULT NULL,"
    " foldercountrecursive INT DEFAULT NULL,"
    " totalsizerecursive INT DEFAULT NULL"
    ")");
  execute_sql_cmd(file_callback_data.sqliteconn,
    "CREATE TABLE file ("
    " id INT NOT NULL PRIMARY KEY,"
    " folder INT,"
    " name TEXT NOT NULL,"
    " extension TEXT,"
    " size INT NOT NULL,"
    " created INT,"
    " lastaccess INT,"
    " ownerid TEXT,"
    " crc32 INT DEFAULT NULL,"
    " sha1 VARCHAR(40) DEFAULT NULL,"
    " adler32 INT DEFAULT NULL"
    ")");
  execute_sql_cmd(file_callback_data.sqliteconn,
    "CREATE TABLE user ("
    " userid TEXT NOT NULL PRIMARY KEY,"
    " name TEXT NOT NULL"
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
    for (i = 1; i < argc; i++) {
#ifdef USE_DB_TRANSACTIONS
      execute_sql_cmd(file_callback_data.sqliteconn, "BEGIN");
#endif
      DIRTRAVFN(traverse_directory)(argv[i], &file_callback, &folder_callback_before, &folder_callback_after, &file_callback_data);
#ifdef USE_DB_TRANSACTIONS
      execute_sql_cmd(file_callback_data.sqliteconn, "COMMIT");
#endif
    }
    //show elapsed time
    printf("\relapsed time processing folders: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

    //look up user IDs
    file_folder_lookup_users(&file_callback_data);

    //clean up
    printf("resolving user IDs...  ");
    starttime = clock();
    file_folder_callback_data_cleanup(&file_callback_data);
    printf("\relapsed time resolving user IDs: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

#ifdef USE_TEMP_DB
    //copy data to disk
    sqlite3* sqliteconn;
    sqlite3_backup* dbcopy;
    printf("writing data to disk...  ");
    starttime = clock();
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
  printf("%" PRIu64 " files found in %" PRIu64 " folders, total size: %" PRIu64 " M\n", (uint64_t)file_callback_data.filecount, (uint64_t)file_callback_data.foldercount, (uint64_t)file_callback_data.totalsize / 1024 / 1024);

  return 0;
}

//sqlite3 -csv -header tempdb.sq3 "SELECT folder.path, file.name AS filename, file.size, file.ownerid, file.crc32, file.sha1, file.adler32 FROM file LEFT JOIN folder ON file.folder = folder.id"
