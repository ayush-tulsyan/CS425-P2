# CS425-P2
Project 2 of CS425A course

Problem Statement
Task is to build a web proxy capable of accepting HTTP requests, forwarding requests to
remote (origin) servers, and returning response data to a client. The proxy MUST
handle concurrent requests by forking a process for each new client request using
the fork() system call. You will only be responsible for implementing the GET method. All other
request methods received by the proxy should elicit a "Not Implemented" (501) error (see RFC
1945 section 9.5 - Server Error).

Launching the proxy server executable
First compile the files using the Makefile and then execute the binary. The Binary execution command must be passed the Port number as an arguement.

1. make
2. ./proxy \<Port number\>

Port numbers are limited to the range 0-65535. Usually port numbers above 5000 are available. It may seldom happen that the command 2 aborts with the message: "Error on Binding". In that case, probably the port asked by the user is already taken by some other process. Repeat the step 2 with a different port number.