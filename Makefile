# C++ compiler to use
CXX = g++

# Compiler flags: C++17 standard, warnings, and include folder path
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude

# All C++ source files in the project (Consolidated)
SRCS = src/main.cpp \
       src/cli_parser.cpp \
       src/transfer_manager.cpp \
       src/sender_engine.cpp \
       src/receiver_engine.cpp \
       src/discovery_service.cpp \
       src/folder_scanner.cpp \
       src/sha256_hasher.cpp \
       src/ui_utils.cpp

# Target executable name
TARGET = warp.exe

# Libraries to link: ws2_32 is required for Windows Winsock Sockets
LIBS = -lws2_32

# Default target: builds the executable directly from sources (no intermediate .o files)
all: $(TARGET)

$(TARGET):
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

# Clean target: deletes the executable
clean:
	del /F /Q $(TARGET) 2>nul || exit 0
