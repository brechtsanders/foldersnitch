FolderSnitch
============
Cross-platform command-line tools to get reports on file and folder usage, including duplicate files.

Description
-----------
FolderSnitch consists of a series of tools to collect data about folder structures and the files in them, and to generate reports with information about them.

The tools included are:
- `processfolders` - collects data about all files and folders under the specified directory into a database
- `findduplicates` - checks if any of the files in the database are duplicates
- `generatereports` - generate general reports about the files and folders in the database
- `generateuserreports` - generate user-specific reports about the files and folders in the database

How it works
------------
The `processfolders` application crawls trough a directory tree and puts information about all files and folders in a SQLite3 database.
The following SQL tables are are created and populated during this process:
- folder:
    | field                | type    | description                                                     |
    | -------------------- | ------- | --------------------------------------------------------------- |
    | id                   | INT     | unique key (primary key)                                        |
    | parent               | INT     | id of parent folder (or NULL if top level)                      |
    | path                 | VARCHAR | full path                                                       |
    | owner                | VARCHAR | owner's username                                                |
    | filecount            | INT     | total number of files in this folder alone                      |
    | foldercount          | INT     | total number of folders in this folder alone                    |
    | totalsize            | INT     | total size in bytes of all files in this folder alone           |
    | filecountrecursive   | INT     | total number of files in this folder and all deeper levels      |
    | foldercountrecursive | INT     | total number of folders in this folder and all deeper levels    |
    | totalsizerecursive   | INT     | total size in bytes of all in this folder and all deeper levels |
    ")");
- file:
    | field      | type    | description              |
    | ---------- | ------- | ------------------------ |
    | id         | INT     | unique key (primary key) |
    | folder     | INT     | id of parent folder      |
    | name       | VARCHAR | filename                 |
    | extension  | VARCHAR | extension                |
    | size       | INT     | size in bytes            |
    | created    | INT     | creation date/time       |
    | lastaccess | INT     | last access date/time    |
    | owner      | VARCHAR | owner's username         |
    | crc32      | INT     | CRC32 checksum           |
    | sha1       | VARCHAR | SHA1 checksum            |
    | adler32    | INT     | Adler32 checksum         |

Dependancies
------------
This project has the following depencancies:
- [mhash](http://mhash.sourceforge.net/)
- [SQLite 3](http://www.sqlite.org/)
- [libdirtrav](https://github.com/brechtsanders/libdirtrav/)
- [XLSX I/O](http://brechtsanders.github.io/xlsxio/)

License
-------
FolderSnitch is released under the terms of the GPL-3.0, see LICENSE.txt.
