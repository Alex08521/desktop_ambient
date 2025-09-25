# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/desktop_ambient_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/desktop_ambient_autogen.dir/ParseCache.txt"
  "desktop_ambient_autogen"
  )
endif()
