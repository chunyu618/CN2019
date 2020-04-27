#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<sys/select.h>
#include<sys/stat.h>
#include<dirent.h>
#include<signal.h>
#include "opencv2/opencv.hpp"
#include<iostream>

using namespace std;
using namespace cv;

#define ERR_EXIT(a) { perror(a); exit(1); } 
#define BUF_SIZE 4096

typedef struct {
	char hostname[128];
	unsigned short port;
	int listen_fd;
} server_info;

typedef struct{
	int conn_fd;
	char command[32];
	char filename[128];
	int left;
	int f;
	int width, height;
} request;

void handle_sigpipe(int sig){
	printf("catch SIGPIPE!!\n");
	return;
}

server_info svr;
request* Crequest = NULL;
fd_set master_read_set, master_write_set, working_set;
VideoCapture cap[1024];
Mat img[1024];

static void init_server(unsigned short port);
static void init_request(request* R);
static int read_client(request* R);
static int write_client(request* R);
int my_max(int x, int y);
void keep_connect(request* R);
int disconnect = 0;
static int put_function(request* R);

char folder_name[128] = "b06902048_server_folder";


int main(int argc, char *argv[]){
	mkdir(folder_name, 0777); //mkdir
	struct sockaddr_in client_addr; //used by accept()
	VideoCapture cap;
	//deal with SIGPIPE
	struct sigaction new_act_PIPE, old_act_PIPE;
	new_act_PIPE.sa_handler = handle_sigpipe;
	sigemptyset(&new_act_PIPE.sa_mask);
	sigaddset(&new_act_PIPE.sa_mask, SIGPIPE);
	new_act_PIPE.sa_flags = 0;
	sigaction(SIGPIPE, &new_act_PIPE, &old_act_PIPE);

	int conn_fd = 0; //fd for a new client
	int client_len;

	//set timeout foe select()
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;

	FD_ZERO(&master_read_set);
	FD_ZERO(&master_write_set);
	FD_ZERO(&working_set);

	//Parse args
	if(argc != 2){
		fprintf(stderr, "usage: %s [port]\n", argv[0]);
		exit(1);
	}

	//Initialize server
	init_server((unsigned short) atoi(argv[1]));
	
	int maxfd = getdtablesize(), nowmaxfd = 0;
	if(svr.listen_fd > nowmaxfd){
		nowmaxfd = svr.listen_fd;
	}

	Crequest = (request*)malloc(sizeof(request) * maxfd);
	if(Crequest == NULL){
		ERR_EXIT("out of memory");
	}
	for(int i = 0; i < maxfd; i++){
		init_request(&Crequest[i]);
	}
	Crequest[svr.listen_fd].conn_fd = svr.listen_fd;
	fprintf(stderr, "starting on %.80s, port %d, fd %d, max connection %d......\n", svr.hostname, atoi(argv[1]), svr.listen_fd, maxfd);
	FD_SET(svr.listen_fd, &master_read_set);
	while(1){
		//deal with client
		FD_CLR(svr.listen_fd, &master_read_set);
		memcpy(&working_set, &master_read_set, sizeof(master_read_set));
		select(nowmaxfd + 1, &working_set, NULL, NULL, &timeout);
		for(int i = svr.listen_fd + 1; i <= nowmaxfd; i++){
			if(FD_ISSET(i, &working_set)){	
				int retv = -1;
				if(strlen(Crequest[i].command) == 0){
					retv = read_client(&Crequest[i]);
				}
				else if(strcmp(Crequest[i].command, "put") == 0){
					retv = put_function(&Crequest[i]);
				}
				
				if(retv < 0){
					if(disconnect == 1){
						if(strcmp(Crequest[i].command, "put") == 0){
							char temp_filename[BUF_SIZE];
							sprintf(temp_filename, "./%s/%s", folder_name, Crequest[i].filename);
							remove(temp_filename);
						}
						printf("disconnect from fd %d\n", Crequest[i].conn_fd);
						FD_CLR(Crequest[i].conn_fd, &master_read_set);
						close(Crequest[i].conn_fd);
						init_request(&Crequest[i]);
						disconnect = 0;
					}
					continue;		
				}
			}
		}

		//deal with write
		memcpy(&working_set, &master_write_set, sizeof(master_write_set));
		select(nowmaxfd + 1, NULL, &working_set, NULL, &timeout);
		for(int i = svr.listen_fd + 1; i <= nowmaxfd; i++){
			if(FD_ISSET(i, &working_set)){
				int retv = write_client(&Crequest[i]);
				//printf("after return\n");
				if(retv < 0){
					if(disconnect == 1){
						printf("disconnect from fd %d\n", Crequest[i].conn_fd);
						FD_CLR(Crequest[i].conn_fd, &master_read_set);
						FD_CLR(Crequest[i].conn_fd, &master_write_set);
						close(Crequest[i].conn_fd);
						init_request(&Crequest[i]);
						disconnect = 0;
					}
					continue;		
				}
			}
		}

		//deal with request
		//printf("to deal with request\n");
		FD_SET(svr.listen_fd, &master_read_set);
		memcpy(&working_set, &master_read_set, sizeof(master_read_set));
		select(svr.listen_fd + 1, &working_set, NULL, NULL, &timeout);
		if(FD_ISSET(svr.listen_fd, &working_set)){
			client_len = sizeof(client_addr);
			conn_fd = accept(svr.listen_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);			
			if(conn_fd < 0){
				ERR_EXIT("connection");
			}
			if(conn_fd > nowmaxfd){
				nowmaxfd = conn_fd;
			}
			init_request(&Crequest[conn_fd]);
			Crequest[conn_fd].conn_fd = conn_fd;
			FD_SET(conn_fd, &master_read_set);
			fprintf(stderr, "accpet connection on fd %d\n", Crequest[conn_fd].conn_fd);
		}
		//printf("after deal with request\n");
	}
	free(Crequest);
	return 0;
}



static void init_server(unsigned short port){
	struct sockaddr_in server_addr;
	
	gethostname(svr.hostname, sizeof(svr.hostname));
	svr.port = port;
	svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	bzero(&server_addr, sizeof(server_addr));// erase the data
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	if(svr.listen_fd < 0){
		ERR_EXIT("socket");
	}
	if(bind(svr.listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		ERR_EXIT("bind");
	}
	if(listen(svr.listen_fd, 1024) < 0){
		ERR_EXIT("listen");
	}
	return;
}

static void init_request(request* R){
	R->conn_fd = -1;
	R->left = 0;
	R->f = -1;
	memset(R->command, 0, sizeof(R->command));
	memset(R->filename, 0, sizeof(R->filename));
	//memset(&(R->cap), 0, sizeof(R->cap));
	R->width = 0;
	R->height = 0;
}

static int write_client(request* R){
	if(strcmp(R->command, "get") == 0){
		char get_buf[BUF_SIZE] = {};
		int r = 0, w = 0;
		r = read(R->f, get_buf, sizeof(get_buf) - 1);
		//printf("r = %d\n", r);
		if(r == 0){
			FD_CLR(R->conn_fd, &master_write_set);
			close(R->f);
			keep_connect(R);
			return 1;
		}
		w = write(R->conn_fd, get_buf, r);
		if(w == -1){
			close(R->f);
			disconnect = 1;
			return -1;
		}
		return 1;	
	}
	else if(strcmp(R->command, "play") == 0){
		cap[R->conn_fd] >> img[R->conn_fd];
		int imgSize = img[R->conn_fd].total() * img[R->conn_fd].elemSize();
		int w = write(R->conn_fd, &imgSize, sizeof(int));
		if(w == -1){
			cap[R->conn_fd].release();
			disconnect = 1;
			return -1;
		}
		uchar write_buf[imgSize] = {};
		memcpy(write_buf, img[R->conn_fd].data, imgSize);
		w = write(R->conn_fd, write_buf, imgSize);
		if(w == -1){
			cap[R->conn_fd].release();
			disconnect = 1;
			return -1;
		}
		else if(w == 0){
			FD_CLR(R->conn_fd, &master_write_set);
			cap[R->conn_fd].release();
			keep_connect(R);
			return 1;
		}
	}
}

static int put_function(request* R){
	char buf[BUF_SIZE] = {};
	int r = read(R->conn_fd, buf, sizeof(buf) - 1);
	if(r <= 0){
		disconnect = 1;
		if(R->f != -1){
			close(R->f);
		}
		return -1;
	}
	int w = write(R->f, buf, r);
	R->left -= w;
	//printf("r = %d w = %d R->left = %d\n", r, w, R->left);
	if(R->left == 0){
		close(R->f);
		keep_connect(R);
		w = 0;
		int succ = 1;
		w = write(R->conn_fd, &succ, sizeof(int));
		if(w == -1){
			close(R->f);
			disconnect = 1;
			return -1;
		}
	}
	return 1;
}


static int read_client(request* R){
	int r;
	char buf[BUF_SIZE];
	memset(buf, 0, sizeof(buf));
	char *data = buf;
	int offset = 0;
	r = recv(R->conn_fd, buf, sizeof(buf) - 1, MSG_WAITALL);
	if(r <= 0){
		disconnect = 1;
		if(R->f != -1){
			close(R->f);
		}
		return -1;
	}
	sscanf(data, "%s%n", R->command, &offset);
	data += offset;
	if(strcmp(R->command, "ls") == 0){
		DIR *dp;
		struct dirent *ep;
		dp = opendir(folder_name);
		int w = 0;
		if(dp == NULL){
			ERR_EXIT("open folder");
		}
		while((ep = readdir(dp)) != NULL){
			if(ep->d_name[0] == '.'){
				continue;
			}
			char temp_folder_name[BUF_SIZE] = {};
			sprintf(temp_folder_name, "%s\n", ep->d_name);
			w = write(R->conn_fd, temp_folder_name, strlen(temp_folder_name));
			if(w == -1){
				disconnect = 1;
				(void)closedir(dp);
				return -1;
			}
			w = 0;
		}
		char over = 7;
		w = write(R->conn_fd, &over, 1);
		(void)closedir(dp);
		if(w == -1){
			disconnect = 1;
			return -1;
		} 
		keep_connect(R);
		return 1;
	}
	else if(strcmp(R->command, "put") == 0){
		sscanf(data, "%s %d", R->filename, &R->left);
		
		//open file
		char temp_filename[BUF_SIZE] = {};
		sprintf(temp_filename, "./%s/%s", folder_name, R->filename);
		R->f = open(temp_filename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
		
		//to check open error
		if(R->f < 0){
			ERR_EXIT("creat file");
		}
		
		//accept
		int acc = 1;
		int w = write(R->conn_fd, &acc, sizeof(int));
		if(w == -1){
			close(R->f);
			disconnect = 1;
			return -1;
		}

		//file lingth = 0
		if(R->left == 0){
			close(R->f);
			keep_connect(R);
			w = 0;
			int succ = 1;
			w = write(R->conn_fd, &succ, sizeof(int));
			if(w == -1){
				close(R->f);
				disconnect = 1;
				return -1;
			}
			return 1;
		}
	}
	else if(strcmp(R->command, "get") == 0){
		sscanf(data, "%s", R->filename);

		//to check if file exists
		char temp_filename[BUF_SIZE] = {};
		int w = 0;
		sprintf(temp_filename, "./%s/%s", folder_name, R->filename);
		R->f = open(temp_filename, O_RDONLY);
		
		int acc = 0;
		if(R->f < 0){
			//file doesn't exist
			if(errno == 2){
				acc = -1;
				w = write(R->conn_fd, &acc, sizeof(int));
				if(w == -1){
					disconnect = 1;
					return -1;
				}
				keep_connect(R);
				return 0;
			}
		}
		else{
			//file exists
			char temp_messange[BUF_SIZE] = {};
			int acc = lseek(R->f, 0, SEEK_END) - lseek(R->f, 0, SEEK_SET);
			w = write(R->conn_fd, &acc, sizeof(int));
			if(w == -1){
				close(R->f);
				disconnect = 1;
				return -1;
			}
			FD_SET(R->conn_fd, &master_write_set);
			return 0;
		}
	}
	else if(strcmp(R->command, "play") == 0){
		sscanf(data, "%s", R->filename);
		char temp_filename[BUF_SIZE] = {};
		sprintf(temp_filename, "./%s/%s", folder_name, R->filename);
		//printf("%s\n", temp_filename);
		cap[R->conn_fd].open(temp_filename);
		if(!(cap[R->conn_fd].isOpened())){
			int acc = -1;
			int w = write(R->conn_fd, &acc, sizeof(int));
			if(w == -1){
				cap[R->conn_fd].release();
				disconnect = 1;
				return -1;
			}
			keep_connect(R);
			return 0;
		}
		else{
			int acc = 1;
			int w = write(R->conn_fd, &acc, sizeof(int));
			if(w == -1){
				cap[R->conn_fd].release();
				disconnect = 1;
				return -1;
			}
		}

		// get the resolution of the video
		R->width = cap[R->conn_fd].get(CV_CAP_PROP_FRAME_WIDTH);
		R->height = cap[R->conn_fd].get(CV_CAP_PROP_FRAME_HEIGHT);
		//printf("%d %d\n", width, height);
		int w = write(R->conn_fd, &R->width, sizeof(int));
		if(w == -1){
			cap[R->conn_fd].release();
			disconnect = 1;
			return -1;
		}
		w = write(R->conn_fd, &R->height, sizeof(int));
		if(w == -1){
			cap[R->conn_fd].release();
			disconnect = 1;
			return -1;
		}
		img[R->conn_fd] = Mat::zeros(R->height, R->width, CV_8UC3);
		if(!(img[R->conn_fd].isContinuous())){
			img[R->conn_fd] = img[R->conn_fd].clone();
		}
		FD_SET(R->conn_fd, &master_write_set);
		return 0;
	}
			//slouldn't go here
	return 0;
}

int my_max(int x, int y){
	if(x > y){
		return x;
	}
	return y;
}

int my_min(int x, int y){
	if(x > y){
		return y;
	}
	return x;
}

void keep_connect(request* R){
	int temp = R->conn_fd;
	init_request(R);
	R->conn_fd = temp;
	return;
}




