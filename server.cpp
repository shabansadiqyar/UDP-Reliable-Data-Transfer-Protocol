#include <sys/types.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <stdlib.h>
#include <strings.h>
#include <unordered_set>
#include <vector>
#include <chrono>

using namespace std;

typedef uint32_t tcp_seq;

//TCP Header structure as per RFC 793
struct tcphdr {
    tcp_seq th_seq;   /* sequence number */
    tcp_seq th_ack;   /* acknowledgement number */
    uint32_t content_length;
    unsigned long int UID;
    unsigned char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_ACK 0x10
#define TH_FNF 0x20

    char data[1000];
};

struct sendhdr{
	tcphdr header;
	chrono::milliseconds timeStamp;
	bool isAcked;
};

struct sockaddr_in server_addr;
socklen_t size = sizeof(struct sockaddr);

void printToConsole(const int option, const int seqNo = 0, const bool syn = false, const bool retransmission = false, const bool fin = false) {
    
    string sequenceNumber = to_string(seqNo);
    string toPrint;
    
    if (option == 1)
        toPrint = "Receiving Packet ";
    else
        toPrint = "Sending Packet ";

    if (!syn && !fin)
        toPrint += sequenceNumber;
    else if(syn)
        toPrint += "SYN";

    if (retransmission)
        toPrint += " retransmission";

    if (fin)
        toPrint += " FIN";

    fprintf(stdout, "%s\n", toPrint.c_str());

}

char* handShake(const int& sock){
	tcphdr handshake;
	tcphdr recHandShake;
	int num_timeouts = 0;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO , (const char*)&tv, sizeof(struct timeval));
	
	while(true){
		if(recvfrom(sock, &recHandShake, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, &size) > 0)
		{
			break;
		}
	}
	
	if(recHandShake.th_flags == TH_SYN)
		handshake.th_flags = (TH_SYN | TH_ACK);

	while (num_timeouts < 50) {

	cout << "Sending Packet SYN ACK" << endl;
        sendto(sock, &handshake, sizeof(struct tcphdr), 0,
            (struct sockaddr *)&server_addr, size);

	 if (recvfrom(sock, &recHandShake, sizeof(struct tcphdr), 0,
            (struct sockaddr *)&server_addr, &size) < 0) {
            //timeout reached
            num_timeouts++;
            printf("Time out reached. Resending Handshake. timeout count: %d\n", num_timeouts);
            continue;
        }
        else{
	    if(recHandShake.th_flags != TH_ACK){
		printf("Need ACK from SYN");
		continue;
      	    }
	
	    printf("Received Packet ACK %s\n" , recHandShake.data);
	    char* fileName = new char[recHandShake.content_length + 1];
	    int i;
	    for(i = 0; i < recHandShake.content_length; i++){
		fileName[i] = recHandShake.data[i];
	    }
	    fileName[i] = 0;
	    return fileName;
            break; 
	}
      }
}

bool SendDataFromSock(int& sock, char* fileName){
	tcphdr input; 
	tcphdr output;
	fstream file;
	vector<pair<uint32_t, pair<uint32_t, char*>>> dataBuffer;
	socklen_t size = sizeof(struct sockaddr);

	ifstream f;
	f.open(fileName, fstream::binary);
	bool file_is_found = f.is_open();

	if(!file_is_found){
		output.th_flags = TH_FNF;
		f.close();
		return false;
	}
	else{	
		f.seekg (0, f.end);
 		uint32_t file_size = uint32_t(f.tellg());
		cout << "FILE SIZE: " << file_size << endl;
    		f.seekg (0, f.beg);
		uint32_t counter = 0;
		uint32_t seq_num = 0; 
		
		while(counter < file_size){
			char* buffer;
			uint32_t currSize = 1000;
			if(file_size - counter < 1000)
				currSize = file_size-counter;
			buffer = new char[currSize];
			f.read(buffer, 1000);
			dataBuffer.push_back(make_pair(seq_num, make_pair(currSize, buffer)));
			counter += 1000;
			if(seq_num + 1000 > 30720)
				seq_num = 0;
			else
				seq_num += 1000;
		}

		int uidCounter = 0;
		
		const int window_size = 5;
		vector<sendhdr> window(window_size);
		for(int i = 0; i < window_size; i++){
			window[i].isAcked = true;
		}

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 5;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO , (const char*)&tv, sizeof(struct timeval));

		for(vector<pair<uint32_t, pair<uint32_t, char*>>>::iterator it = dataBuffer.begin(); it != dataBuffer.end();) {
			if(recvfrom(sock, &input, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, &size) > 0){
				printToConsole(1, input.th_ack);
				for(int i = 0; i < window_size; i++){
					if(input.th_ack == window[i].header.th_seq){
						window[i].isAcked = true;
					}
				}
			}

			for(int i = 0; i < window.size(); i++){
				chrono::time_point<chrono::system_clock> now = chrono::system_clock::now();
				auto duration = now.time_since_epoch();
				auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
				if(!window[i].isAcked){
					if((millis - window[i].timeStamp).count() >= 500){
						cout << "TIMEOUT RESENDING: " << window[i].header.UID << endl;
						window[i].timeStamp = millis;
						sendto(sock, &window[i].header, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
					}
				}
			}

			while(window.back().isAcked){
				chrono::time_point<chrono::system_clock> now = chrono::system_clock::now();
				auto duration = now.time_since_epoch();
				auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
				window.pop_back();
				sendhdr current;
				output.th_seq = it -> first;
				output.UID = uidCounter;
				uidCounter++;
				for(int j=0; j < it->second.first; j++){
					output.data[j] = it->second.second[j];
				}
				output.content_length = it->second.first;
				current.header = output;
				current.isAcked = false;
				current.timeStamp = millis;
				window.insert(window.begin(), current);
				printToConsole(0, output.th_seq);					
				sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
				delete it->second.second;
				++it;
				if(it == dataBuffer.end())
					break;
			}
		}
		bool allLeftOverAcked = false;
		while(!allLeftOverAcked){
			if(recvfrom(sock, &input, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, &size) > 0){
				printToConsole(1, input.th_ack);
				for(int i = 0; i < window.size(); i++){
					if(input.th_ack == window[i].header.th_seq){
						window[i].isAcked = true;
					}
				}
			}
			allLeftOverAcked = true;
			for(int i = 0; i < window.size(); i++){
				if(!window[i].isAcked){
					allLeftOverAcked = false;
					chrono::time_point<chrono::system_clock> now = chrono::system_clock::now();
					auto duration = now.time_since_epoch();
					auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
					if((millis - window[i].timeStamp).count() >= 500){
						cout << "TIMEOUT RESENDING: " << window[i].header.th_seq << endl;
						window[i].timeStamp = millis;
						sendto(sock, &window[i].header, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
					}
				}
			}
		}
		output.th_flags = TH_FIN;
		printToConsole(0, 0, false, false, true);
		sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO , (const char*)&tv, sizeof(struct timeval));

		bool recFinAck = false;
		while(true){
			if(recvfrom(sock, &input, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, &size) < 0){
				if(recFinAck)
					break;
				cout << "FINACK TIMEOUT, Resending FIN" << endl;
				sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
			}
			else{
				if(input.th_flags == (TH_ACK | TH_FIN)){
					recFinAck = true;
					cout << "Receiving Packet FIN ACK" << endl;
					tv.tv_sec = 0;
					tv.tv_usec = 1000000;
					setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO , (const char*)&tv, sizeof(struct timeval));	
					cout << "Sending Packet ACK" << endl;
					output.th_flags = TH_ACK;
					sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
				}
			}
		}
	}

 	file.close();

 	return true;
}

int main(int argc, char* argv[]){
	
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <port-number>\n", argv[1]);
		exit(1);
	}
	
	int serverPortNo = atoi(argv[1]);
	cout << "Server Port Number: " << serverPortNo << endl;

	int sock;
	char send_data[1024];

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    	{
        perror("socket");
        exit(1);
    	}

 	server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(serverPortNo);
        bzero(&(server_addr.sin_zero), 8);

	if ( bind(sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) <0)
		perror("ERROR on binding");
	while(true){
		char* fileName = handShake(sock);
		SendDataFromSock(sock, fileName);
	}	
}

