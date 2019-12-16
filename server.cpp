#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <cstring>
#include <errno.h>
#include <time.h>
#include <fstream>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>

#include "utils.h"

using namespace std;

//ip = 127.0.0.3:65280
//address=<addr_ip> (<cli_name>)   port=<numer_portu>   protocol=<id_protokolu>

//Serwer dla kazdego polaczenia otwiera nowy watek nasluchujacy na nadchodzace zadania,
//dla kazdego z nich wysyla odpowiedz. Teraz serwer obsluguje polaczenia zwykle oraz szyfrowane TLS.
int main(void) {
	int socketDescrpt, descriptor;

	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(8080);
	inet_aton("127.0.0.3", reinterpret_cast<in_addr*>(&(address.sin_addr)));

	sockaddr_in clientAddress;
	unsigned clientSocketLen = sizeof(clientAddress);

	socketDescrpt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketDescrpt == -1) {
		cout << "Problem z gniazdem: " << strerror(errno) << endl;
		return 0;
	}

	int optVal = 1;
	descriptor = setsockopt(socketDescrpt, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));
	if (descriptor == -1) {
		cout << "Problem z ustawianiem opcji gniazda: " << strerror(errno) << endl;
		descriptor = close(socketDescrpt);
		return 0;
	}

	descriptor = bind(socketDescrpt, reinterpret_cast<sockaddr*>(&address), sizeof(address));
	if (descriptor == -1) {
		cout << "Problem z bindem: " << strerror(errno) << endl;
		descriptor = close(socketDescrpt);
		return 0;
	}

	descriptor = listen(socketDescrpt, 1);
	if (descriptor == -1) {
		cout << "Problem z nasluchiwaniem: " << strerror(errno) << endl;
		descriptor = close(socketDescrpt);
		return 0;
	}

	int i = 0;
	while(1) {
	
		int acceptDescrpt = accept(socketDescrpt, reinterpret_cast<sockaddr*>(&clientAddress), &clientSocketLen);
		if (acceptDescrpt == -1) {
			cout << "Problem z akceptacja: " << strerror(errno) << endl;
		} else if(i < 2) {

			thread activeConnection([&i, descriptor, acceptDescrpt, clientAddress]() mutable {
				++i;
				addrinfo* info;
				const addrinfo ai = {};
				descriptor = getaddrinfo(inet_ntoa(clientAddress.sin_addr), to_string(clientAddress.sin_port).c_str(), &ai, &info);
				char host[21];
				descriptor = gethostname(host, sizeof(host));

				//Timeout dla polaczen
				struct timeval timeOut;
				timeOut.tv_sec = 60; // 60-sekundowy timeout
				timeOut.tv_usec = 0;
				setsockopt(acceptDescrpt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOut, sizeof timeOut);

				cout << "NEW CONNECTION: Address = " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port)
					 << " (" << host << "), protocol = " << (info)->ai_protocol << " ESTABLISHED.\n\n";

				//Mozemy wpisac w konsoli klienta np.: "GET /Strona/index.html HTTP/1.1".
				char header[64]="";
				char message[8192] = "";
				char address[100] = "";
				char port[5] = "";
				int received = recv(acceptDescrpt, &header, sizeof(header), MSG_PEEK);
				if (canHandleRequest(header)) {
					handleHttpConnection(acceptDescrpt, header);
				} else {
					string response = "HTTP/1.1 502 NOT IMPLEMENTED\nContent-Type: text/plain\nContent-Length: 23\n\nRequest type handler not implemented.\n\n";
					descriptor = send(acceptDescrpt, response.c_str(), strlen(response.c_str()), 0);
				}

				cout << "CONNECTION TO: " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port) << " CLOSED.\n\n";
				freeaddrinfo(info);
				descriptor = close(acceptDescrpt);
				--i;
			});
			activeConnection.detach();

		} else {
			string response = "HTTP/1.1 503 SERVICE UNAVAILABLE\nContent-Type: text/plain \nContent-Length: 12\n\nUnavailable.\n\n";
			descriptor = send(acceptDescrpt, response.c_str(), strlen(response.c_str()), 0);
			descriptor = close(acceptDescrpt);
		}
	}

	descriptor = close(socketDescrpt);
	cout << "Server closed.\n";
	return 0;
}