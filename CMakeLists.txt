CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

# Set the project name
PROJECT(venus-sma-net)

# Set the output folder where your program will be created
set(CMAKE_BINARY_DIR .)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Source files
set(VENUS_SMA_NET_SRC
	src/ve_dbus.c
    src/main.c
)

set(CMAKE_FIND_ROOT_PATH  /opt/venus/scarthgap-arm-cortexa8hf-neon/sysroots/cortexa8hf-neon-ve-linux-gnueabi)

# Include directories
include_directories(
	sma/include
	sma/core
	sma/smalib
	sma/os
	sma/protocol
	sma/master
	sma/libs
	sma/projects/generic-cmake/incprj
	/opt/venus/scarthgap-arm-cortexa8hf-neon/sysroots/cortexa8hf-neon-ve-linux-gnueabi/usr/include/dbus-1.0
	/opt/venus/scarthgap-arm-cortexa8hf-neon/sysroots/cortexa8hf-neon-ve-linux-gnueabi/usr/lib/dbus-1.0/include
	# /usr/lib/x86_64-linux-gnu/dbus-1.0/include
)

link_directories(
	../sma/projects/generic-cmake/build-gcc
)

add_subdirectory(sma/projects/generic-cmake .)

if(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x -fmessage-length=0 -fstack-protector -g")
	set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -fmessage-length=0 -g -fstack-protector")
endif()

# Build the application
add_executable(venus-sma-net ${VENUS_SMA_NET_SRC})
TARGET_LINK_LIBRARIES(venus-sma-net dl pthread yasdi yasdimaster dbus-1)

# add the install targets
install (TARGETS venus-sma-net DESTINATION /usr/local/bin)

