# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.6)

# Needed for compilation on ancient RPI
set(CMAKE_C_STANDARD 11)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("MrdsControl")

# Find pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add source to this project's executable.
add_executable (MrdsControl   "rds.c" "rds.h"  "web.c" "web_handlers.c" "web_handlers.h" "web.h" "main.c" "web_client.h" "web_client.c")

# Link
target_link_libraries(MrdsControl PRIVATE Threads::Threads)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET MrdsControl PROPERTY CXX_STANDARD 20)
endif()
