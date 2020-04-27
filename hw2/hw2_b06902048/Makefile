CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`

CLIENT = client.cpp
SERVER = server.cpp
CLI = client
SER = server

all: server client

clean:
	@echo "cleaning up......"
	rm -rf server
	rm -rf client
	rm -rf b06902048_server_folder
	rm -rf b06902048_client_folder

server: $(SERVER)
	@echo "generating server..."
	$(CC) $(SERVER) -o $(SER) $(OPENCV)

client: $(CLIENT)
	@echo "generating client..."
	$(CC) $(CLIENT) -o $(CLI) $(OPENCV)
