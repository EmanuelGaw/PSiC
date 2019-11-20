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

	vector<pollfd> fds;
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(8080);
	inet_aton("127.0.0.3", reinterpret_cast<in_addr*>(&(address.sin_addr)));

	sockaddr_in clientAddress;
	unsigned clientSocketLen = sizeof(clientAddress);

	SSL_CTX *ctx;
    initOpenssl();
    ctx = createContext();
    configureContext(ctx);

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

	int i = 3; //Serwer przyjmie 2 polaczenia, zas przy trzecim zakonczy prace.
	while(i) {

		int acceptDescrpt = accept(socketDescrpt, reinterpret_cast<sockaddr*>(&clientAddress), &clientSocketLen);
		if (acceptDescrpt == -1) {
			cout << "Problem z akceptacja: " << strerror(errno) << endl;
		} else if(--i) {

			thread activeConnection([descriptor, acceptDescrpt, clientAddress, ctx]() mutable { 
				addrinfo* info;
				const addrinfo ai = {};
				descriptor = getaddrinfo(inet_ntoa(clientAddress.sin_addr), to_string(clientAddress.sin_port).c_str(), &ai, &info);
				char host[21];
				descriptor = gethostname(host, sizeof(host));

				//Timeout dla polaczen
				struct timeval timeOut;
				timeOut.tv_sec = 20; // 20-sekundowy timeout
				timeOut.tv_usec = 0;
				setsockopt(acceptDescrpt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeOut, sizeof timeOut);

				cout << "NEW CONNECTION: Address = " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port)
					 << " (" << host << "), protocol = " << (info)->ai_protocol << " ESTABLISHED.\n\n";

				SSL *ssl = SSL_new(ctx);
    			SSL_set_fd(ssl, acceptDescrpt);
				int handshakeResult = 0;
				char header[128]="";

				//Mozemy wpisac w konsoli klienta np.: "GET /Strona/index.html HTTP/1.1".
				handshakeResult = recv(acceptDescrpt, &header, sizeof(header), MSG_PEEK);
				if(!memcmp("", header, 6)) { SSL_set_accept_state(ssl); handleTlsConnection(ssl); }
				else handleNormalConnection(acceptDescrpt);

				cout << "CONNECTION TO: " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port) << " CLOSED.\n\n";
				SSL_free(ssl);
				freeaddrinfo(info);
				descriptor = close(acceptDescrpt);
			});
			activeConnection.detach();
		}
	}

	descriptor = close(socketDescrpt);
	SSL_CTX_free(ctx);
    cleanupOpenssl();
	cout << "Server closed.\n";
	return 0;
}