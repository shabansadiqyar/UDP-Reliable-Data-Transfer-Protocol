# UDP-Reliable-Data-Transfer-Protocol

The purpose of this project was to use UDP Sockets and C++ in order to implement a simple window-based, reliable data transfer protocol built on top of Selective Repeat Protocol. The client and server were required to communicate between each other using UDP which is not a reliable transfer protocol. Therefore I had to implement retransmission when packets were lost. The client side will run based on the server hostname, server port name and filename. The server will run based on only the port name which is bound to the socket and its job is to receive the file from its directory and transfer it to the client. In the case of the file being too large then it must be split into 1024 Byte chunks and sent to the client. 

## How to Run and Test

In order to compile the program, type
```
make
```
which will create the C object. The Makefile will have everything in order for make to work. One can also run 
```
g++ -std=c++0x server.cpp -o server
g++ -std=c++0x client.cpp -o client 
```
in order to compile the program. The server is ran by typing ./server 2500 on the command line . The port number is arbitrary but we specified it as 2500. The client is ran by typeing ./client localhost 2500 filename. The filename can be any file thats being requested by the client. The port number for the client must match the server. This will send the file from the server to the client in a received.txt file. 

Packet loss can be simulated using 
```
sudo tc qdisc add dev lo root netem loss 20% delay 100ms
```
This will create 20% loss and a 100ms delay upon retransmission. 
