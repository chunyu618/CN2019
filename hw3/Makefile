CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`

CLIENT = receiver.cpp
SERVER = sender.cpp
AGENT = agent.c
CLI = receiver
SER = sender
AGE = agent

all: server client agent

clean:
	@echo "cleaning up......"
	rm -rf sender
	rm -rf receiver
	rm -rf agent

server: $(SERVER)
	@echo "generating server......"
	$(CC) $(SERVER) -o $(SER) $(OPENCV)

client: $(CLIENT)
	@echo "generating client......"
	$(CC) $(CLIENT) -o $(CLI) $(OPENCV)

agent: $(AGENT)
	@echo "generating agnet......"
	gcc $(AGENT) -o $(AGE)
