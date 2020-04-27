#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include "opencv2/opencv.hpp"
#include <iostream>
#include<errno.h>
#include<deque>

using namespace std;
using namespace cv;

//file information
char filepath[128];
VideoCapture cap;
Mat img;

int my_min(int x, int y){
	return (x<y)? x:y;
}

int my_max(int x, int y){
	return (x>y)? x:y;
}

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

deque<segment> waitACK;
deque<segment> waitSent;



void setIP(char *dst, char *src){
	if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost")){
		sscanf("127.0.0.1", "%s", dst);
	}
	else{
		sscanf(src, "%s", dst);
	}
}

void MakeData(segment *Data, uchar temp[], int seq, int n){
	memset(Data, '\0', sizeof(Data));
	memcpy(Data->data, temp, n);
	Data->head.length = n;
	Data->head.seqNumber = seq;
	Data->head.fin = 0;
	Data->head.ack = 0;
	Data->head.syn = 0;
}


int main(int argc, char *argv[]){
	int sender_socket, portNum;
	struct sockaddr_in sender, agent, temp_addr;
	socklen_t sender_size, agent_size, temp_size;
	char sender_ip[32], agent_ip[32];
	int sender_port, agent_port;
	double N = 1; //window size
	int Thres = 16;
	int base = 1; //waiting ACK number
	int sent = 1; //next sending seq number
	int recv_val;
	int first_sent = 1;
	segment Data, ACK;

	//check format
	if(argc != 5){
		fprintf(stderr, "Usage : <agent IP> <agent port> <sender port> <filepath>\n");
		fprintf(stderr, "example: ./sender local 8888 8887 ./tmp.mpg\n");
		exit(0);
	}
	else{
		//set IP and port number
		char temp[8] = "local";
		setIP(sender_ip, temp);
		setIP(agent_ip, argv[1]);

		sscanf(argv[2], "%d", &agent_port);
		sscanf(argv[3], "%d", &sender_port);
		
		//open file
		strcpy(filepath, argv[4]);
		cap.open(filepath);
		//To check file exist
		if(!(cap.isOpened())){
			fprintf(stderr, "The %s doesn't exist.\n", filepath);
			exit(0);
		}
	}

	//Create UDP socket
	sender_socket = socket(PF_INET, SOCK_DGRAM, 0);

	//Setting timeout option	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;	
	setsockopt(sender_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	//Configuration setting in sender socket
	sender.sin_family = AF_INET;
	sender.sin_port = htons(sender_port);
	sender.sin_addr.s_addr = inet_addr(sender_ip);
	memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));

	//Configuration setting in agent socket
	agent.sin_family = AF_INET;
	agent.sin_port = htons(agent_port);
	agent.sin_addr.s_addr = inet_addr(agent_ip);
	memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

	//bind socket
	bind(sender_socket, (struct sockaddr*)&sender, sizeof(sender));

	//Initialize size variables
	sender_size = sizeof(sender);
	agent_size = sizeof(agent);
	temp_size = sizeof(temp_addr);

	//Get the resolution of vedio
	int width = cap.get(CV_CAP_PROP_FRAME_WIDTH);
	int height = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
	
	//Compute the imgSize
	img = Mat::zeros(height, width, CV_8UC3);
	if(!(img.isContinuous())){
		img = img.clone();
	}
	cap >> img;
	int imgSize = img.total() * img.elemSize();	
	
	int SPF = (imgSize+999) / 1000;//Segment Per Frame

	//send width, height and ingSize to receiver
	char temp[1000] = {};
	sprintf(temp, "%d %d %d", width, height, imgSize);
	MakeData(&Data, (uchar*)&temp, 1, sizeof(temp));
	int segment_size = sizeof(Data);
	sendto(sender_socket, &Data, segment_size, 0, (struct sockaddr*)&agent, agent_size);
	fprintf(stderr, "send	data	#%d, 	winSize = %d\n", base,(int)N);
	first_sent = 1;
	while((recv_val = recvfrom(sender_socket, &ACK, sizeof(ACK), 0, (struct sockaddr*)&temp_addr, &temp_size)) == -1){
		sendto(sender_socket, &Data, segment_size, 0, (struct sockaddr*)&agent, agent_size);
		fprintf(stderr, "resend	data	#%d,	winSize = %d\n", base, (int)N);
	}
	if(recv_val){
		fprintf(stderr, "recv	ack	#%d\n", ACK.head.ackNumber);
	}
	base++;
	sent++;
	N++;
	int timeout = 0;


	//printf("%d %d\n", imgSize, SPF);
	//exit(0);

	uchar buf[imgSize] = {};
	memcpy(buf, img.data, imgSize);
	int pos = 0;

	while(imgSize - pos > 0){
		MakeData(&Data, &buf[pos], sent, my_min(1000, imgSize - pos));
		waitSent.push_back(Data);
		pos += 1000;
		sent++;
	}
	pos = 0;

	while(1){

		//sending until winSize is full
		while(waitACK.size() < (int)N){
			
			//next frame
			if(waitSent.empty()){
				cap >> img;
				if(img.empty()){
					break;
				}
				memcpy(buf, img.data, imgSize);
				pos = 0;
				while(imgSize - pos > 0){
					MakeData(&Data, &buf[pos], sent, my_min(imgSize - pos, 1000));
					waitSent.push_back(Data);
					pos += 1000;
					sent++;
				}
				pos = 0;
				
			}	
			
			Data = waitSent.front();
			waitSent.pop_front();
			sendto(sender_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&agent, agent_size);
			if(first_sent >= Data.head.seqNumber){	
				fprintf(stderr, "resend	data	#%d,	winSize = %d\n", Data.head.seqNumber, (int)N);
			}
			else{
				fprintf(stderr, "send	data	#%d,	winSize = %d\n", Data.head.seqNumber, (int)N);
				first_sent = Data.head.seqNumber; 
			}
			waitACK.push_back(Data);
			
			
		}
		
		//wait ACK
		while(!(waitACK.empty())){
			memset(&ACK, 0, sizeof(ACK));
			if((recv_val = recvfrom(sender_socket, &ACK, sizeof(ACK), 0, (struct sockaddr*)&temp_addr, &temp_size)) == -1){
				timeout = 1;
				break;
			}
			fprintf(stderr, "recv	ack	#%d\n", ACK.head.ackNumber);
			
			//wrong ACK number caused by packet loss
			if(base > ACK.head.ackNumber){
				continue;
			}

			//renew base
			base = ACK.head.ackNumber + 1;
			segment temp_ACK = waitACK.front();
			waitACK.pop_front();
			while(temp_ACK.head.seqNumber < ACK.head.ackNumber){
				temp_ACK = waitACK.front();
				waitACK.pop_front();
			}

			//renew Window Size
			if((int)N < Thres){
				N = N + 1;
			}
			else{
				N = N + 1/N;
			}

			//send segment
			
			//next frame
			if(waitSent.empty()){
				cap >> img;
				if(img.empty()){
					continue;
				}	
				memcpy(buf, img.data, imgSize);
				pos = 0;
				while(imgSize - pos > 0){
					MakeData(&Data, &buf[pos], sent, my_min(imgSize - pos, 1000));
					waitSent.push_back(Data);
					pos += 1000;
					sent++;
				}
				pos = 0;
			}

			Data = waitSent.front();
			waitSent.pop_front();
			sendto(sender_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&agent, agent_size);
			if(first_sent >= Data.head.seqNumber){
				fprintf(stderr, "resend	data	#%d,	winSize = %d\n", Data.head.seqNumber, (int)N);
			}
			else{
				fprintf(stderr, "send	data	#%d,	winSize = %d\n", Data.head.seqNumber, (int)N);
				first_sent = Data.head.seqNumber;
			}
			waitACK.push_back(Data);

			
			//exponentially growing
			if((int)N < Thres){
				
				//next frame
				if(waitSent.empty()){
					cap >> img;
					if(img.empty()){
						continue;
					}
					memcpy(buf, img.data, imgSize);
					pos = 0;
					while(imgSize - pos > 0){
						MakeData(&Data, &buf[pos], sent, my_min(imgSize - pos, 1000));
						waitSent.push_back(Data);
						pos += 1000;
						sent++;
					}
					pos = 0;
				
				}

				Data = waitSent.front();
				waitSent.pop_front();
				sendto(sender_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&agent, agent_size);
				if(first_sent > Data.head.seqNumber){
					fprintf(stderr, "resend	data	#%d,	winSize = %d\n", Data.head.seqNumber, (int)N);
				}
				else{	
					fprintf(stderr, "send	data	#%d,	winSize = %d\n", Data.head.seqNumber, (int)N);
					first_sent = Data.head.seqNumber;
				}
				waitACK.push_back(Data);				
			}	
			else{
				break;
			}
		}

		//Timeout
		if(timeout == 1){
			timeout = 0;
			Thres = my_max((int)N/2, 1);
			N = 1.0;
			fprintf(stderr, "time	out,			threshold = %d\n", Thres);
			while(!waitACK.empty()){
				segment Data_temp;
				Data_temp = waitACK.back();
				waitACK.pop_back();
				waitSent.push_front(Data_temp);
			}
			continue;
		}
		
		//finish
		if(waitSent.empty() && waitACK.empty()){
			//printf("----------------\n");
			//printf("receive all ACK\n");
			//printf("----------------\n");
			break;
		}
	}
	uchar fin_temp[1000];
	while(1){
		MakeData(&Data, fin_temp, sent, 0);
		Data.head.fin = 1;
		sendto(sender_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&agent, agent_size);
		if(first_sent >= Data.head.seqNumber){
			fprintf(stderr, "resend	fin,	winSize = %d\n", (int)N);
		}
		else{
			fprintf(stderr, "send	fin,	winSize = %d\n", (int)N);
			first_sent = Data.head.seqNumber;
		}

		//Timeout
		if((recv_val = recvfrom(sender_socket, &ACK, sizeof(ACK), 0, (struct sockaddr*)&temp_addr, &temp_size)) == -1){			
			
			Thres = my_max((int)N/2, 1);
			N = 1;
			fprintf(stderr, "time	out,			threshold = %d\n", Thres);
			sendto(sender_socket, &Data, sizeof(Data), 0, (struct sockaddr*)&agent, agent_size);
			fprintf(stderr, "resend	fin,	winSize = %d\n", (int)N);

		}
		if(recv_val && ACK.head.ackNumber == sent){
			fprintf(stderr, "recv	finack\n");
			break;
		}
		else{
			fprintf(stderr, "recv	ack	#%d\n", ACK.head.ackNumber);
		}
	}

	cap.release();
	return 0;
}
