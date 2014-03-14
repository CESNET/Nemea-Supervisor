#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define TRUE 				1 ///< Bool true
#define FALSE 				0 ///< Bool false
#define DAEMON_UNIX_PATH_FILENAME_FORMAT	"/tmp/supervisor_daemon.sock"

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

int connect_to_supervisor()
{
	int sockfd;
	union tcpip_socket_addr addr;

	memset(&addr, 0, sizeof(addr));

	addr.unix_addr.sun_family = AF_UNIX;
	snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, DAEMON_UNIX_PATH_FILENAME_FORMAT);
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (connect(sockfd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) == -1) {
		return -1;
	}
	return sockfd;
}

int main ()
{
	int connected = FALSE;
	int ret_val;
	int supervisor_sd;
	char buffer[100];
	int command;


	supervisor_sd = connect_to_supervisor();
	if(supervisor_sd < 0){
		printf("Could not connect to supervisor.\n");
		return 0;
	} else {
		connected = TRUE;
	}

	while(connected){

		printf("--------OPTIONS--------\n");
		printf("0. SET VERBOSE LEVEL\n");
		printf("1. RUN CONFIGURATION\n");
		printf("2. STOP CONFIGURATION\n");
		printf("3. START MODUL\n");
		printf("4. STOP MODUL\n");
		printf("5. SET MODULE ENABLED\n");
		printf("6. STARTED MODULES STATUS\n");
		printf("7. AVAILABLE MODULES\n");
		printf("8. SHOW GRAPH\n");
		printf("9. RUN TEMP CONF\n");
		printf("10. QUIT\n");



		printf("Type in number of command:\n");
		if(scanf("%d",&command) == 0){
			printf("Wrong input.\n");
			continue;
		}
		if(command < 0 || command > 10){
			printf("Wrong input.\n");
			continue;
		}

		printf("Sending request> ");
		ret_val = send(supervisor_sd,&command,sizeof(command),0);
		if(ret_val == 0){
			printf("Send returned 0\n");
			connected = FALSE;
			break;
		} else if (ret_val == -1){
			printf("Error while sending.\n");
			if (errno == ENOTCONN) {
				printf("errno ENOTCONN\n");
			}
			connected = FALSE;
			break;
		}

		ret_val = recv(supervisor_sd,buffer,50,0);
		if(ret_val == 0){
			printf("Supervisor has disconnected.\n");
			connected = FALSE;
			break;
		} else if (ret_val == -1){
			printf("Error while recving.\n");
			connected = FALSE;
			break;
		}
		printf("%s\n", buffer);

		if(command == 10){
			connected = FALSE;
		}

	}


	close(supervisor_sd);
	return 0;
}