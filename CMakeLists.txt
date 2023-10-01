cmake_minimum_required(VERSION 3.10)
project(wtsclwq_server)

include(cmake/utils.cmake)
set(CMAKE_VERBOSE_MAKEFILE ON)

# 编译器选项
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++17 -O0 -ggdb -Wall -Werror")

# -rdynamic: 将所有符号都加入到符号表中，便于使用dlopen或者backtrace追踪到符号
# -fPIC: 生成位置无关的代码，便于动态链接
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# Wno-unused-function: 忽略未使用的函数
# Wno-builtin-macro-redefined: 忽略宏重定义
# Wno-deprecated-declarations: 忽略带有[[deprecated]]的函数
# Wno-deprecated: 忽略过时警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations -Wno-deprecated")

include_directories(.)

option(BUILD_TEST "ON for complie test" ON)

find_package(Boost REQUIRED)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif()

set(LIB_SRC 
    server/log.cpp 
    server/utils.cpp 
    server/env.cpp 
    server/config.cpp 
    server/thread.cpp 
    server/lock.cpp
    )

add_library(server SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(server)

set(LIBS server ${Boost_LIBRARIES} pthread yaml-cpp dl)

if (BUILD_TEST)
wtsclwq_add_executable(test_env "test/test_env.cpp" server "${LIBS}")
wtsclwq_add_executable(test_config "test/test_config.cpp" server "${LIBS}")
wtsclwq_add_executable(test_thread "test/test_thread.cpp" server "${LIBS}")
wtsclwq_add_executable(test_lock "test/test_lock.cpp" server "${LIBS}")
wtsclwq_add_executable(test_log "test/test_log.cpp" server "${LIBS}")
wtsclwq_add_executable(test_utils "test/test_utils.cpp" server "${LIBS}")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)