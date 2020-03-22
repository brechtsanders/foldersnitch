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
#include "traverse_directory.h"

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
  time_t movebeforetime;
  uint64_t filecount;
  uint64_t totalsize;
  uint64_t foldercount;
} file_folder_callback_data;

int file_folder_callback_data_initialize (file_folder_callback_data* file_callback_data, time_t move_before_time)
{
  file_callback_data->movebeforetime = move_before_time;
  file_callback_data->filecount = 0;
  file_callback_data->totalsize = 0;
  file_callback_data->foldercount = 0;
  return 0;
}

void file_folder_callback_data_cleanup (file_folder_callback_data* file_callback_data)
{
}

void show_progress_movement (int index)
{
  const int progresscharstotal = 4;
  const char progresschars[] = "/-\\|";
  printf("\b%c", progresschars[index % progresscharstotal]);
}

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
  //stop processing if the entry is a link
  if (is_link(fullpath))
    return -1;

  return 0;
}

#ifdef USE_WIDE_STRINGS
int folder_callback_after (const wchar_t* fullpath, void* callbackdata, void** folderdata, void* parentfolderdata)
#else
int folder_callback_after (const char* fullpath, void* callbackdata, void** folderdata, void* parentfolderdata)
#endif
{
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
  STAT_STRUCT statbuf;
  time_t createdtimestamp = 0;
  time_t latesttimestamp = 0;
	file_folder_callback_data* file_callback_data = (file_folder_callback_data*)callbackdata;
  if (STATOK(fullpath, &statbuf)) {
    createdtimestamp = statbuf.st_ctime;
    //determine latest time stamp (between created/modified/accessed)
    latesttimestamp = statbuf.st_atime;
    //note: st_mtime is sometimes set to a date in the future (especially by digital cameras)
    if (statbuf.st_mtime > latesttimestamp)
      latesttimestamp = statbuf.st_mtime;
    if (statbuf.st_ctime > latesttimestamp)
      latesttimestamp = statbuf.st_ctime;
    //update data
    file_callback_data->filecount++;
    file_callback_data->totalsize += statbuf.st_size;
    if (parentfolderdata) {
      ((folder_data*)parentfolderdata)->filecount++;
      ((folder_data*)parentfolderdata)->totalsize += statbuf.st_size;
    }

    if (latesttimestamp < file_callback_data->movebeforetime) {
#ifdef USE_WIDE_STRINGS
      wprintf(L"\r%s%s\n", directory, filename);
#else
      printf("\r%s%s\n", directory, filename);
#endif
    }

  }
  return 0;
}

////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
  time_t movebeforetime = 0;
  clock_t starttime;
  int i;
  file_folder_callback_data file_callback_data;

  if (argc % 2 != 1) {
    fprintf(stderr, "Invalid number of parameters\nUsage: %s {src} {dst} ...", argv[0]);
    return 1;
  }

  movebeforetime = time(NULL) - 365 * 24 * 60 * 60;

  //start timer
  printf("processing folders...  ");
  starttime = clock();
  //process folders
  if (file_folder_callback_data_initialize(&file_callback_data, movebeforetime) == 0) {
    //treat each command line argument as a folder to process
#ifdef USE_WIDE_STRINGS
    LPWSTR *wide_argv;
    int wide_argc;
    wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    for (i = 1; i < wide_argc; i++) {
#else
    for (i = 1; i < argc; i += 2) {
#endif
#ifdef USE_WIDE_STRINGS
      wide_traverse_directory(wide_argv[i], &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);
#else
      traverse_directory(argv[i], &file_callback, &file_callback_data, &folder_callback_before, &folder_callback_after, &file_callback_data, NULL);
#endif
    }
#ifdef USE_WIDE_STRINGS
    LocalFree(wide_argv);
#endif
    file_folder_callback_data_cleanup(&file_callback_data);
    //show elapsed time
    printf("\relapsed time processing folders: %.3f s\n", (float)(clock() - starttime) / CLOCKS_PER_SEC);
  }

  //show information
  printf("%" PRINTF_INT64_MODIFIER "u files found in %" PRINTF_INT64_MODIFIER "u folders, total size: %" PRINTF_INT64_MODIFIER "u M\n", (uint64_t)file_callback_data.filecount, (uint64_t)file_callback_data.foldercount, (uint64_t)file_callback_data.totalsize / 1024 / 1024);

  return 0;
}
