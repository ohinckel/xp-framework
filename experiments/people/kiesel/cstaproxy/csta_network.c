#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>


#include "cstaproxy.h"
#include "csta_error.h"
#include "csta_network.h"
#include "csta_connection.h"

int create_listening_socket (char *addr, int port) {
	int hSocket;
	struct sockaddr_in my_addr;
	int yes;
	
	hSocket = socket(AF_INET, SOCK_STREAM, 0);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(MYPORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(my_addr.sin_zero), '\0', 8);
	
	if (-1 == setsockopt (hSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int))) {
		ERR("setsockopt failed");
		return -1;
	}
	
	if (-1 == bind(hSocket, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))) {
		ERR("bind failed");
		return -1;
	}
	
	if (-1 == listen (hSocket, 10)) {
		ERR("listen failed");
		return -1;
	}
	
	LOG("Listener created.");
	return hSocket;
}

int create_proxy_connection(fd_set *master, connection_context *ctx, int hListen) {
	struct sockaddr_in remoteaddr;
	proxy_connection *conn;
	int addrlen;
	int newfd;
	
	addrlen= sizeof (remoteaddr);
	if (-1 == (newfd= accept(hListen, (struct sockaddr *)&remoteaddr, &addrlen))) {
		ERR("Could not accept new connection");
		return -1;
	}
	
	alloc_connection(&conn);
	conn->hClient= newfd;
	add_connection(ctx, conn);

	/* Add socket to fdset */
	FD_SET (newfd, master);
	
	_dump_context (ctx);
	
	LOG("New client connected");
	return newfd;
}

int _open_server_connection(fd_set *master, proxy_connection *conn) {
	return 1;
}

int _process_client_command(fd_set *master, connection_context *ctx, int hSocket, char *buf, int length) {
	proxy_connection *conn;
	
	conn= get_connection_by_socket (ctx, hSocket);

	/* Process shutdown request */
	if (0 == strncmp(buf, "QUIT", 4)) {
		/* Close open sockets */
		if (NULL != conn->hServer) {
			close (conn->hServer);
			FD_CLR(conn->hServer, master);
		}
		if (NULL != conn->hClient) {
			close (conn->hClient);
			FD_CLR(conn->hClient, master);
		}
		
		/* Cleanup */
		delete_connection(ctx, conn);
		free_connection(&conn);
		
		return -1;	/* Indicate we have closed the sockets */
	}
	
	/* Process authenticate request */
	if (0 == strncmp (buf, "AUTHENTICATE", 12)) {
		if (!conn->is_authenticated) {
			conn->is_authenticated= 1;
			LOG ("Authenticating client");
			_open_server_connection (master, conn);
		}
		return 1;
	}
	
	return 0;
}

int process_client(fd_set *master, connection_context *ctx, int hSocket) {
	char buf[256];
	int nbytes;
	proxy_connection *conn;
	
	memset (&buf, '\0', sizeof (buf));
	
	/* Handle data from client */
	if (0 >= (nbytes= recv (hSocket, buf, sizeof (buf), 0))) {
		if (0 == nbytes) {
			log (__FILE__, __LINE__, "Client %d closed connection.", hSocket);
		} else {
			ERR ("Error reading from socket");
		}
		
		if (NULL == (conn= get_connection_by_socket (ctx, hSocket))) {
			ERR ("Unable to find proxy connection in context");
		} else {
			delete_connection(ctx, conn);
			free_connection(&conn);
		}
		
		_dump_context (ctx);
		
		close (hSocket);
		FD_CLR(hSocket, master);
	} else {
	
		if (NULL == (conn= get_connection_by_socket (ctx, hSocket))) {
			ERR("Unable to find connection in context!");
			return 0;
		}

		/* First stage: process unauthenticated */
		if (hSocket == conn->hClient) {
			/* -1 means: connections have been shutdown */
			if (-1 == _process_client_command(master, ctx, hSocket, buf, nbytes)) {
				return 1;
			}
		}
		
		/* Client has sent data, so process */
		if (hSocket == conn->hClient && conn->is_authenticated && conn->hServer) {
			/* This is an authenticated client => pass data through */
			send (conn->hServer, buf, nbytes, 0);
		}
		
		if (hSocket == conn->hClient) {
			/* Not authenticated! */
			send (conn->hClient, "-ERR Not authenticated!\n", 24, 0);
		}
		
		if (hSocket == conn->hServer) {
			/* It's the server, passthrough data */
			send (conn->hClient, buf, nbytes, 0);
		}
	}
	
	return 1;
}

int select_loop(int hListen) {
	fd_set	master, read_fs;
	int fdMax, fdNew, i;
	int quit;
	connection_context *ctx;
	
	/* Add listener to sockets and remember max socket */
	FD_SET (hListen, &master);
	fdMax= hListen;
	
	/* Initialize array of connections */
	alloc_connection_context (&ctx);
	
	quit= 0;
	while (!quit) {
		/* Make a copy */
		read_fs= master;
		
		if (-1 == select(fdMax+1, &read_fs, NULL, NULL, NULL)) {
			ERR("Error in select()");
			return -1;
		}
		
		for (i= 0; i <= fdMax; i++) {
			/* Check for active connections... */
			if (FD_ISSET (i, &read_fs)) {
				if (i == hListen) {
					/* New connection coming in */
					if (-1 != (fdNew= create_proxy_connection(&master, ctx, i))) {
						if (fdNew > fdMax) fdMax= fdNew;
					}
				} else {
					/* This is a normal client connection */
					
					process_client(&master, ctx, i);
				}
			}
		}
	}
	
	return 1;
}
