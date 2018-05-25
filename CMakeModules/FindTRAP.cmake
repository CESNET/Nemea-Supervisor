#  TRAP_FOUND - System has libtrap
#  TRAP_INCLUDE_DIRS - The libtrap include directories
#  TRAP_LIBRARIES - The libraries needed to use libtrap
#  TRAP_DEFINITIONS - Compiler switches required for using CMocka


find_path(TRAP_INCLUDE_DIR trap.h
        HINTS ${PC_TRAP_INCLUDEDIR} ${PC_TRAP_INCLUDE_DIRS}
        PATH_SUFFIXES libtrap)

find_library(TRAP_LIBRARY NAMES trap
        HINTS ${PC_TRAP_LIBDIR} ${PC_TRAP_LIBRARY_DIRS})

set(TRAP_LIBRARIES ${TRAP_LIBRARY})
set(TRAP_INCLUDE_DIRS ${TRAP_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set TRAP_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(trap  DEFAULT_MSG
        TRAP_LIBRARY TRAP_INCLUDE_DIR)

mark_as_advanced(TRAP_INCLUDE_DIR TRAP_LIBRARY)