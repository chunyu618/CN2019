#include <iostream>
#include <sys/socket.h> 
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<unistd.h> 
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<string>
#include<fcntl.h>
#include<sys/stat.h>
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;


#define BUF_SIZE 4096

char server_ip[16];
int port, temp_ip[4], server_fd;

int connect_server(char ip[16], int port);
int my_max(int x, int y);
int my_min(int x, int y);
char *find_first_nonzero(const char *x);
char folder_name[BUF_SIZE] = {"b06902048_client_folder"};
int re_connect();



int main(int argc, char *argv[]){
	if(argc != 2){
		printf("usage: ip:port\n");
		exit(0);
	}
	mkdir(folder_name, 0777);
	sscanf(argv[1], "%d.%d.%d.%d:%d", &temp_ip[0], &temp_ip[1], &temp_ip[2], &temp_ip[3], &port);
	sprintf(server_ip, "%d.%d.%d.%d", temp_ip[0], temp_ip[1], temp_ip[2], temp_ip[3]);
	server_fd = connect_server(server_ip, port);
	if(server_fd == -1){
		return 0;
	}
	printf("connecting to %s port %d...\n", server_ip, port);
	string input;
	while(getline(cin, input)){
		if(strlen((char*)input.c_str()) == 0){
			continue;
		}
		char *c_input = (char*)input.c_str();
		char *pos = strchr(c_input, ' ');
		char command[16] = {};
		if(pos == NULL){
			strncpy(command, c_input, my_min(16, strlen(c_input)));
		}
		else{
			strncpy(command, c_input, my_min(16, pos - c_input));
		}
		if(strcmp(command, "ls") == 0 && pos == NULL){
			write(server_fd, "ls", 2);
			char ls_buf[BUF_SIZE];
			int r;
			memset(ls_buf, 0, sizeof(ls_buf));
			while((r = read(server_fd, ls_buf, sizeof(ls_buf))) != 0){
				int count = 0;
				for(int i = 0; i < strlen(ls_buf); i++){
					if(ls_buf[i] == 7){
						count = 1;
						ls_buf[i] = '\0';
						break;
					}
				}
				write(1, ls_buf, strlen(ls_buf));
				if(count == 1){
					break;
				}
				memset(ls_buf, 0, sizeof(ls_buf));
			}
			continue;
		}
		else if(strcmp(command, "put") == 0){
			char *non_zero = find_first_nonzero(pos);
			if(non_zero == NULL){
				printf("Command not found\n");
				continue;
			}
			char file_name[BUF_SIZE] = {};
			sprintf(file_name, "./%s/%s", folder_name, non_zero);
			int f = open(file_name, O_RDONLY);
			if(f < 0){
				printf("The %s doesn't exist.\n", non_zero);
				continue;
			}
			long long int file_len = lseek(f, 0, SEEK_END) - lseek(f, 0, SEEK_SET);
			long long int now_len = 0;
			printf("%lld/%lld", now_len, file_len);
			char temp_messange[BUF_SIZE] = {};
			//send command to server
			sprintf(temp_messange, "put %s %lld", non_zero, file_len);
			write(server_fd, temp_messange, sizeof(temp_messange) - 1);
			memset(temp_messange, 0, sizeof(temp_messange));
			//accept or not
			int acc = 0;
			read(server_fd, &acc, sizeof(int));
			if(acc == 1){
				char write_buf[BUF_SIZE] = {};
				int r = 0;
				while((r = read(f, write_buf, sizeof(write_buf) - 1))){
					int w = write(server_fd, write_buf, r);
					now_len += w;
					putchar('\r');
					printf("%lld/%lld", now_len, file_len);
					memset(write_buf, 0, sizeof(write_buf));
				}	
			}
			printf("\n");
			//successful or not
			int succ = 0;
			read(server_fd, &succ, sizeof(int));
			if(succ == 1){
				printf("successfully putting\n");
			}
			continue;
		}
		else if(strcmp(command, "get") == 0){
			char *non_zero = find_first_nonzero(pos);
			if(non_zero == NULL){
				printf("Command not found\n");
				continue;
			}
			char temp_messange[BUF_SIZE] = {};
			sprintf(temp_messange, "get %s", non_zero);
			write(server_fd, temp_messange, strlen(temp_messange));
			memset(temp_messange, 0, sizeof(temp_messange));
			//accept or not
			int acc = 0;
			read(server_fd, &acc, sizeof(int));
			if(acc == -1){
				printf("The %s doesn't exist.\n", non_zero);
				continue;
			}
			int filelen = acc, total = 0;
			total = filelen;
			printf("%d/%d", total - filelen, total);
			char temp_filename[1024] = {};
			sprintf(temp_filename, "./%s/%s", folder_name, non_zero);
			int f = open(temp_filename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			if(f < 0){
				//shouldn't happen
				printf("open error\n");
				exit(0);
			}
			int r = 0;
			char rec_buf[BUF_SIZE];
			while(filelen){
				r = read(server_fd, rec_buf, sizeof(rec_buf) - 1);
				int w = write(f, rec_buf, r);
				filelen -= w;
				putchar('\r');
				printf("%d/%d", total - filelen, total);
				r = 0;	
				memset(rec_buf, 0, sizeof(rec_buf));
			}
			printf("\n");
			printf("successfully getting\n");
			continue;
		}
		else if(strcmp(command, "play") == 0){
			char *non_zero = find_first_nonzero(pos);
			if(non_zero == NULL){
				printf("Command not found\n");
				continue;
			}
			int l = strlen(non_zero);
			if(non_zero[l - 4] != '.' || non_zero[l - 3] != 'm' || non_zero[l - 2] != 'p' || non_zero[l - 1] != 'g'){
				printf("The %s is not a mpg file.\n", non_zero);
				continue;
			}
			char temp_messange[1024] = {};
			sprintf(temp_messange, "play %s", non_zero);
			write(server_fd, temp_messange, strlen(temp_messange));
			int acc = 0;
			read(server_fd, &acc, sizeof(int));
			if(acc == -1){
				printf("The %s doesn't exist.\n", non_zero);
				continue;
			}
			else if(acc == 1){
				int width = 0, height = 0;
				read(server_fd, &width, sizeof(int));
				read(server_fd, &height, sizeof(int));
				Mat img;
				img = Mat::zeros(height, width, CV_8UC3);
				while(1){
					int imgSize = 0;
					int r = read(server_fd, &imgSize, sizeof(int));
					if(imgSize < 0){
						exit(0);
					}
					else if(imgSize == 0){
						break;
					}
					uchar *iptr =  img.data;
					recv(server_fd, iptr, imgSize, MSG_WAITALL);
					imshow("Video", img);
					char c = (char)waitKey(33.33333);
					if(c == 27){
						break;
					}
				}
				server_fd = re_connect();
				destroyAllWindows();
			}		
			continue;
		}
		else{
			printf("Command not found\n");
			continue;
		}
		
	}
}

int connect_server(char ip[16], int port){
	int Sock_fd;
	Sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(Sock_fd == -1){
		printf("Fail to create a socket.\n");	
		return -1;
	}

	struct sockaddr_in info;
	memset(&info, 0, sizeof(info));
	info.sin_family = PF_INET;
	info.sin_addr.s_addr = inet_addr(ip);
	info.sin_port = htons(port);
	int err = connect(Sock_fd, (struct sockaddr*)&info, sizeof(info));
	if(err == -1){
		printf("connect error\n");
		return -1;
	}
	return Sock_fd;
}


int my_max(int x, int y){
	if(x > y){
		return x;
	}
	return y;
}

int my_min(int x, int y){
	if(x < y){
		return x;
	}
	return y;
}

char *find_first_nonzero(const char *x){
	char *y = (char*)x;
	if(y == NULL){
		return NULL;
	}
	while(*y == ' '){
		y++;
	}
	if(*y == '\0'){
		return NULL;
	}
	return y;
}

int re_connect(){
	close(server_fd);
	return connect_server(server_ip, port);
}
