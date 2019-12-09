#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
# Program testujący serwer pośredniczący. Tworzy tymczasowy serwer HTTP,
# tworzy prostego klienta HTTP, wykonuje połączenie przez serwer pośredniczący
# (adres w zmiennej 'PROXY_ADDRESS') i porównuje dane odebrane przez klienta
# z danymi wysłanymi przez serwer. Sprawdza podstawowe rzeczy typu poprawność
# ścieżki w zapytaniu wysłanym do serwera docelowego i przekazywanie nagłówków.
# Wykonuje dwa połączenia -- jedno z metodą GET, drugie z metodą CONNECT.
#
# Spodziewany wynik:
#
#   $ ./test-proxy.py
#   Metoda GET
#   Klient połączony z serwerem proxy
#   Serwer proxy połączony z serwerem docelowym
#   Wysłano 33554432 bajtów (MD5: 43526662e2efdccd149f39dc8754d088)
#   Odebrano 33554432 bajtów (MD5: 43526662e2efdccd149f39dc8754d088)
#   Metoda CONNECT
#   Klient połączony z serwerem proxy
#   Serwer proxy połączony z serwerem docelowym
#   Wysłano 33554432 bajtów (MD5: 43526662e2efdccd149f39dc8754d088)
#   Odebrano 33554432 bajtów (MD5: 43526662e2efdccd149f39dc8754d088)
#
# lub podobny zależnie od ustawienia zmiennej 'PROXY_MEGABYTES'.


from socket import *
import threading
import random
import time
import sys
import hashlib
import ssl

PROXY_ADDRESS = ('127.0.0.3', 8080)
PROXY_SSL = True
PROXY_MEGABYTES = 32

port = None
port_set_event = threading.Event()
test_path = "/what?ever=42"
test_header = "Whatever: 42"

def server_thread_func():
    global port, port_set_event
    server = socket(AF_INET, SOCK_STREAM)
    server.bind(('127.0.0.3', 0))
    port = server.getsockname()[1]
    port_set_event.set()
    server.listen(1)

    while True:
        client, addr = server.accept()
        print ("Serwer proxy połączony z serwerem docelowym")
        buf = ""
        while buf.find("\r\n\r\n") == -1:
            tmp = client.recv(4096)
            if not tmp:
                break
            buf += tmp
        if not buf.startswith("GET " + test_path + " HTTP/1"):
            print ("Klient wysłał nieprawidłowe zapytanie:\n" + buf)
            return
        if buf.find("\r\n" + test_header + "\r\n") == -1:
            print ("Klient nie przekazał nagłówka:\n" + buf)
            return
        client.send("HTTP/1.0 200 OK\r\nContent-Length: " + str(PROXY_MEGABYTES * 1048576) + "\r\n\r\n")
        sent = 0
        md5 = hashlib.md5()
        for i in range(PROXY_MEGABYTES):
            buf = chr(i) * 1048576
            client.send(buf)
            md5.update(buf)
            sent += len(buf)
        print ("Wysłano %d bajtów (MD5: %s)" % (sent, md5.hexdigest()))
        client.close()

def client_func(connect_method):
    client = socket(AF_INET, SOCK_STREAM)
    client.connect(PROXY_ADDRESS)
    if PROXY_SSL:
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        conn = context.wrap_socket(client)
    else:
        conn = client
    dest = "127.0.0.3:%d" % port
    print ("Klient połączony z serwerem proxy")
    headers = "Host: " + dest + "\r\nConnection: close\r\n" + test_header + "\r\n"
    if not connect_method:
        conn.sendall("GET http://" + dest + test_path + " HTTP/1.0\r\n" + headers + "\r\n")
    else:
        conn.sendall("CONNECT " + dest + " HTTP/1.0\r\n\r\n")
        buf = ""
        while buf.find("\r\n\r\n") == -1:
            tmp = conn.recv(4096)
            if not tmp:
                break
            buf += tmp
        conn.sendall("GET " + test_path + " HTTP/1.0\r\n" + headers + "\r\n")

    buf = ""
    last_megabyte = 0
    while True:
        tmp = conn.recv(1048576)
        sys.stdout.write("Transfer: [" + (len(buf) / 1048576) * "#" + (PROXY_MEGABYTES - len(buf) / 1048576) * "." + "]\r")
        sys.stdout.flush()
        if not tmp:
            break
        buf += tmp
        current_megabyte = len(buf) / 1048576
        if last_megabyte != current_megabyte:
            time.sleep(random.random())
            last_megabyte = current_megabyte

    idx = buf.find("\r\n\r\n")
    if idx == -1:
        print ("Nie odebrano nagłówka")
        return

    data = buf[idx + 4:]

    print ("Odebrano %d bajtów (MD5: %s)" % (len(data), hashlib.md5(data).hexdigest()))
    client.close()

server_thread = threading.Thread(target=server_thread_func)
server_thread.daemon = True
server_thread.start()

port_set_event.wait()

print ("Metoda GET")
client_func(False)
print ("Metoda CONNECT")
client_func(True)

