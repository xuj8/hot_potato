# Compiler settings - Can be customized.
CC = g++
CFLAGS = -std=c++11 -Wall -lpthread

COMMON_HEADERS = client.h server.h commands.h common_defs.h

# Your final executables should be named here
all: ringmaster player

# Main programs
ringmaster: ringmaster_controller.o
	$(CC) $(CFLAGS) ringmaster_controller.o -o ringmaster

player: player_controller.o
	$(CC) $(CFLAGS) player_controller.o -o player

# client_test: client_controller.o
# 	$(CC) $(CFLAGS) client_controller.o -o client_test

# server_test: server_controller.o
# 	$(CC) $(CFLAGS) server_controller.o -o server_test

# Object files with dependencies on common headers
ringmaster_controller.o: ringmaster_controller.cpp ringmaster.h $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c ringmaster_controller.cpp -o ringmaster_controller.o

player_controller.o: player_controller.cpp player.h $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c player_controller.cpp -o player_controller.o

# client_controller.o: client_controller.cpp client.h $(COMMON_HEADERS)
# 	$(CC) $(CFLAGS) -c client_controller.cpp -o client_controller.o

# server_controller.o: server_controller.cpp server.h $(COMMON_HEADERS)
# 	$(CC) $(CFLAGS) -c server_controller.cpp -o server_controller.o

# Clean up
clean:
	rm -f *.o ringmaster player