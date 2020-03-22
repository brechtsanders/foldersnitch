/**
 * @file folderreports.h
 * @brief folderreports library header file with version information
 * @author Brecht Sanders
 *
 * This header file defines the common version information
 */

#ifndef INCLUDED_FOLDERREPORTSVERSION_H
#define INCLUDED_FOLDERREPORTSVERSION_H

/*! \brief version number constants
 * \sa     folderreports_get_version()
 * \sa     folderreports_get_version_string()
 * \name   FOLDERREPORTS_VERSION_*
 * \{
 */
/*! \brief major version number */
#define FOLDERREPORTS_VERSION_MAJOR 0
/*! \brief minor version number */
#define FOLDERREPORTS_VERSION_MINOR 1
/*! \brief micro version number */
#define FOLDERREPORTS_VERSION_MICRO 0
/*! @} */

/*! \brief packed version number */
#define FOLDERREPORTS_VERSION (FOLDERREPORTS_VERSION_MAJOR * 0x01000000 + FOLDERREPORTS_VERSION_MINOR * 0x00010000 + FOLDERREPORTS_VERSION_MICRO * 0x00000100)

/*! \cond PRIVATE */
#define FOLDERREPORTS_VERSION_STRINGIZE_(major, minor, micro) #major"."#minor"."#micro
#define FOLDERREPORTS_VERSION_STRINGIZE(major, minor, micro) FOLDERREPORTS_VERSION_STRINGIZE_(major, minor, micro)
/*! \endcond */

/*! \brief string with dotted version number \hideinitializer */
#define FOLDERREPORTS_VERSION_STRING FOLDERREPORTS_VERSION_STRINGIZE(FOLDERREPORTS_VERSION_MAJOR, FOLDERREPORTS_VERSION_MINOR, FOLDERREPORTS_VERSION_MICRO)

#endif
