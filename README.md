# TCP Server-Client Application

This repository contains a basic TCP server-client application.

## Compilation

To compile both the server and client applications, run:

make

To run the server, use:
./server
By default, the server listens on port 1234.

To run the client, use:

./client <IP> <PORT> [cmd...]

To remove the compiled executables, run:
make clean

## Summary:
This application consists of two primary components: a TCP server and a TCP client.

### TCP Server (server.cpp):

The server listens on a specified port (default: 1234) for incoming client connections.
When a client connects, the server starts monitoring the connection for incoming messages.
For simplicity, messages are assumed to be delimited by newline characters.
Upon receiving a complete message, the server prepares an echo response.
The server is built to handle multiple simultaneous client connections using the poll system call.

### TCP Client (client.cpp):

The client connects to the server on a given IP address and port.
It sends a set of strings to the server as a request.
The request format includes the total message size, number of strings, and the strings themselves.
After sending a request, the client waits for a response from the server.
The response contains a status code and a message from the server. For this setup, the server simply echoes back the message.
Overall, the application demonstrates a basic interaction pattern between a TCP server and a client: the client sends requests, and the server processes and responds to those requests.
