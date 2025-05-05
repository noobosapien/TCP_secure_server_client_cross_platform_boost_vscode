
#Windows
CC := g++
TARGET := secure_client
SRC := secure_client.cpp



# Detect the operating system
ifeq ($(OS),Windows_NT)
	# Windows
# 	TARGET := $(TARGET)
	CFLAGS := -c -std=c++14 -Wall -O2 -fconserve-space $(SRC) -I"C:/boost_1_87_0"
    LFLAGS := -lws2_32 -L"C:/boost_1_87_0/libs" -I"C:/boost_1_87_0" -static-libgcc -static-libstdc++
    EXTENSION = .exe
	CLEANUP := del
	CLEANUP_OBJS := del *.o
	CLEANUP_OBJS := del *.exe
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		# macOS
# 		TARGET := $(TARGET).out
		EXTENSION = .out
		CFLAGS := -c -std=c++14 -Wall -I /usr/local/include
		LFLAGS := -L /usr/local/lib -lboost_system -lboost_filesystem
		CLEANUP := rm -f
		CLEANUP_OBJS := rm -f *.o
	else ifeq ($(UNAME_S),Linux)
		# Linux
# 		TARGET := $(TARGET).out
		EXTENSION = .out
		CFLAGS := -c -std=c++14 -Wall -I /usr/local/include
		LFLAGS := -L /usr/local/lib -lboost_system -lboost_filesystem
		CLEANUP := rm -f
		CLEANUP_OBJS := rm -f *.o
	endif
endif




$(TARGET)$(EXTENSION)	:  $(TARGET).o 
	$(CC)  $(SRC) $(LFLAGS) -o $(TARGET)$(EXTENSION)
			
$(TARGET).o	 : 	$(SRC) 
	$(CC) $(CFLAGS) $(SRC) 

clean:
	$(CLEANUP) $(TARGET)
	$(CLEANUP_OBJS)
