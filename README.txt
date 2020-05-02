IRC PROJECT 19/20

COMPILER SETTINGS:

	SERVER: gcc -Wall server.c -o server
	CLIENTE: gcc -Wall client.c -o client
	PROXY: gcc -Wall proxy.c -o proxy

RUNNING SETTING:

	SERVER: ./server <port> <MaxClients>
	CLIENTE:./client <proxyIP> <serverIP> <Port> <Protocol>
	PROXY: ./porxy <Port>

EXAMPLE:

	SERVER: ./server 9000 10
	CLIENTE: ./client 127.0.0.1 192.168.1.12 9000 UDP
	PROXY: ./proxy 9000


Thanks for reading this carefully.
