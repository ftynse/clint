project(clint)
cmake_minimum_required(VERSION 3.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -pedantic -O3 -Wno-sign-compare")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -pedantic -O0 -g")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  message(STATUS "Using clang compiler")
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wdocumentation") # Boost doxygen generates too many errors.
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  message(STATUS "Using GNU compiler")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra")
else()
  message(STATUS "Using ${CMAKE_CXX_COMPILER_ID} compiler.  No predefined flags set")
endif()

# Periscop projects
find_package(osl REQUIRED)
if(NOT DEFINED OSL_FOUND)
  message(FATAL_ERROR "osl library not found")
else()
  message(STATUS "Using osl library from: ${OSL_LIBRARY}")
endif()

find_package(cloog-isl REQUIRED)
if(NOT DEFINED CLOOG_ISL_FOUND)
  message(FATAL_ERROR "cloog-isl library not found")
else()
  message(STATUS "Using osl library from: ${CLOOG_ISL_LIBRARY}")
endif()

find_package(candl REQUIRED)
if(NOT DEFINED CANDL_FOUND)
  message(FATAL_ERROR "candl library not found")
else()
  message(STATUS "Using candl library form: ${CANDL_LIBRARY}")
endif()

find_package(clan REQUIRED)
if(NOT DEFINED CLAN_FOUND)
  message(FATAL_ERROR "clan library not found")
else()
  message(STATUS "Using clan library from: ${CLAN_LIBRARY}")
endif()

find_package(clay REQUIRED)
if(NOT DEFINED CLAY_FOUND)
  message(FATAL_ERROR "clay library not found")
else()
  message(STATUS "Using clay library from: ${CLAY_LIBRARY}")
endif()

find_package(piplibmp REQUIRED)
if(NOT DEFINED PIPLIBMP_FOUND)
  message(FATAL_ERROR "piplib_MP not found")
else()
  message(STATUS "Using piplib library with multiprecision from: ${PIPLIBMP_LIBRARY}")
endif()

find_package(chlore REQUIRED)
if(NOT DEFINED CHLORE_FOUND)
  message(FATAL_ERROR "chlore library not found")
else()
  message(STATUS "Using chlore library from: ${CHLORE_LIBRARY}")
endif()

find_library(GMP_LIBRARY gmp)
find_path(GMP_INCLUDE_DIR gmp.h)

find_library(ISL_LIBRARY isl)
find_path(ISL_INCLUDE_DIR isl/ctx.h)

if(ISL_INCLUDE_DIR)
  message(STATUS "Using isl library from: ${ISL_LIBRARY}")
else()
  message(FATAL_ERROR "isl library not found")
endif()

include_directories(${OSL_INCLUDE_DIR})
include_directories(${CLOOG_ISL_INCLUDE_DIR})
include_directories(${CANDL_INCLUDE_DIR})
include_directories(${CLAN_INCLUDE_DIR})
include_directories(${CLAY_INCLUDE_DIR})
include_directories(${GMP_INCLUDE_DIR})
include_directories(${ISL_INCLUDE_DIR})
include_directories(${PIPLIBMP_INCLUDE_DIR})
include_directories(${CHLORE_INCLUDE_DIR})

# Qt libraries
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
include_directories(${Qt5Core_INCLUDE_DIRS})

aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST} macros.h)

qt5_use_modules(${PROJECT_NAME} Widgets Gui Core Xml Svg)

# Boost libraries
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.55.0 REQUIRED)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
else()
  message(FATAL_ERROR "boost library not found")
endif()

target_link_libraries(${PROJECT_NAME} ${OSL_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${ISL_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${CLOOG_ISL_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${CLAN_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${CANDL_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${CLAY_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${PIPLIBMP_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${CHLORE_LIBRARY})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake-uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake-uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake-uninstall.cmake)
