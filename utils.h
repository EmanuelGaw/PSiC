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

/*
vector<char> readFile (const char* path) {
  vector<char> result;
  
  ifstream file(path, ios::in | ios::binary | ios::ate);
  file.exceptions(ifstream::failbit | ifstream::badbit);
  result.resize(file.tellg(), 0);
  file.seekg (0, ios::beg);
  file.read (result.data(), result.size());
  file.close();

  return result;
}

string getFileExtension(const string &str) {
	string suffix[3] = {"html", "jpg", "js"};
	for (int i = 0; i < 3; i++)
		if(str.size() >= suffix[i].size() && str.compare(str.size() - suffix[i].size(), suffix[i].size(), suffix[i]) == 0) return suffix[i];
	return "";
}

string getResponse(string suffix, vector<char> data) {
	int contentLength = data.size();
	string content(data.begin(), data.end());
	
	string response = "";
	if (suffix == "html")	
		response = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " + to_string(contentLength) + "\n\n";
	else if (suffix == "jpg")
		response = "HTTP/1.1 200 OK\nContent-Type: image/jpeg\nContent-Length: " + to_string(contentLength) + "\n\n";
	else if (suffix == "js")
		response = "HTTP/1.1 200 OK\nContent-Type: application/javascript\nContent-Length: " + to_string(contentLength) + "\n\n";
	else
		response = "HTTP/1.1 200 OK\nContent-Type: application/octet-stream\nContent-Length: " + to_string(contentLength) + "\n\n";
	return response.append(content).append("\n");
}

string processRequest(string request, bool conn) {
	regex connectPattern("CONNECT (.*) HTTP/1.[0-9]+");
	regex reqPattern("(.*) HTTP/1.[0-9]+");
	smatch match;
	conn = false;
	if (regex_search(request, match, connectPattern) && match.size() > 1) {
		conn = true;
		return match.str(1);
  	} else if (regex_search(request, match, reqPattern) && match.size() > 1) {
  		return request;
  	} else {
		return "";
  	}
}
*/

bool getTargetAddress(string & request, string & address, int & port) {
	regex normalReqPattern("(.*) ((https?://)?[\\-0-9A-Za-z\\.]*)/(.*) HTTP(.*)");
	regex proxyReqPattern("CONNECT (.*):(.*) HTTP/1.[0-9]+");
	smatch match;
	if (regex_search(request, match, proxyReqPattern) && match.size() > 1) {
		address = match.str(1); port = stoi(match.str(2));
		request = "";
		return false;
  	} else if(regex_search(request, match, normalReqPattern) && match.size() > 1) {
		address = match.str(2); request = regex_replace(request, normalReqPattern, "$1 /$4 HTTP$5");
		if(match.str(3) == "https://") port = 443; else port = 80;
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
	char recvData[8000] = "";

	int desc = send(targetSt, initRequest.c_str(), strlen(initRequest.c_str()), 0);
	if (desc < 0) return;

	//while oraz vector aby przechowywac string z requestem podzielonym na czesci
	desc = recv(targetSt, &recvData, sizeof(recvData), 0);

	desc = send(clientSt, recvData, strlen(recvData), 0);
}

void handleHttpConnection(int & clientSt, string initRequest) {
	int sendDesc = 0;
	char * answer = "";
	if (strlen(initRequest.c_str()) > 8000) {
		answer = "HTTP/1.1 413 PAYLOAD TOO LARGE\nContent-Type: text/plain\nContent-Length: 18\n\nPayload too large!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
		return;
	}

	string address;
	int port;
	bool init = getTargetAddress(initRequest, address, port);
	
	sockaddr_in targetAddress;
	targetAddress.sin_family = AF_INET;
	targetAddress.sin_port = htons(port);
	inet_aton(address.c_str() , reinterpret_cast<in_addr*>(&(targetAddress.sin_addr)));
	unsigned targetSocketLen = 24;

	int targetSt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (targetSt == -1) {
		answer = "HTTP/1.1 500 INTERNAL SERVER ERROR\nContent-Type: text/plain\nContent-Length: 25\n\nCouldn't create a socket!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
		return;
	}

	//Timeout dla polaczen
	struct timeval timeOut;
	timeOut.tv_sec = 21; // 60-sekundowy timeout
	timeOut.tv_usec = 0;
	setsockopt(targetSt, SOL_SOCKET, SO_RCVTIMEO | SO_REUSEADDR, &timeOut, sizeof timeOut);

	int connectDesc = connect(targetSt, reinterpret_cast<sockaddr*>(&targetAddress), targetSocketLen);
	if (connectDesc == -1) {
		answer = "HTTP/1.1 502 BAD GATEWAY\nContent-Type: text/plain\nContent-Length: 12\n\nBad gateway!\n\n";
		sendDesc = send(clientSt, answer, strlen(answer), 0);
		return;
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
			char recvData[8000] = "";

			descriptor = recv(clientSt, &recvData, sizeof(recvData), 0);
			if (sizeof(recvData) > 8000) {
				string response = "HTTP/1.1 413 PAYLOAD TOO LARGE\nContent-Type: text/plain\nContent-Length: 18\n\nPayload too large!\n\n";
				sendDesc = send(clientSt, response.c_str(), strlen(response.c_str()), 0);
				return;
			} else if (strlen(recvData) > 0){
				cout << " Received data from client: " << recvData << endl;
				descriptor = send(targetSt, recvData, strlen(recvData), 0);
				if (descriptor < 0) return;
				
				descriptor = recv(targetSt, &recvData, sizeof(recvData), 0);
				cout << " Received data from target: " << recvData << endl;
				
				changeNumberInString(recvData, newNumber);

				descriptor = send(clientSt, recvData, strlen(recvData), 0);
				if (descriptor < 0) return;
			} else break;
			
		}
	}
}

/////////////////////////////////////////////////////////////
/*
void handleNormalConnection(int & acceptDescrpt) {
	bool connReq = false;
	int descriptor = 0;
	string response;
	while(1) {
		char recvData[8000] = "";

		//while oraz vector aby przechowywac string z requestem podzielonym na czesci
		descriptor = recv(acceptDescrpt, &recvData, sizeof(recvData), 0);

		if(descriptor > 0) {
			try {
				if (sizeof(recvData) > 8000) 
					response = "HTTP/1.1 413 PAYLOAD TOO LARGE\nContent-Type: text/plain\nContent-Length: 18\n\nPayload too large!\n\n";
				else {
					cout << recvData << endl;
					string filePath = processRequest(recvData, connReq);
					response = filePath != "" ? 
						getResponse(getFileExtension(filePath), readFile(filePath.c_str())) : 
						"HTTP/1.1 502 NOT IMPLEMENTED\nContent-Type: text/plain\nContent-Length: 23\n\nRequest type handler not implemented.\n\n";
				}
			} catch(exception e) {
				response = "HTTP/1.1 404 NOT FOUND\nContent-Type: text/plain\nContent-Length: 15\n\nFile not found!\n\n";
			}

			descriptor = send(acceptDescrpt, response.c_str(), strlen(response.c_str()), 0);
			if (descriptor < 0) break;
		} else break;
	}
}

void initOpenssl()
{ 
    SSL_load_error_strings();	
    OpenSSL_add_ssl_algorithms();
}

void cleanupOpenssl()
{
    EVP_cleanup();
}

SSL_CTX *createContext()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
    }

    return ctx;
}

void configureContext(SSL_CTX *ctx)
{
    SSL_CTX_set_ecdh_auto(ctx, 1);

    // Set the key and cert
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
    }
}*/

#endif
