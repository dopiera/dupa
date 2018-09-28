# (C) Copyright 2018 Marek Dopiera
#
# This file is part of dupa.
#
# dupa is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dupa is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with dupa. If not, see http://www.gnu.org/licenses/.

# - Try to find Sqlite3
# Once done this will define
#  Sqlite3_FOUND - System has Sqlite3
#  Sqlite3_INCLUDE_DIRS - The Sqlite3 include directories
#  Sqlite3_LIBRARIES - The libraries needed to use Sqlite3
#  Sqlite3_DEFINITIONS - Compiler switches required for using Sqlite3

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

