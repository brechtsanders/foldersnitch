FolderSnitch
============
Cross-platform command-line tools to get reports on file and folder usage, including duplicate files.

Description
-----------
FolderSnitch consists of a series of tools to collect data about folder structures and the files in them, and to generate reports with information about them.

The tools included are:
- processfolders: collects data about all files and folders under the specified directory into a database
- findduplicates: checks if any of the files in the database are duplicates
- generatereports: generate general reports about the files and folders in the database
- generateuserreports: generate user-specific reports about the files and folders in the database

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
