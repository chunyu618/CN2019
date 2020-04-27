clean:
	@echo "cleaning up......"
	rm -rf server
	rm -rf client
	rm -rf b06902048_server_folder
	rm -rf b06902048_client_folder

server: server.cpp
	@echo "generating server..."
	g++ server.cpp -o server `pkg-config --cflags --libs opencv`

client: client.cpp
	@echo "generating client..."
	g++ client.cpp -o client `pkg-config --cflags --libs opencv`
