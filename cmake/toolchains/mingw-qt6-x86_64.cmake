set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH
    /usr/x86_64-w64-mingw32
    /root/qtwin/6.2.4/mingw_64
    /root/co/win-deps/openssl-win
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(QT_HOST_PATH /usr)
set(QT_HOST_PATH_CMAKE_DIR /usr/lib/x86_64-linux-gnu/cmake)
set(CMAKE_PREFIX_PATH /root/qtwin/6.2.4/mingw_64)
set(Qt6_DIR /root/qtwin/6.2.4/mingw_64/lib/cmake/Qt6)

set(OPENSSL_ROOT_DIR /root/co/win-deps/openssl-win)
set(OPENSSL_USE_STATIC_LIBS ON)
