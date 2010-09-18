/* config-amarok.h. Generated by cmake from config-amarok.h.cmake */

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT ${SIZEOF_INT}

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG ${SIZEOF_LONG}

/* have LIBVISUAL */
#cmakedefine HAVE_LIBVISUAL 1

/* have Qt with OpenGL support */
/* #undef HAVE_QGLWIDGET */

/* Define to 1 if you have the `statvfs' function. */
#define HAVE_STATVFS 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <systems.h> header file. */
/* #undef HAVE_SYSTEMS_H */

/* have TagLib */
#cmakedefine HAVE_TAGLIB 1

/* MySql database support enabled */
#cmakedefine USE_MYSQL 1

/* Whether taglib exposes filenames as TagLib::FileName type */
#cmakedefine HAVE_TAGLIB_FILENAME 1

/* Whether TagLib::FileName is a struct supporting wide characters or just a typedef */
#cmakedefine COMPLEX_TAGLIB_FILENAME 1

/* The Git version being compiled, if any. undef means not running from Git. */
#cmakedefine CURRENT_GIT_VERSION ${CURRENT_GIT_VERSION}

/* If liblastfm is found */
#cmakedefine HAVE_LIBLASTFM 1

/* If we can use KStatusNotifierItem class (KDE 4.4) */
#cmakedefine HAVE_KSTATUSNOTIFIERITEM 1

/* Whether cmake build type is debug */
#cmakedefine DEBUG_BUILD_TYPE
