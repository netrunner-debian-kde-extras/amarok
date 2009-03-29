# - Try to find the MP4V2 library
# Once done this will define
#
#  MP4V2_FOUND - system has the MP4V2 library
#  MP4V2_INCLUDE_DIR - the MP4V2 include directory
#  MP4V2_LIBRARIES - The libraries needed to use MP4V2

if (MP4V2_INCLUDE_DIR AND MP4V2_LIBRARIES)
  # Already in cache, be silent
  set(Mp4v2_FIND_QUIETLY TRUE)
endif (MP4V2_INCLUDE_DIR AND MP4V2_LIBRARIES)

FIND_PATH(MP4V2_INCLUDE_DIR mp4.h
)

FIND_LIBRARY(MP4V2_LIBRARY NAMES mp4v2
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MP4V2 DEFAULT_MSG MP4V2_LIBRARY MP4V2_INCLUDE_DIR)

MARK_AS_ADVANCED(MP4V2_INCLUDE_DIR MP4V2_LIBRARY)
