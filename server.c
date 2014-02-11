#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFSIZE 1024
#define BACKLOG 5
#define CLIENTS 5
#define PORT 9034
#define IP_ADDRESS "127.0.0.1" 

int main(int argc, char ** argv){
	int sockfd, newfd, err_ret;
	struct sockaddr_in serv_addr, client_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		fprintf(stderr, "Creating socket failed");
		return errno;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);

	if( bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
		fprintf(stderr, "Error binding socket");
		return errno;
	}

	if( listen(sockfd, BACKLOG) == -1){
		fprintf(stderr, "Error listening");
		return errno;
	}

	printf("Starting to listen to socket.. \n");
	
	return 0;
}
