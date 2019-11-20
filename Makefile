all:
	g++ server.cpp -o exe -Lopenssl/openssl-1.1.0/ -lpthread -lssl -lcrypto -Iopenssl/openssl-1.1.0/include

clean:
	rm -f exe
	
run:
	./exe
