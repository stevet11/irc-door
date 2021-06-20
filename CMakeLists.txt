cmake_minimum_required(VERSION 3.0)
project(ircdoor
  VERSION 0.1
  LANGUAGES CXX C)

###########
# Debug or Release
###########
if (NOT CMAKE_BUILD_TYPE)
  ## set default to Debug
  set(CMAKE_BUILD_TYPE RelWithDebInfo)  # override with -DCMAKE_BUILD_TYPE=Release
  message("==> CMAKE_BUILD_TYPE empty. Changing it to Debug.")
else()
  message("==> CMAKE_BUILD_TYPE == ${CMAKE_BUILD_TYPE}.")
endif()

## https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
## During Debug, use debug version of libstdc++ (asserts on access to invalid iterators, etc!)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

##############
# C++ Standard
##############
set(CMAKE_CXX_STANDARD   14)
# set(CMAKE_CXX_STANDARD   17)
set(CMAKE_CXX_EXTENSIONS ON)

set(THREADS_PREFER_PTHEAD_FLAG ON)

set(BOOST_THREAD_DYN_LINK ON) # I don't think this is seen
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS system thread REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(LINK_LIBS ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES})


if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/door++)
  message("***")
  message("*** ERROR/MISSING *** please run: git clone https://github.com/stevet11/door.git door++")
  message("*** (Or whatever git clone you need for door++)")
  message("***")
endif()

add_subdirectory(door++)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp)
  message("***")
  message("*** ERROR/MISSING *** please run: git clone https://github.com/jbeder/yaml-cpp.git --depth 1")
  message("***")
endif()

add_subdirectory(yaml-cpp)

add_executable(irc-door main.cpp irc.h irc.cpp)
target_link_libraries(irc-door door++ pthread ${LINK_LIBS} dl yaml-cpp)

