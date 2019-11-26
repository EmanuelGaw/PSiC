#ifndef UTILS_H_
#define UTILS_H_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <regex>

using namespace std;

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
	if (std::regex_search(request, match, connectPattern) && match.size() > 1) {
		conn = true;
		return match.str(1);
  	} else if (std::regex_search(request, match, reqPattern) && match.size() > 1) {
  		return request;
  	} else {
		return "";
  	}
}

string handleRequest(string request, bool conn) {
	regex connectPattern("CONNECT (.*) HTTP/1.[0-9]+");
	regex reqPattern("(.*) HTTP/1.[0-9]+");
	smatch match;
	conn = false;
	if (std::regex_search(request, match, connectPattern) && match.size() > 1) {
		conn = true;
		return match.str(1);
  	} else if (std::regex_search(request, match, reqPattern) && match.size() > 1) {
  		return request;
  	} else {
		return "";
  	}
}

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

void handleTlsConnection(SSL * ssl) {
	bool connReq = false;
	int descriptor = 0;
	string response;
	while(1) {
		char recvData[1024] = "";

		do {
			descriptor = SSL_read(ssl, &recvData, sizeof(recvData));
		} while(SSL_pending(ssl) && descriptor != -1);
		
		if(descriptor > 0) {

			try {
				string filePath = processRequest(recvData, connReq);
				response = filePath != "" ? 
					getResponse(getFileExtension(filePath), readFile(filePath.c_str())) : 
					"HTTP/1.1 502 NOT IMPLEMENTED\nContent-Type: text/plain\nContent-Length: 23\n\nRequest type handler not implemented.\n\n";
			} catch(exception e) {
				response = "HTTP/1.1 404 NOT FOUND\nContent-Type: text/plain\nContent-Length: 15\n\nFile not found!\n\n";
			}
			
			cout << recvData << endl;
			descriptor = SSL_write(ssl, response.c_str(), strlen(response.c_str()));
			if (descriptor < 0) break;
		} else break;
	}
}

/////////////////////////////////////////////////////////////

void createConnection(string address, int port) {
	
}

/////////////////////////////////////////////////////////////

string getTime(const char* format) {
  time_t rawtime;
  struct tm* timeinfo;
  char output[40];

  time (&rawtime);
  timeinfo = localtime (&rawtime);

  stringstream formatString;
  formatString << "Server time: "<< format;
  strftime (output, sizeof(output), formatString.str().data(), timeinfo);

  return output;
}

pollfd preparePollfd(int fd, short events, short revents = 0) {
	pollfd pfd = {};
	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = revents;
	return pfd;
}

////////////////////////////////////////////////////////////////

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

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
    }
}

#endif

/*
descriptor = poll(fds.data(), fds.size(), -1); //&fds[0]
		if (descriptor == -1) {
			cout << "Problem z pollem: " << strerror(errno) << endl; 
			return 0;
		} else if (fds[0].revents & POLLIN) {
fds.push_back(preparePollfd(acceptDescrpt, POLLIN));
*/
//fds.push_back(preparePollfd(socketDescrpt, POLLIN));

	/*descriptor = fcntl(socketDescrpt, F_GETFL);
	descriptor = fcntl(socketDescrpt, F_SETFL, descriptor); //& O_NONBLOCK
	if (descriptor == -1) {
		cout << "Problem z wykonaniem zadanej funkcji na gniezdzie: " << strerror(errno) << endl;
		descriptor = close(socketDescrpt);
		return 0;
	}*/

//else if (!(strncmp("GET", header, 3) && strncmp("POST", header, 4))) handleNormalConnection(acceptDescrpt);
//else cout << "HTTP/1.1 400 BAD REQUEST\nContent-Type: text/plain\nContent-Length: 23\n\nBad or unknown request.\n\n";