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
#include <string.h>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <sstream>

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

struct sockaddr_in server_addr;
socklen_t size = sizeof(struct sockaddr);

void printToConsole(const int option, const int seqNo = 0, const bool syn = false, const bool retransmission = false, const bool fin = false) {
	string sequenceNumber = to_string(seqNo);
	string toPrint;

	if (option == 1)
		toPrint = "Receiving Packet ";
	else
		toPrint = "Sending Packet ";

	if (!syn)
		toPrint += sequenceNumber;
	else
		toPrint += "SYN";

	if (retransmission)
		toPrint += " retransmission";

	if (fin)
		toPrint += " FIN";

	fprintf(stdout, "%s\n", toPrint.c_str());

}

bool handShake(const int& sock) {
	tcphdr handshake;
	tcphdr recHandshake;
	int num_timeouts = 0;
	handshake.th_seq = 0;
	handshake.th_ack = 0;
	handshake.th_flags = TH_SYN;

	while (num_timeouts < 50) {

		printToConsole(0, 0, true);
		sendto(sock, &handshake, sizeof(struct tcphdr), 0,
			(struct sockaddr *)&server_addr, size);


		if (recvfrom(sock, &recHandshake, sizeof(struct tcphdr), 0,
			(struct sockaddr *)&server_addr, &size) < 0) {
			//timeout reached
			num_timeouts++;
			printf("Time out reached. Resending Handshake. timeout count: %d\n", num_timeouts);
			continue;
		}
		else{	
			break;
		}		
	}

	if (num_timeouts >= 50)
		return false;

	if (recHandshake.th_flags == (TH_SYN | TH_ACK)) {
		cout << "Received Packet SYN ACK" << endl;		
		return true;
	}
	else
		return false;
}

void sendFileName(int& sock, char* fileName) {
	tcphdr fileNameHdr;
	fileNameHdr.th_flags = TH_ACK;
	for (int i = 0; i < strlen(fileName); i++) {
		fileNameHdr.data[i] = fileName[i];
	}
	fileNameHdr.content_length = strlen(fileName);
	fileNameHdr.th_seq = 0;
	fileNameHdr.th_ack = 0;
	printf("Sending Packet filename: %s\n", fileName);
	if(sendto(sock, &fileNameHdr, sizeof(struct tcphdr), 0,
            (struct sockaddr *)&server_addr, size) < 0){
		cout << "Send failed.";
	}
}

bool readFileFromSock(int& sock, char* fileName) {
	tcphdr input;
	tcphdr output;
	ofstream file;
	unordered_set<unsigned long int> sentAckNos;
	vector<pair<uint32_t, pair<uint32_t, char*>>> dataBuffer;
	file.open("received.data");
	bool fileNameAcked = false;
	while (true) {
		if(recvfrom(sock, &input, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, &size) < 0){		
			if(!fileNameAcked)
				sendFileName(sock, fileName);
			continue;
		}
		if(input.th_flags == (TH_SYN | TH_ACK))
			continue;
		fileNameAcked = true;
		if(input.th_flags == TH_SYN)
			continue;
		if (input.th_flags == TH_FNF) {
			printf("404: FILE NOT FOUND\n");
			file.close();
			return false;
		}
		if(input.th_flags == TH_FIN)
			cout << "Receiving Packet FIN" << endl;
		else
			printToConsole(1, input.th_seq);
		unordered_set<unsigned long int>::const_iterator got = sentAckNos.find(input.UID);
		if (got == sentAckNos.end() && input.th_flags != TH_FIN) {
			char* data = new char[input.content_length];
			for (int i = 0; i < input.content_length; i++) {
				data[i] = input.data[i];
			}
			dataBuffer.push_back(make_pair(input.UID, make_pair(input.content_length, data)));
			sentAckNos.insert(input.UID);
		}
		if (input.th_flags == TH_FIN) {
			output.th_flags = TH_ACK | TH_FIN;
		}
		else {
			memset(&input.data[0], 0, sizeof(input.data));
			output.th_flags = TH_ACK;
		}
		output.th_ack = input.th_seq;
		if (output.th_flags == (TH_ACK | TH_FIN))
			cout << "Sending Packet FIN ACK" << endl;
		else if (got == sentAckNos.end())
			printToConsole(0, output.th_ack);
		else
			printToConsole(0, output.th_ack, false, true);
		sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
		if (input.th_flags == TH_FIN) {
			break;
		}
	}
	//FINACK Receive handling
	struct timeval tv;

	tv.tv_sec = 0; 
	tv.tv_usec = 500000;  // Not init'ing this can cause strange errors
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));

	while(true){
		if(recvfrom(sock, &input, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, &size)>0) 
		{		
			if(input.th_flags == TH_FIN){
				output.th_flags = (TH_FIN | TH_ACK);
				cout << "Sending Packet FIN ACK" << endl;				
				sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
			}

			if(input.th_flags == TH_ACK){
				cout << "Received ACK. Exited cleanly.." << endl;
				break;				
			}
		}
		else {
			output.th_flags = (TH_FIN | TH_ACK);
			cout << "Timeout. Resending Packet FIN ACK" << endl;				
			sendto(sock, &output, sizeof(struct tcphdr), 0, (struct sockaddr *)&server_addr, size);
			continue;
		}
	}
	
	
	sort(dataBuffer.begin(), dataBuffer.end());
	for (std::vector<pair<uint32_t, pair<uint32_t, char*>>>::iterator it = dataBuffer.begin(); it != dataBuffer.end(); ++it) {
		stringstream s;
		for(int j = 0; j < it->second.first; j++)
			s << it->second.second[j];
		file << s.str();
	}
	file.close();
	return true;
}


int main(int argc, char* argv[]) {

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <server-host-or-ip> <port-number>\n", argv[0]);
		exit(1);
	}

	string serverHostName(argv[1]);
	int serverPortNo = atoi(argv[2]);

	cout << serverHostName << endl;
	cout << serverPortNo << endl;

	int sock;
	struct hostent *host;
	char send_data[1024];

	host = (struct hostent *) gethostbyname(argv[1]);


	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror("socket");
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(serverPortNo);
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server_addr.sin_zero), 8);

	struct timeval tv;

	tv.tv_sec = 0; 
	tv.tv_usec = 500000;  // Not init'ing this can cause strange errors
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));

	if (!handShake(sock)) {
		fprintf(stderr, "Handshake not completed. Exited...");
		return 0;
	}

	sendFileName(sock, argv[3]);

	readFileFromSock(sock, argv[3]);

//	tcphdr currentHeader;
}

