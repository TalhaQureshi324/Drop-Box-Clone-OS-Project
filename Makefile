# ===== Makefile for Fedora OS Mini Project =====

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
SRC_DIR = src
CLIENT_DIR = client

SERVER_OBJS = $(SRC_DIR)/server.o $(SRC_DIR)/queues.o $(SRC_DIR)/client_thread.o $(SRC_DIR)/task_queue.o $(SRC_DIR)/worker_thread.o $(SRC_DIR)/auth.o $(SRC_DIR)/locks.o
CLIENT_OBJS = $(CLIENT_DIR)/client.o

all: server client

# ---- Compile object files ----
$(SRC_DIR)/server.o: $(SRC_DIR)/server.c $(SRC_DIR)/server.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/server.c -o $(SRC_DIR)/server.o

$(SRC_DIR)/queues.o: $(SRC_DIR)/queues.c $(SRC_DIR)/server.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/queues.c -o $(SRC_DIR)/queues.o

$(SRC_DIR)/client_thread.o: $(SRC_DIR)/client_thread.c $(SRC_DIR)/server.h $(SRC_DIR)/auth.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/client_thread.c -o $(SRC_DIR)/client_thread.o

$(SRC_DIR)/task_queue.o: $(SRC_DIR)/task_queue.c $(SRC_DIR)/server.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/task_queue.c -o $(SRC_DIR)/task_queue.o

$(SRC_DIR)/worker_thread.o: $(SRC_DIR)/worker_thread.c $(SRC_DIR)/server.h $(SRC_DIR)/locks.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/worker_thread.c -o $(SRC_DIR)/worker_thread.o

$(SRC_DIR)/auth.o: $(SRC_DIR)/auth.c $(SRC_DIR)/auth.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/auth.c -o $(SRC_DIR)/auth.o

$(SRC_DIR)/locks.o: $(SRC_DIR)/locks.c $(SRC_DIR)/locks.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/locks.c -o $(SRC_DIR)/locks.o

$(CLIENT_DIR)/client.o: $(CLIENT_DIR)/client.c
	$(CC) $(CFLAGS) -c $(CLIENT_DIR)/client.c -o $(CLIENT_DIR)/client.o

# ---- Build executables ----
server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client_app $(CLIENT_OBJS)

# ---- Clean ----
clean:
	rm -f $(SRC_DIR)/*.o $(CLIENT_DIR)/*.o server client_app
