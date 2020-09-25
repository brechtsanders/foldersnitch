#ifdef WIN32
#include <windows.h>
#define USE_WIDE_STRINGS
#endif
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "sqlitefunctions.h"
#include "folderreportsversion.h"
#include "hashconfig.h"
#if defined(HASH_MHASH_CRC32) || defined(HASH_MHASH_SHA1) || defined(HASH_MHASH_ADLER32)
#define USING_MHASH
#include <mhash.h>
#endif
#if defined(HASH_RHASH_CRC32) || defined(HASH_RHASH_SHA1)
#define USING_RHASH
#include <rhash.h>
#endif
#if defined(HASH_OPENSSL_SHA1)
#define USING_OPENSSL
#include <openssl/sha.h>
#endif
#if defined(HASH_NETTLE_SHA1)
#define USING_NETTLE
#include <nettle/sha1.h>
#endif
#define BUILD_DIRTRAV_STATIC
#ifdef USE_WIDE_STRINGS
#include <dirtravw.h>
#define DIRTRAVFN(fn) dirtravw_##fn
#else
#include <dirtrav.h>
#define DIRTRAVFN(fn) dirtrav_##fn
#endif

#define APPLICATION_NAME "findduplicates"

#if !defined(HASH_MHASH_CRC32) && !defined(HASH_MHASH_SHA1) && !defined(HASH_MHASH_ADLER32) && !defined(HASH_RHASH_CRC32) && !defined(HASH_RHASH_SHA1) && !defined(HASH_OPENSSL_SHA1) && !defined(HASH_NETTLE_SHA1)
#define DO_REAL_COMPARE
#define NO_HASH
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

#ifndef NO_HASH
#ifdef USE_WIDE_STRINGS
int hash_file (const wchar_t* filename, void* crc32, void* sha1, void* adler32)
#else
int hash_file (const char* filename, void* crc32, void* sha1, void* adler32)
#endif
//crc32 = 4 bytes; sha1 = 20 bytes; adler32 = 4 bytes
{
  FILE* src;
  char* buf;
  int len;
  int success = 0;
#ifdef HASH_MHASH_CRC32
  MHASH mhashcrc32 = mhash_init(MHASH_CRC32);
#endif
#ifdef HASH_MHASH_SHA1
  MHASH mhashsha1 = mhash_init(MHASH_SHA1);
#endif
#ifdef HASH_MHASH_ADLER32
  MHASH mhashadler32 = mhash_init(MHASH_ADLER32);
#endif
#ifdef USING_RHASH
  rhash rhashcontext = rhash_init(
#ifdef HASH_RHASH_CRC32
    RHASH_CRC32 |
#endif
#ifdef HASH_RHASH_SHA1
    RHASH_SHA1 |
#endif
  0);
#endif
#ifdef HASH_OPENSSL_SHA1
  SHA_CTX opensslsha1;
  SHA1_Init(&opensslsha1);
#endif
#ifdef HASH_NETTLE_SHA1
  struct sha1_ctx nettlesha1;
  sha1_init(&nettlesha1);
#endif
#ifdef USE_WIDE_STRINGS
  if ((src = _wfopen(filename, L"rb")) != NULL) {
#else
  if ((src = fopen(filename, "rb")) != NULL) {
#endif
    if ((buf = (char*)malloc(HASH_READ_BUFFER_SIZE)) != NULL) {
      while ((len = fread(buf, 1, sizeof(buf), src)) > 0) {
#ifdef HASH_MHASH_CRC32
        if (mhashcrc32 != MHASH_FAILED)
          mhash(mhashcrc32, buf, len);
#endif
#ifdef HASH_MHASH_SHA1
        if (mhashsha1 != MHASH_FAILED)
          mhash(mhashsha1, buf, len);
#endif
#ifdef HASH_MHASH_ADLER32
        if (mhashadler32 != MHASH_FAILED)
          mhash(mhashadler32, buf, len);
#endif
#ifdef USING_RHASH
        if (rhashcontext)
          rhash_update(rhashcontext, buf, len);
#endif
#ifdef HASH_OPENSSL_SHA1
        SHA1_Update(&opensslsha1, buf, len);
#endif
#ifdef HASH_NETTLE_SHA1
        sha1_update(&nettlesha1, len, (uint8_t*)buf);
#endif
      }
      success = 1;
      free(buf);
    }
    fclose(src);
  }
  if (success) {
#ifdef HASH_MHASH_CRC32
    if (mhashcrc32 != MHASH_FAILED)
      mhash_deinit(mhashcrc32, crc32);
#endif
#ifdef HASH_MHASH_SHA1
    if (mhashsha1 != MHASH_FAILED)
      mhash_deinit(mhashsha1, sha1);
#endif
#ifdef HASH_MHASH_ADLER32
    if (mhashadler32 != MHASH_FAILED)
      mhash_deinit(mhashadler32, sha1);
#endif
#ifdef USING_RHASH
    rhash_final(rhashcontext, NULL);
#ifdef HASH_RHASH_CRC32
    rhash_print((char*)crc32, rhashcontext, RHASH_CRC32, RHPR_RAW);
#endif
#ifdef HASH_RHASH_SHA1
    rhash_print((char*)sha1, rhashcontext, RHASH_SHA1, RHPR_RAW);
#endif
#endif
#ifdef HASH_OPENSSL_SHA1
    SHA1_Final((unsigned char*)sha1, &opensslsha1);
#endif
#ifdef HASH_NETTLE_SHA1
    sha1_digest(&nettlesha1, SHA1_DIGEST_SIZE, (uint8_t*)sha1);
#endif
  }
  return success;
}
#endif

#ifdef DO_REAL_COMPARE
#define FILECMP_EQUAL      0
#define FILECMP_NOTEQUAL   -1
#define FILECMP_MEMERROR   -2
#define FILECMP_OPENFAILED -3
int compare_files (const char* file1, const char* file2)
{
  FILE* f1;
  FILE* f2;
  int len;
  char* buf1;
  char* buf2;
  int status = FILECMP_EQUAL;
  //allocate buffer
  if ((buf1 = (char*)malloc(COMPARE_READ_BUFFER_SIZE * 2)) == NULL)
    return FILECMP_MEMERROR;
  buf2 = buf1 + COMPARE_READ_BUFFER_SIZE;
  if ((f1 = fopen(file1, "rb")) == NULL) {
    status = FILECMP_OPENFAILED;
  } else {
    if ((f2 = fopen(file2, "rb")) != NULL) {
      status = FILECMP_OPENFAILED;
    } else {
      do {
        if ((len = fread(buf1, 1, sizeof(buf1), f1)) != fread(buf2, 1, sizeof(buf2), f2)) {
          status = FILECMP_NOTEQUAL;
          break;
        } else if (len > 0 && memcmp(buf1, buf2, len) != 0) {
          status = FILECMP_NOTEQUAL;
          break;
        }
      } while (len == sizeof(buf1) /*&& status == FILECMP_EQUAL*/);
      fclose(f2);
    }
    fclose(f1);
  }
  free(buf1);
  return status;
}
#endif

//#define STRINGIFY_(n) #n
//#define STRINGIFY(n) STRINGIFY_(n)

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
        "Looks for files with identical sizes in database %s\n"
        "and calculates their hash to determine if they are identical.\n", APPLICATION_NAME, "tempdb.sq3");
      return 0;
    }
  }

  //elevate access
  DIRTRAVFN(elevate_access)();

  //initialize
#ifdef USING_RHASH
  rhash_library_init();
#endif
	if (sqlite3_open(dbname, &sqliteconn) != SQLITE_OK) {
		return 1;
  }

  //change database settings for optimal performance
  sqlite3_exec(sqliteconn, "PRAGMA encoding = \"UTF-8\"", NULL, NULL, NULL);
  sqlite3_exec(sqliteconn, "PRAGMA synchronous=OFF", NULL, NULL, NULL);
  sqlite3_exec(sqliteconn, "PRAGMA journal_mode=OFF", NULL, NULL, NULL);
  sqlite3_exec(sqliteconn, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  //create table and indices
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_file_size ON file (size DESC)");
  execute_sql_cmd(sqliteconn, "DROP TABLE IF EXISTS duplicatefiles");
  execute_sql_cmd(sqliteconn, "BEGIN");
  execute_sql_cmd(sqliteconn,
    "CREATE TABLE duplicatefiles (\n"
    "  file1 INT NOT NULL,\n"
    "  file2 INT NOT NULL\n"
    ")");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_duplicatefiles_file1 ON duplicatefiles (file1)");
  execute_sql_cmd(sqliteconn,
    "CREATE INDEX IF NOT EXISTS idx_duplicatefiles_file2 ON duplicatefiles (file2)");
  execute_sql_cmd(sqliteconn, "COMMIT");

  //start timer
  printf("checking for duplicate files...  ");
  starttime = clock();
  //search for duplicate files by comparing all files with the same size
  int status;
	sqlite3_stmt* sqlresult;
  int progress = 0;
  size_t newsincecommit = 0;
  clock_t lastcommit = starttime;
  execute_sql_cmd(sqliteconn, "BEGIN");
  if ((sqlresult = execute_sql_query_with_1_parameter(sqliteconn,
    "SELECT\n"
    "  size,\n"
    "  COUNT(*)\n"
    "FROM file\n"
    "WHERE size >= ?\n"
    "GROUP BY size\n"
    "HAVING COUNT(*) > 1", &status, (MINIMUM_DUPLICATE_FILE_SIZE > 0 ? MINIMUM_DUPLICATE_FILE_SIZE : 1))) != NULL) {
    if (status == SQLITE_ROW || status == SQLITE_DONE) {
      while (status == SQLITE_ROW) {
        int substatus;
        sqlite3_stmt* subsqlresult;
#ifndef NO_HASH
#ifdef DO_REAL_COMPARE
        //hash files if there are more than 2 of the same size
        if (sqlite3_column_int64(sqlresult, 1) > 2) {
#else
        //hash files
        {
#endif
          if ((subsqlresult = execute_sql_query_with_1_parameter(sqliteconn,
            "SELECT\n"
            "  file.id,\n"
            "  folder.path || file.name AS fullpath\n"
            "FROM file\n"
            "JOIN folder ON file.folder = folder.id\n"
            "WHERE file.size = ? AND (0=1"
#if defined(HASH_MHASH_CRC32) || defined(HASH_RHASH_CRC32)
            " OR crc32 IS NULL"
#endif
#if defined(HASH_MHASH_SHA1) || defined(HASH_RHASH_SHA1) || defined(HASH_OPENSSL_SHA1) || defined(HASH_NETTLE_SHA1)
            " OR sha1 IS NULL"
#endif
#if defined(HASH_MHASH_ADLER32)
            " OR adler32 IS NULL"
#endif
            ")", &substatus, sqlite3_column_int64(sqlresult, 0))) != NULL) {
            if (substatus == SQLITE_ROW || substatus == SQLITE_DONE) {
              while (substatus == SQLITE_ROW) {
                uint32_t crc32;
                unsigned char sha1[20];
                uint32_t adler32;
                show_progress_movement(progress++);
#ifdef USE_WIDE_STRINGS
                if (hash_file((const wchar_t*)sqlite3_column_text16(subsqlresult, 1), &crc32, sha1, &adler32)) {
#else
                if (hash_file((const char*)sqlite3_column_text(subsqlresult, 1), &crc32, sha1, &adler32)) {
#endif
                  int subsubstatus;
                  sqlite3_stmt* subsubsqlresult;
#if defined(HASH_MHASH_SHA1) || defined(HASH_RHASH_SHA1) || defined(HASH_OPENSSL_SHA1) || defined(HASH_NETTLE_SHA1)
                  int i;
                  const char hexdigit[] = "0123456789abcdef";
                  char sha1text[40];
                  for (i = 0; i < 20; i++) {
                    sha1text[i * 2] = hexdigit[(sha1[i] & 0xF0) >> 4];
                    sha1text[i * 2 + 1] = hexdigit[sha1[i] & 0x0F];
                  }
#endif
                  if ((subsubstatus = sqlite3_prepare_v2(sqliteconn, "UPDATE file SET crc32 = ?, sha1 = ?, adler32 = ? WHERE id = ?", -1, &subsubsqlresult, NULL)) == SQLITE_OK) {
#if defined(HASH_MHASH_CRC32) || defined(HASH_RHASH_CRC32)
                    sqlite3_bind_int64(subsubsqlresult, 1, crc32);
#else
                    sqlite3_bind_null(subsubsqlresult, 1);
#endif
#if defined(HASH_MHASH_SHA1) || defined(HASH_RHASH_SHA1) || defined(HASH_OPENSSL_SHA1) || defined(HASH_NETTLE_SHA1)
                    sqlite3_bind_text(subsubsqlresult, 2, sha1text, 40, NULL);
#else
                    sqlite3_bind_null(subsubsqlresult, 2);
#endif
#if defined(HASH_MHASH_ADLER32)
                    sqlite3_bind_int64(subsubsqlresult, 3, adler32);
#else
                    sqlite3_bind_null(subsubsqlresult, 3);
#endif
                    sqlite3_bind_int64(subsubsqlresult, 4, sqlite3_column_int64(subsqlresult, 0));
                    while ((subsubstatus = sqlite3_step(subsubsqlresult)) == SQLITE_BUSY)
                      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
                    sqlite3_finalize(subsubsqlresult);
                    newsincecommit++;
                  }
                }
                while ((substatus = sqlite3_step(subsqlresult)) == SQLITE_BUSY)
                  WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
              }
            }
            sqlite3_finalize(subsqlresult);
          }
        }
#endif
        //compare files with same size and same hash keys
        if ((subsqlresult = execute_sql_query_with_1_parameter(sqliteconn,
          "SELECT\n"
          "  file1.id,\n"
          "  file2.id,\n"
          "  folder1.path || file1.name AS fullpath1,\n"
          "  folder2.path || file2.name AS fullpath2\n"
          "FROM file AS file1\n"
          "JOIN file AS file2 ON file1.size = file2.size\n"
          "JOIN folder AS folder1 ON file1.folder = folder1.id\n"
          "JOIN folder AS folder2 ON file2.folder = folder2.id\n"
          "WHERE file1.size = ?\n"
          "  AND file1.id < file2.id\n"
#if defined(HASH_MHASH_CRC32) || defined(HASH_RHASH_CRC32)
          "  AND (file1.crc32 = file2.crc32"
#ifdef DO_REAL_COMPARE
          "    OR (file1.crc32 IS NULL AND file2.crc32 IS NULL)"
#else
          "    AND file1.crc32 IS NOT NULL AND file2.crc32 IS NOT NULL"
#endif
          "  )\n"
#endif
#if defined(HASH_MHASH_SHA1) || defined(HASH_RHASH_SHA1) || defined(HASH_OPENSSL_SHA1) || defined(HASH_NETTLE_SHA1)
          "  AND (file1.sha1 = file2.sha1"
#ifdef DO_REAL_COMPARE
          "    OR (file1.sha1 IS NULL AND file2.sha1 IS NULL)"
#else
          "    AND file1.sha1 IS NOT NULL AND file2.sha1 IS NOT NULL"
#endif
          "  )\n"
#endif
#ifdef HASH_MHASH_ADLER32
          "  AND (file1.adler32 = file2.adler32"
#ifdef DO_REAL_COMPARE
          "    OR (file1.adler32 IS NULL AND file2.adler32 IS NULL)"
#else
          "    AND file1.adler32 IS NOT NULL AND file2.adler32 IS NOT NULL"
#endif
          "  )\n"
#endif
          "ORDER BY file1.id, file2.id", &substatus, sqlite3_column_int64(sqlresult, 0))) != NULL) {
          if (substatus == SQLITE_ROW || substatus == SQLITE_DONE) {
            while (substatus == SQLITE_ROW) {
              //only compare files if they were not already matched
              if (get_single_value_sql_with_2_parameters(sqliteconn, "SELECT COUNT(*) FROM duplicatefiles WHERE file2 = ? OR file2 = ?", sqlite3_column_int64(subsqlresult, 0), sqlite3_column_int64(subsqlresult, 1), 0) == 0) {
#ifdef DO_REAL_COMPARE
                show_progress_movement(progress++);
                if (compare_files((const char*)sqlite3_column_text(subsqlresult, 2), (const char*)sqlite3_column_text(subsqlresult, 3)) == FILECMP_EQUAL) {
#else
                {
#endif
                  //update database
                  int subsubstatus;
                  sqlite3_stmt* subsubsqlresult;
                  if ((subsubstatus = sqlite3_prepare_v2(sqliteconn, "INSERT INTO duplicatefiles (file1, file2) VALUES (?, ?)", -1, &subsubsqlresult, NULL)) == SQLITE_OK) {
                    sqlite3_bind_int64(subsubsqlresult, 1, sqlite3_column_int64(subsqlresult, 0));
                    sqlite3_bind_int64(subsubsqlresult, 2, sqlite3_column_int64(subsqlresult, 1));
                    while ((subsubstatus = sqlite3_step(subsubsqlresult)) == SQLITE_BUSY)
                      WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
                    sqlite3_finalize(subsubsqlresult);
                    newsincecommit++;
                  }
                }
              }
              while ((substatus = sqlite3_step(subsqlresult)) == SQLITE_BUSY)
                WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
            }
          }
          sqlite3_finalize(subsqlresult);
        }
        //commit if long enough ago
        if (newsincecommit > 0 && newsincecommit % COMMIT_FORCE_CHECK_EVERY == 0) {
          if (newsincecommit >= COMMIT_FORCE_AMOUNT || clock() > lastcommit + COMMIT_FORCE_INTERVAL * CLOCKS_PER_SEC) {
            show_progress_flush();
            execute_sql_cmd(sqliteconn, "COMMIT");
            execute_sql_cmd(sqliteconn, "BEGIN");
            lastcommit = clock();
            newsincecommit = 0;
          }
        }
        //get next record
        while ((status = sqlite3_step(sqlresult)) == SQLITE_BUSY)
          WAIT_BEFORE_RETRY(RETRY_WAIT_TIME)
      }
    }
    sqlite3_finalize(sqlresult);
  }
  execute_sql_cmd(sqliteconn, "COMMIT");
  printf("\relapsed time comparing files with identical sizes: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);

  //clean up database connection
  sqlite3_close(sqliteconn);

  return 0;
}
