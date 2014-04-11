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
#include <sys/ioctl.h>

#define TRUE 				1 ///< Bool true
#define FALSE 				0 ///< Bool false
#define DAEMON_UNIX_PATH_FILENAME_FORMAT	"/tmp/supervisor_daemon.sock"

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

int connect_to_supervisor()
{
	printf("Connecting to Supervisor-daemon...\n");
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
	int x = 0;
	int connected = FALSE;
	int ret_val;
	int supervisor_sd = -1;
	FILE * supervisor_stream_input = NULL;
	FILE * supervisor_stream_output = NULL;
	char buffer[1000];
	memset(buffer,0,1000);
	int command;

	char testbuffer[100];
	memset(testbuffer,0,100);

	supervisor_sd = connect_to_supervisor();
	if(supervisor_sd < 0){
		printf("Could not connect to supervisor.\n");
		return 0;
	} else {
		printf("Connected !\n");
		connected = TRUE;
	}

	supervisor_stream_input = fdopen(supervisor_sd, "r");
	if(supervisor_stream_input == NULL) {
		printf("1Fdopen error\n");
		close(supervisor_sd);
		return;
	}

	supervisor_stream_output = fdopen(supervisor_sd, "w");
	if(supervisor_stream_output == NULL) {
		printf("1Fdopen error\n");
		close(supervisor_sd);
		return;
	}

	int supervisor_stream_input_fd = fileno(supervisor_stream_input);
	if(supervisor_stream_input_fd < 0) {
		printf("2Fdopen error\n");
		close(supervisor_sd);
		return;
	}

	fd_set read_fds;
	struct timeval tv;

	while(connected){

		FD_ZERO(&read_fds);
		FD_SET(supervisor_stream_input_fd, &read_fds);
		FD_SET(0, &read_fds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		ret_val = select(supervisor_stream_input_fd+1, &read_fds, NULL, NULL, &tv);

		if (ret_val == -1) {
			perror("select()");
			close(supervisor_stream_input_fd);
			fclose(supervisor_stream_input);
			fclose(supervisor_stream_output);
			close(supervisor_sd);
			connected = FALSE;
			break;
		} else if (ret_val) {
			if (FD_ISSET(0, &read_fds)) {
				scanf("%s", buffer);
				if (strcmp(buffer,"quit") == 0) {
					close(supervisor_stream_input_fd);
					fclose(supervisor_stream_input);
					fclose(supervisor_stream_output);
					close(supervisor_sd);
					connected = FALSE;
					break;
				}
				fprintf(supervisor_stream_output,"%s\n",buffer);
				fflush(supervisor_stream_output);
			}
			if (FD_ISSET(supervisor_stream_input_fd, &read_fds)) {
				usleep(200000);
				ioctl(supervisor_stream_input_fd, FIONREAD, &x);
				if (x == 0) {
					printf("Supervisor has disconnected, I'm done.\n");
					close(supervisor_sd);
					fclose(supervisor_stream_input);
					fclose(supervisor_stream_output);
					close(supervisor_stream_input_fd);
					connected = FALSE;
					break;
				} else {
					int y;
					for(y=0; y<x; y++) {
						printf("%c", (char) fgetc(supervisor_stream_input));
					}
				}
			}
		} else {
			// timeout, nothing to do
		}

		memset(buffer,0,1000);
		fsync(supervisor_stream_input_fd);
		fflush(supervisor_stream_input);
		fflush(stdout);
	}

	return 0;
}