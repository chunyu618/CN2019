#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[1000];
} segment;

void setIP(char *dst, char *src) {
	if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost")){
		sscanf("127.0.0.1", "%s", dst);
	} 
	else{
		sscanf(src, "%s", dst);
	}
}

int setACK(segment *ACK, segment *Data, int seq){
	ACK->head.ack = 1;
	ACK->head.fin = 0;
	ACK->head.syn = 0;
	if(Data->head.seqNumber == seq + 1){
		ACK->head.ackNumber = seq + 1;
		return seq + 1;
	}
	ACK->head.ackNumber = seq;
	return seq;
}

int main(int argc, char *argv[]){
	int rec_socket, portNum;
	struct sockaddr_in agent, receiver, temp_addr;
	socklen_t agent_size, rec_size, temp_size;
	char rec_ip[32], agent_ip[32];
	int rec_port, agent_port;
	int ACK_num = 0;
	segment Data, ACK;
	int seg_size;
	
	
	//Check format
	if(argc != 4){
		fprintf(stderr, "usage : <Agent IP> <Agent Port> <Receiver Port>\n");
		fprintf(stderr, "./receiver local 8888 8889\n");
		exit(1);
	}
	else{
		//set IP and port number
		char temp[8] = "local";
		setIP(rec_ip, temp);
		setIP(agent_ip, argv[1]);
		
		sscanf(argv[2], "%d", &agent_port);
		sscanf(argv[3], "%d", &rec_port);
	}

	//create UDP socket
	rec_socket = socket(PF_INET, SOCK_DGRAM, 0);
	
	//Configuration setting in receiver socket
	receiver.sin_family = AF_INET;
	receiver.sin_port = htons(rec_port);
	receiver.sin_addr.s_addr = inet_addr(rec_ip);
	memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero));

	//bind socket
	bind(rec_socket, (struct sockaddr *)&receiver, sizeof(receiver));

	//Configuration setting in agent socket
	agent.sin_family = AF_INET;
	agent.sin_port = htons(agent_port);
	agent.sin_addr.s_addr = inet_addr(agent_ip);
	memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

	//Initialize size variable 
	rec_size = sizeof(receiver);
	agent_size = sizeof(agent);
	temp_size = sizeof(temp_addr);
	
	//Get the resolution of video
	while(1){
		recvfrom(rec_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&temp_addr, &temp_size);
		fprintf(stderr, "recv	data	#%d\n", Data.head.seqNumber);
		memset(&ACK, 0, sizeof(ACK));
		ACK_num = setACK(&ACK, &Data, ACK_num);
		if(ACK_num == 0){
			fprintf(stderr, "drop	data	#%d\n", Data.head.seqNumber);
		}
		seg_size = sizeof(ACK);
		sendto(rec_socket, &ACK, seg_size, 0, (struct sockaddr*)&agent, agent_size);
		fprintf(stderr, "send	ack	#%d\n", ACK.head.ackNumber);
		if(ACK_num == 1){
			break;
		}
	}

	int width, height, imgSize;
	sscanf(Data.data, "%d %d %d", &width, &height, &imgSize);
	
	int SPF = (imgSize + 999) / 1000; //Segment Per Frame
	Mat img;
	img = Mat::zeros(height, width, CV_8UC3);
	uchar *iptr = img.data;
	//uchar buf[imgSize];
	uchar *buf = (uchar*)malloc(sizeof(uchar) * imgSize);

	while(1){
		//receive data
		recvfrom(rec_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&temp_addr, &temp_size);
		if(Data.head.fin == 1){
			fprintf(stderr, "recv	fin\n");
		}
		else{
			fprintf(stderr, "recv	data	#%d\n", Data.head.seqNumber);
		}


		//send ack
		memset(&ACK, '\0', sizeof(ACK));
		int temp = ACK_num;
		ACK_num = setACK(&ACK, &Data, ACK_num);
		seg_size = sizeof(ACK);
		if(Data.head.fin == 1){
			ACK.head.fin = 1;
		}
		sendto(rec_socket, &ACK, seg_size, 0, (struct sockaddr*)&agent, agent_size);
		if(Data.head.fin == 1){
			fprintf(stderr, "send	finack\n");
		}
		else{
			fprintf(stderr, "send	ack	#%d\n", ACK.head.ackNumber);
		}

		//Right seqNumber
		if(temp != ACK_num){
			//Video finish
			if(Data.head.fin == 1){
				destroyAllWindows();
				break;
			}
			memcpy(&buf[((temp-1) % SPF) * 1000], Data.data, Data.head.length);
			
			//if buffer is full
			if((ACK_num > 1) && (ACK_num % SPF) == 1){
				memcpy(iptr, buf, imgSize);
				imshow("Video", img);
				fprintf(stderr, "flush buffer\n");
				char c = (char)waitKey(33.33333);
				if(c == 27){
					break;
				}
				memset(iptr, 0, imgSize);
			}
		
		}
		else{
			fprintf(stderr, "drop	data	#%d\n", Data.head.seqNumber);
		}

		
	}
	free(buf);
	return 0;
}
