cmake_minimum_required(VERSION 3.20)
project(RealChat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ASIO headers
include_directories("C:/Users/rjeev/Documents/library/asio-1.30.2/include")

# MySQL headers
set(MYSQL_ROOT "C:/Users/rjeev/Documents/c++/SQLC++/vcpkg/mysql-mingw/mysql-mingw64-port-master")
include_directories("${MYSQL_ROOT}/mysql-connector-c-6.1.5-src/include")
include_directories("${MYSQL_ROOT}/mysql-connector-c-6.1.5-src/build32/include")

# Define macros for Windows and ASIO
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DASIO_STANDALONE)
add_definitions(-DNOMINMAX)

# Compiler flags for better error handling and optimization
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")

# Executables
add_executable(server server.cpp)
add_executable(client client.cpp)

# Link libraries for server (with MySQL)
target_link_libraries(server
    "${MYSQL_ROOT}/lib/x64/libmysqlclient.a"
    ws2_32
    mswsock
    secur32
    crypt32
    wldap32
)

# Link libraries for client (no MySQL needed)
target_link_libraries(client
    ws2_32
    mswsock
)

# Copy MySQL DLL to build directory after build
add_custom_command(TARGET server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${MYSQL_ROOT}/lib/x64/liblibmysql.dll"
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Print build information
message(STATUS "RealChat project configured successfully!")
message(STATUS "ASIO path: C:/Users/rjeev/Documents/library/asio-1.30.2/include")
message(STATUS "MySQL path: ${MYSQL_ROOT}")
