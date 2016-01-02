# - Try to find Sqlite3
# Once done this will define
#  Sqlite3_FOUND - System has LibXml2
#  Sqlite3_INCLUDE_DIRS - The LibXml2 include directories
#  Sqlite3_LIBRARIES - The libraries needed to use LibXml2
#  Sqlite3_DEFINITIONS - Compiler switches required for using LibXml2

find_package(PkgConfig)
pkg_check_modules(PC_Sqlite3 QUIET sqlite3)
set(Sqlite3_DEFINITIONS ${PC_Sqlite3_CFLAGS_OTHER})

find_path(Sqlite3_INCLUDE_DIR sqlite3.h
          HINTS ${PC_Sqlite3_INCLUDEDIR} ${PC_Sqlite3_INCLUDE_DIRS}
		  PATH_SUFFIXES sqlite3)

find_library(Sqlite3_LIBRARY sqlite3
             HINTS ${PC_Sqlite3_LIBDIR} ${PC_Sqlite3_LIBRARY_DIRS} )

set(Sqlite3_LIBRARIES ${Sqlite3_LIBRARY} )
set(Sqlite3_INCLUDE_DIRS ${Sqlite3_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set Sqlite3_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Sqlite3  DEFAULT_MSG
                                  Sqlite3_LIBRARY Sqlite3_INCLUDE_DIR)

mark_as_advanced(Sqlite3_INCLUDE_DIR Sqlite3_LIBRARY )

