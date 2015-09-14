#/bin/sh

# (C) Copyright 2010 Marek Dopiera
#
# This file is part of CoherentDB.
# 
# CoherentDB is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# CoherentDB is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public
# License along with CoherentDB. If not, see
# http://www.gnu.org/licenses/.

PROJ_DIR="`dirname \"$0\"`/.."
cd "${PROJ_DIR}"
rm -f cscope.files cscope.out
find "`pwd`" -type f \( -name \*.cpp -o -name \*.h \) > cscope.files
cscope -k -i cscope.files -f cscope.out -b
