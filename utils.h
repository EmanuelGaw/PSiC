#ifndef UTILS_H_
#define UTILS_H_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <regex>

#include "stringValidate.h"

using namespace std;

bool getTargetAddress(string & request, string & address, int & port) {
	regex normalReqPattern("(.*) (https?://)?([\\-0-9A-Za-z\\.]*)/(.*) HTTP(.*)");
	regex proxyReqPattern("CONNECT (.*):(.*) HTTP/1.[0-9]+");
	smatch match;
	if (regex_search(request, match, proxyReqPattern) && match.size() > 1) {
		address = match.str(1); port = stoi(match.str(2));
		request = "";
		return false;
  	} else if(regex_search(request, match, normalReqPattern) && match.size() > 1) {
		address = match.str(3); request = regex_replace(request, normalReqPattern, "$1 /$4 HTTP$5");
		if(match.str(2) == "https://") port = 443; else port = 80;
		return true;
  	} else {
		address = ""; port = 0;
		return false;
	}
}

bool canHandleRequest(string request) {
	regex reqPattern("(.*) HTTP/1.[0-9]+");
	smatch match;
	if (regex_search(request, match, reqPattern) && match.size() > 1) return true;
	return false;
}

void initialExchange(int clientSt, int targetSt, string initRequest) {
	char recvData[8192] = "";

	int desc = send(targetSt, initRequest.c_str(), strlen(initRequest.c_str()), 0);
	if (desc < 0) return;

	//while() oraz vector<>() aby przechowywac string z requestem podzielonym na czesci
	desc = recv(targetSt, &recvData, sizeof(recvData), 0);

	desc = send(clientSt, recvData, strlen(recvData), 0);
}

void handleHttpConnection(int & clientSt, string initRequest) {
	int sendDesc = 0;
	char * answer = "";
	if (strlen(initRequest.c_str()) > 8192) {
		answer = "HTTP/1.1 413 PAYLOAD TOO LARGE\nContent-Type: text/plain\nContent-Length: 18\n\nPayload too large!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
		return;
	}

	string address;
	int port;
	bool init = getTargetAddress(initRequest, address, port);
	cout << "Request=[" << initRequest << "], target address=[" << address << "], port=" << port << "\n";
	
	sockaddr_in targetAddress;
	targetAddress.sin_family = AF_INET;
	targetAddress.sin_port = htons(port);
	if (inet_aton(address.c_str() , reinterpret_cast<in_addr*>(&(targetAddress.sin_addr))) != 1) {
		//Dla ulatwienia, powinno byc uzyte getaddrinfo()
		auto he = gethostbyname(address.c_str());
		if (he == nullptr) {
			answer = "HTTP/1.1 404 NOT FOUND\n\nHost not found...\n";
			sendDesc = send(clientSt, answer, strlen(answer), 0);
			return;
		}
		memcpy(&targetAddress.sin_addr, he->h_addr, he->h_length);
	}
	unsigned targetSocketLen = sizeof(sockaddr_in);

	int targetSt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (targetSt == -1) {
		cout << "DEBUG SOCKET " << strerror(errno) << endl;
		answer = "HTTP/1.1 500 INTERNAL SERVER ERROR\nContent-Type: text/plain\nContent-Length: 25\n\nCouldn't create a socket!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
		return;
	}

	//Timeout dla polaczen
	struct timeval timeOut;
	timeOut.tv_sec = 60; // 60-sekundowy timeout
	timeOut.tv_usec = 0;
	setsockopt(targetSt, SOL_SOCKET, SO_RCVTIMEO | O_NONBLOCK, &timeOut, sizeof timeOut); // | SO_REUSEADDR

	/*sendDesc = bind(targetSt, reinterpret_cast<sockaddr*>(&targetAddress), sizeof(targetAddress));
	if (sendDesc == -1) {
		cout << strerror(errno) << endl;
		answer = "HTTP/1.1 500 INTERNAL SERVER ERROR\nContent-Type: text/plain\nContent-Length: 25\n\nCouldn't create a socket!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
		return;
	}*/

	int connectDesc = connect(targetSt, reinterpret_cast<sockaddr*>(&targetAddress), targetSocketLen);
	if (connectDesc == -1) {
		cout << "DEBUG CONNECT " << strerror(errno) << endl;
		answer = "HTTP/1.1 502 BAD GATEWAY\nContent-Type: text/plain\nContent-Length: 12\n\nBad gateway!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
	} else {
		cout << "CONNECTED TO: " << address << " :: " << port << endl;
		if(init) initialExchange(clientSt, targetSt, initRequest);
		else {
			answer = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 10\n\nConnected!\n\n";
			sendDesc = send(clientSt, answer, strlen(answer), 0);
		}
		int descriptor = 0;
		//Nasz nowy numer
		char newNumber[28] = "22222222222222222222222200";

		while(1) {
			char recvData[8192] = "";

			descriptor = recv(clientSt, &recvData, sizeof(recvData), 0);
			if (descriptor > 8192) {
				string response = "HTTP/1.1 413 PAYLOAD TOO LARGE\nContent-Type: text/plain\nContent-Length: 18\n\nPayload too large!\n\n";
				sendDesc = send(clientSt, response.c_str(), strlen(response.c_str()), 0);
				return;
			} else
				if (descriptor > 0) {
				cout << " Received data from client: " << recvData << endl;
				sendDesc = send(targetSt, recvData, descriptor, 0);
				if (sendDesc < 0) return;
				
				descriptor = recv(targetSt, &recvData, sizeof(recvData), 0);
				cout << " Received data from target: " << recvData << endl;
				
				changeNumberInString(recvData, newNumber);

				sendDesc = send(clientSt, recvData, descriptor, 0);
				if (sendDesc < 0) return;
			} else break;
			
		}
	}
	close(targetSt);
}

#endif
