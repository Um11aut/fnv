# CMakeList.txt : CMake project for fnv, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("fnv")

find_package(spdlog CONFIG REQUIRED)
find_package(ffmpeg REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable (fnv ${SOURCES})

include_directories(include)

target_link_libraries(fnv PRIVATE spdlog::spdlog)

target_include_directories(fnv PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(fnv PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(fnv PRIVATE ${FFMPEG_LIBRARIES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET fnv PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
