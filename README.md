FolderSnitch
============
Cross-platform command-line tools to get reports on file and folder usage, including duplicate files.

Description
-----------
FolderSnitch consists of a series of tools to collect data about folder structures and the files in them, and to generate reports with information about them.

These tools are:
- processfolders: collects data about all files and folders under the specified directory into a database
- findduplicates: finds duplicate files in database
- generatereports: generate general reports about the files and folders in database
- generateuserreports: generate user-specific reports about the files and folders in database

Dependancies
------------
This project has the following depencancies:
- [mhash](http://mhash.sourceforge.net/)
- [libdirtrav](https://github.com/brechtsanders/libdirtrav/)
- [XLSX I/O](http://brechtsanders.github.io/xlsxio/)

License
-------
FolderSnitch is released under the terms of the GPL-3.0, see LICENSE.txt.
