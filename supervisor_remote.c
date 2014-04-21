#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/wait.h>
#include <limits.h>

#include "supervisor.h"

#define MAX_RESTARTS	1000  ///< Maximum number of module restarts
#define LOGSDIR_NAME 	"./modules_logs/" ///< Name of directory with logs
#define TRAP_PARAM 		"-i" ///< Interface parameter for libtrap

char ** make_module_arguments (const int number_of_module);

static void *get_in_addr(struct sockaddr *sa)
{
   if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
   }

   return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int 				service_thread_continue;
int 				connected;
int 				supervisor_remote_sd;
running_module_t **	running_modules;
int 				loaded_modules_cnt;
pthread_t * 		service_thread_id;

void init_daemon()
{
	pid_t process_id = 0;
	pid_t sid = 0;

	process_id = fork();
	if (process_id < 0)	{
		printf("fork failed!\n");
		exit(1);
	} else if (process_id > 0)	{
		printf("process_id of child process %d \n", process_id);
		exit(0);
	}

	umask(0);
	sid = setsid();
	if(sid < 0)	{
		exit(1);
	}

	// int fd_fd = open ("supervisor_remote_log", O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
	// dup2(fd_fd, 1);
	// dup2(fd_fd, 2);
	// close(fd_fd);
}

int connect_to_supervisor()
{	
	char dest_addr[100];
	memset(dest_addr,0,100);
	strcpy(dest_addr, "localhost");

	char dest_port[20];
	memset(dest_port,0,20);
	strcpy(dest_port, "11011");

	struct addrinfo addr;
	struct addrinfo *servinfo, *p = NULL;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&addr, 0, sizeof(addr));

	addr.ai_family = AF_INET;
	addr.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(dest_addr, dest_port, &addr, &servinfo)) != 0) {
		printf("Error getaddrinfo\n");
	   return -1;
	}

	printf("Try to connect");
	for (p = servinfo; p != NULL; p = p->ai_next) {
	   if ((supervisor_remote_sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
	      continue;
	   }

		if (connect(supervisor_remote_sd, p->ai_addr, p->ai_addrlen) == -1) {
		   printf("TCPIP ifc connect error %d (%s)", errno, strerror(errno));
		   close(supervisor_remote_sd);
		   continue;
		}
		break;
	}

	if (p != NULL) {
		inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
		printf("client: connected to %s", s);
	}
	connected = TRUE;
	fflush(stdout);
	freeaddrinfo(servinfo); // all done with this structure
	return 0;
}
/*******************************************/
void start_module (const int module_number)
{
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	char log_path_stdout[100];
	char log_path_stderr[100];
	// int x = 0;

	running_modules[module_number]->module_enabled = TRUE;
	running_modules[module_number]->module_restart_cnt = 0;
	running_modules[module_number]->module_service_sd = -1;
	running_modules[module_number]->remote_module = FALSE;

	// running_modules[module_number]->module_counters_array = (int *) calloc (3*running_modules[module_number]->module_num_out_ifc + running_modules[module_number]->module_num_in_ifc,sizeof(int));
	sprintf(log_path_stdout,"%s%d_%s_stdout_remote",LOGSDIR_NAME, module_number, running_modules[module_number]->module_name);
	sprintf(log_path_stderr,"%s%d_%s_stderr_remote",LOGSDIR_NAME, module_number, running_modules[module_number]->module_name);
	
	fflush(stdout);
	running_modules[module_number]->module_pid = fork();
	if (running_modules[module_number]->module_pid == 0) {
		int fd_stdout = open(log_path_stdout, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		int fd_stderr = open(log_path_stderr, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		dup2(fd_stdout,1); //stdout
		dup2(fd_stderr,2); //stderr
		close(fd_stdout);
		close(fd_stderr);
		char ** params = make_module_arguments(module_number);
		fprintf(stdout,"---> %s | %s   %s   %s    %s\n", asctime (timeinfo), params[0], params[1], params[2], params[3]);
		fprintf(stderr,"---> %s | %s   %s   %s    %s\n", asctime (timeinfo), params[0], params[1], params[2], params[3]);
		fflush(stdout);
		execvp(running_modules[module_number]->module_path, params);
		printf("Error while executing module binary.\n");
		running_modules[module_number]->module_enabled = FALSE;
		exit(1);
	} else if (running_modules[module_number]->module_pid == -1) {
		running_modules[module_number]->module_status = FALSE;
		printf("Error in fork.\n");
	} else {
		running_modules[module_number]->module_status = TRUE;
	}
}

void restart_module (const int module_number)
{
	if(running_modules[module_number]->module_running == FALSE){
		start_module(module_number);
		return;
	}
	printf("Restarting module %d_%s\n", module_number, running_modules[module_number]->module_name);
	char log_path_stdout[100];
	char log_path_stderr[100];
	sprintf(log_path_stdout,"%s%d_%s_stdout_remote",LOGSDIR_NAME, module_number, running_modules[module_number]->module_name);
	sprintf(log_path_stderr,"%s%d_%s_stderr_remote",LOGSDIR_NAME, module_number, running_modules[module_number]->module_name);

	// memset(running_modules[module_number]->module_counters_array,0,(running_modules[module_number]->module_num_in_ifc + (3*running_modules[module_number]->module_num_out_ifc)) * sizeof(int));
	running_modules[module_number]->last_cpu_usage_user_mode = 0;
	running_modules[module_number]->last_cpu_usage_kernel_mode = 0;
	running_modules[module_number]->module_service_sd = -1;

	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	fflush(stdout);
	printf("pred forkem\n");
	running_modules[module_number]->module_pid = fork();
	if (running_modules[module_number]->module_pid == 0) {
		int fd_stdout = open(log_path_stdout, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		int fd_stderr = open(log_path_stderr, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		dup2(fd_stdout,1); //stdout
		dup2(fd_stderr,2); //stderr
		close(fd_stdout);
		close(fd_stderr);
		char ** params = make_module_arguments(module_number);
		fprintf(stdout,"---> %s | %s   %s   %s    %s\n", asctime (timeinfo), params[0], params[1], params[2], params[3]);
		fprintf(stderr,"---> %s | %s   %s   %s    %s\n", asctime (timeinfo), params[0], params[1], params[2], params[3]);
		fflush(stdout);
		execvp(running_modules[module_number]->module_path, params);
		printf("Error while executing module binary.\n");
		running_modules[module_number]->module_enabled = FALSE;
		exit(1);
	} else if (running_modules[module_number]->module_pid == -1) {
		running_modules[module_number]->module_status = FALSE;
		running_modules[module_number]->module_restart_cnt++;
		printf("Err Fork()\n");
	} else {
		running_modules[module_number]->module_status = TRUE;
		running_modules[module_number]->module_restart_cnt++;
	}
}

void restart_modules()
{
	int x = 0;
	for (x=0; x<loaded_modules_cnt; x++) {
		if (running_modules[x]->module_status == FALSE && (running_modules[x]->module_restart_cnt >= MAX_RESTARTS)) {
			printf("Module: %d_%s was restarted %d times and it is down again.\n",x, running_modules[x]->module_name, MAX_RESTARTS);
		} else if (running_modules[x]->module_status == FALSE && running_modules[x]->module_enabled == TRUE) {
			restart_module(x);
		}
	}
}

void stop_module (const int module_number)
{
	if(running_modules[module_number]->module_status) {
		printf("Stopping module %d_%s.\n", module_number, running_modules[module_number]->module_name);
		running_modules[module_number]->module_enabled = FALSE;
		kill(running_modules[module_number]->module_pid,2);
		running_modules[module_number]->module_status = FALSE;
		running_modules[module_number]->module_service_ifc_isconnected = FALSE;
	}
}

void update_module_status ()
{
	int x;
	int status;
	pid_t result;

	for (x=0; x<loaded_modules_cnt; x++) {
		if(running_modules[x]->module_running){
			result = waitpid(running_modules[x]->module_pid , &status, WNOHANG);
			if (result == 0) {
			  // Child still alive
			} else if (result == -1) {
			  // Error
				if(running_modules[x]->module_service_sd != -1) {
					close(running_modules[x]->module_service_sd);
					running_modules[x]->module_service_sd = -1;
				}
				running_modules[x]->module_status = FALSE;
				running_modules[x]->module_service_ifc_isconnected = FALSE;
			} else {
			  // Child exited
				if(running_modules[x]->module_service_sd != -1) {
					close(running_modules[x]->module_service_sd);
					running_modules[x]->module_service_sd = -1;
				}
				running_modules[x]->module_status = FALSE;
				running_modules[x]->module_service_ifc_isconnected = FALSE;
			}
		}
	}
}


char ** make_module_arguments (const int number_of_module)
{
	char atr[1000]; // TODO
	memset(atr,0,1000); // TODO
	int x = 0,y=0;
	char * ptr = atr;
	int str_len;
	fflush(stdout);
	for(x=0; x<running_modules[number_of_module]->module_ifces_cnt; x++) {
		if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_direction, "IN", 2)) {
			if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_type, "TCP", 3)) {
				strncpy(ptr,"t",1);
				ptr++;
			} else if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_type, "UNIXSOCKET", 10)) {
				strncpy(ptr,"u",1);
				ptr++;
			} else {
				printf("%s\n", running_modules[number_of_module]->module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
	}

	for(x=0; x<running_modules[number_of_module]->module_ifces_cnt; x++) {
		if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_direction,"OUT", 3)) {
			if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_type, "TCP", 3)) {
				strncpy(ptr,"t",1);
				ptr++;
			} else if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_type, "UNIXSOCKET", 10)) {
				strncpy(ptr,"u",1);
				ptr++;
			} else {
				printf("%s\n", running_modules[number_of_module]->module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
	}

	for(x=0; x<running_modules[number_of_module]->module_ifces_cnt; x++) {
		if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_direction,"SERVICE", 7)) {
			if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_type, "TCP", 3)) {
				strncpy(ptr,"s",1);
				ptr++;
			} else if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_type, "UNIXSOCKET", 10)) {
				strncpy(ptr,"s",1);
				ptr++;
			} else {
				printf("%s\n", running_modules[number_of_module]->module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
	}

	strncpy(ptr,";",1);
	ptr++;
	fflush(stdout);
	for(x=0; x<running_modules[number_of_module]->module_ifces_cnt; x++) {
		if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_direction,"IN", 2)) {
			sprintf(ptr,"%s;",running_modules[number_of_module]->module_ifces[x].ifc_params);
			ptr += strlen(running_modules[number_of_module]->module_ifces[x].ifc_params) + 1;
		}
	}

	for(x=0; x<running_modules[number_of_module]->module_ifces_cnt; x++) {
		if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_direction,"OUT", 3)) {
			sprintf(ptr,"%s;",running_modules[number_of_module]->module_ifces[x].ifc_params);
			ptr += strlen(running_modules[number_of_module]->module_ifces[x].ifc_params) + 1;
		}
	}

	for(x=0; x<running_modules[number_of_module]->module_ifces_cnt; x++) {
		if (!strncmp(running_modules[number_of_module]->module_ifces[x].ifc_direction,"SERVICE", 7)) {
			sprintf(ptr,"%s;",running_modules[number_of_module]->module_ifces[x].ifc_params);
			ptr += strlen(running_modules[number_of_module]->module_ifces[x].ifc_params) + 1;
		}
	}
	memset(ptr-1,0,1);
	fflush(stdout);
	char ** params = NULL;
	if (running_modules[number_of_module]->module_params == NULL) {
		params = (char **) calloc (4,sizeof(char*));
		str_len = strlen(running_modules[number_of_module]->module_name);
		params[0] = (char *) calloc (str_len+1, sizeof(char)); 	// binary name for exec
		strncpy(params[0],running_modules[number_of_module]->module_name, str_len+1);
		str_len = strlen(TRAP_PARAM);
		params[1] = (char *) calloc (str_len+1,sizeof(char)); 	// libtrap param "-i"
		strncpy(params[1],TRAP_PARAM,str_len+1);
		str_len = strlen(atr);
		params[2] = (char *) calloc (str_len+1,sizeof(char)); // atributes for "-i" param
		strncpy(params[2],atr,str_len+1);

		params[3] = NULL;
		fflush(stdout);
	} else {

		int params_counter;
		char buffer[100];
		int num_module_params = 0;
		int module_params_length = strlen(running_modules[number_of_module]->module_params);
		for(x=0; x<module_params_length; x++) {
			if(running_modules[number_of_module]->module_params[x] == 32) {
				num_module_params++;
			}
		}
		num_module_params++;

		params = (char **) calloc (4+num_module_params,sizeof(char*));
		str_len = strlen(running_modules[number_of_module]->module_name);
		params[0] = (char *) calloc (str_len+1, sizeof(char)); 	// binary name for exec
		strncpy(params[0],running_modules[number_of_module]->module_name, str_len+1);
		str_len = strlen(TRAP_PARAM);
		params[1] = (char *) calloc (str_len+1,sizeof(char)); 	// libtrap param "-i"
		strncpy(params[1],TRAP_PARAM,str_len+1);
		str_len = strlen(atr);
		params[2] = (char *) calloc (str_len+1,sizeof(char)); // atributes for "-i" param
		strncpy(params[2],atr,str_len+1);

		params_counter = 3;

		y=0;
		memset(buffer,0,100);
		for(x=0; x<module_params_length; x++) {
			if(running_modules[number_of_module]->module_params[x] == 32) {
				params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
				sprintf(params[params_counter],"%s",buffer);
				params_counter++;
				memset(buffer,0,100);
				y=0;
			} else {
				buffer[y] = running_modules[number_of_module]->module_params[x];
				y++;
			}
		}
		params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
		sprintf(params[params_counter],"%s",buffer);
		params_counter++;


		params[params_counter] = NULL;
	}
	fflush(stdout);
	return params;
}

void * service_thread_routine(void* arg)
{
	while(service_thread_continue == TRUE)	{
		update_module_status();
		restart_modules();
		sleep(2);
	}
	pthread_exit(NULL);
}

void start_service_thread()
{
	service_thread_id = (pthread_t *) calloc (1,sizeof(pthread_t));
	pthread_create(service_thread_id,NULL,service_thread_routine, NULL);
}
/******************************************/

void remote_start_module (int sizeofrecv)
{
	int str_len = 0;
	int sizeof_recv = 0;
	int total_receved = 0;
	int last_receved = 0;
	char * ptr = NULL;
	running_module_t * recived_running_module = (running_module_t *)calloc(1,sizeof(running_module_t));;

	char buffer[2000];
	memset(buffer,0,2000);
	int ret_val;
	int x, y;
	
	sizeof_recv = sizeof(running_module_t);
	total_receved = 0;
	last_receved = 0;

	while(total_receved < sizeof_recv) { 
		last_receved = recv(supervisor_remote_sd, recived_running_module + total_receved, sizeof_recv - total_receved, 0);
		if(last_receved == 0) {
			printf("! Modules service thread closed its socket, im done !\n");
			return;
		} else if (last_receved == -1) {
			printf("! Error while recving\n");
			return;
		}
		total_receved += last_receved;
	}

	recived_running_module->module_ifces = (interface_t *)calloc(recived_running_module->module_ifces_cnt, sizeof(interface_t));

	sizeof_recv = sizeofrecv;
	total_receved = 0;
	last_receved = 0;
	printf("sizeof_recv> %d\n", sizeof_recv);

	while(total_receved < sizeof_recv) { 
		last_receved = recv(supervisor_remote_sd, buffer + total_receved, sizeof_recv - total_receved, 0);
		if(last_receved == 0) {
			printf("! Modules service thread closed its socket, im done !\n");
			return;
		} else if (last_receved == -1) {
			printf("! Error while recving\n");
			return;
		}
		total_receved += last_receved;
	}

	ptr = buffer;

	if(recived_running_module->module_params != NULL){
		str_len = strlen(ptr)+1;
		recived_running_module->module_params = (char *)calloc(str_len, sizeof(char));
		for(x=0;x<str_len;x++){
			sscanf(ptr,"%c",recived_running_module->module_params + x);
			ptr++;
		}
	}

	str_len = strlen(ptr)+1;
	recived_running_module->module_name = (char *)calloc(str_len, sizeof(char));
	sscanf(ptr,"%s",recived_running_module->module_name);
	ptr+=str_len;


	str_len = strlen(ptr)+1;
	recived_running_module->module_path = (char *)calloc(str_len, sizeof(char));
	sscanf(ptr,"%s",recived_running_module->module_path);
	ptr+=str_len;

	for(x=0; x<recived_running_module->module_ifces_cnt; x++){
		str_len = strlen(ptr)+1;
		recived_running_module->module_ifces[x].ifc_type = (char *)calloc(str_len, sizeof(char));
		sscanf(ptr,"%s",recived_running_module->module_ifces[x].ifc_type);
		ptr+=str_len;

		str_len = strlen(ptr)+1;
		recived_running_module->module_ifces[x].ifc_params = (char *)calloc(str_len, sizeof(char));
		sscanf(ptr,"%s",recived_running_module->module_ifces[x].ifc_params);
		ptr+=str_len;

		str_len = strlen(ptr)+1;
		recived_running_module->module_ifces[x].ifc_direction = (char *)calloc(str_len, sizeof(char));
		sscanf(ptr,"%s",recived_running_module->module_ifces[x].ifc_direction);
		ptr+=str_len;
	}


	printf("%d_%s:  PATH:%s   PARAMS:%s\n", x, recived_running_module->module_name, recived_running_module->module_path, recived_running_module->module_params);

	for(x=0; x<recived_running_module->module_ifces_cnt; x++) {
		printf("\tIFC%d\tNOTE: %s\n",x, recived_running_module->module_ifces[x].ifc_note);
		printf("\t\tTYPE: %s\n", recived_running_module->module_ifces[x].ifc_type);
		printf("\t\tDIRECTION: %s\n", recived_running_module->module_ifces[x].ifc_direction);
		printf("\t\tPARAMS: %s\n", recived_running_module->module_ifces[x].ifc_params);
	}

		

	running_modules[loaded_modules_cnt] = recived_running_module;
	loaded_modules_cnt++;

	sleep(1);
	start_module(loaded_modules_cnt-1);
}

void remote_restart_module(int sizeofrecv)
{
	int str_len = 0;
	int sizeof_recv = 0;
	int total_receved = 0;
	int last_receved = 0;
	char * ptr = NULL;
	running_module_t * recived_running_module = (running_module_t *)calloc(1,sizeof(running_module_t));

	char buffer[2000];
	memset(buffer,0,2000);
	int ret_val;
	int x, y;

	sizeof_recv = sizeof(running_module_t);
	total_receved = 0;
	last_receved = 0;

	while(total_receved < sizeof_recv) { 
		last_receved = recv(supervisor_remote_sd, recived_running_module + total_receved, sizeof_recv - total_receved, 0);
		if(last_receved == 0) {
			printf("! Modules service thread closed its socket, im done !\n");
			return;
		} else if (last_receved == -1) {
			printf("! Error while recving\n");
			return;
		}
		total_receved += last_receved;
	}

	recived_running_module->module_ifces = (interface_t *)calloc(recived_running_module->module_ifces_cnt, sizeof(interface_t));

	sizeof_recv = sizeofrecv;
	total_receved = 0;
	last_receved = 0;
	printf("sizeof_recv> %d\n", sizeof_recv);

	while(total_receved < sizeof_recv) { 
		last_receved = recv(supervisor_remote_sd, buffer + total_receved, sizeof_recv - total_receved, 0);
		if(last_receved == 0) {
			printf("! Modules service thread closed its socket, im done !\n");
			return;
		} else if (last_receved == -1) {
			printf("! Error while recving\n");
			return;
		}
		total_receved += last_receved;
	}

	ptr = buffer;

	if(recived_running_module->module_params != NULL){
		str_len = strlen(ptr)+1;
		recived_running_module->module_params = (char *)calloc(str_len, sizeof(char));
		for(x=0;x<str_len;x++){
			sscanf(ptr,"%c",recived_running_module->module_params + x);
			ptr++;
		}
	}

	str_len = strlen(ptr)+1;
	recived_running_module->module_name = (char *)calloc(str_len, sizeof(char));
	sscanf(ptr,"%s",recived_running_module->module_name);
	ptr+=str_len;

	str_len = strlen(ptr)+1;
	recived_running_module->module_path = (char *)calloc(str_len, sizeof(char));
	sscanf(ptr,"%s",recived_running_module->module_path);
	ptr+=str_len;

	for(x=0; x<recived_running_module->module_ifces_cnt; x++){
		str_len = strlen(ptr)+1;
		recived_running_module->module_ifces[x].ifc_type = (char *)calloc(str_len, sizeof(char));
		sscanf(ptr,"%s",recived_running_module->module_ifces[x].ifc_type);
		ptr+=str_len;

		str_len = strlen(ptr)+1;
		recived_running_module->module_ifces[x].ifc_params = (char *)calloc(str_len, sizeof(char));
		sscanf(ptr,"%s",recived_running_module->module_ifces[x].ifc_params);
		ptr+=str_len;

		str_len = strlen(ptr)+1;
		recived_running_module->module_ifces[x].ifc_direction = (char *)calloc(str_len, sizeof(char));
		sscanf(ptr,"%s",recived_running_module->module_ifces[x].ifc_direction);
		ptr+=str_len;
	}


	printf("%d_%s:  PATH:%s   PARAMS:%s\n", x, recived_running_module->module_name, recived_running_module->module_path, recived_running_module->module_params);

	for(x=0; x<recived_running_module->module_ifces_cnt; x++) {
		printf("\tIFC%d\tNOTE: %s\n",x, recived_running_module->module_ifces[x].ifc_note);
		printf("\t\tTYPE: %s\n", recived_running_module->module_ifces[x].ifc_type);
		printf("\t\tDIRECTION: %s\n", recived_running_module->module_ifces[x].ifc_direction);
		printf("\t\tPARAMS: %s\n", recived_running_module->module_ifces[x].ifc_params);
	}

	for(x=0; x<loaded_modules_cnt; x++){
		if(running_modules[x]->module_number == recived_running_module->module_number){
			for(y=0; y<running_modules[x]->module_ifces_cnt; y++){
				if(running_modules[x]->module_ifces[y].ifc_type != NULL){
					free(running_modules[x]->module_ifces[y].ifc_type);
				}
				if(running_modules[x]->module_ifces[y].ifc_note != NULL){
					free(running_modules[x]->module_ifces[y].ifc_note);
				}
				if(running_modules[x]->module_ifces[y].ifc_params != NULL){
					free(running_modules[x]->module_ifces[y].ifc_params);
				}
				if(running_modules[x]->module_ifces[y].ifc_direction != NULL){
					free(running_modules[x]->module_ifces[y].ifc_direction);
				}
			}
			free(running_modules[x]->module_ifces);
			running_modules[x]->module_ifces = recived_running_module->module_ifces;
			free(recived_running_module->module_path);
			free(recived_running_module->module_name);
			free(recived_running_module);
			stop_module(x);
			running_modules[x]->module_enabled = TRUE;
		}
	}
}

void remote_stop_module()
{

}

void remote_start_again()
{

}

int main ()
{
	service_thread_continue = TRUE;
	start_service_thread();
	connected = FALSE;
	int sizeof_recv = 0;
	int total_receved = 0;
	int last_receved = 0;
	remote_info_t command;
	char * ptr = NULL;
	loaded_modules_cnt = 0;
	running_module_t * recived_running_module = NULL;
	running_modules = (running_module_t **)calloc(10,sizeof(running_module_t *));

	char buffer[2000];
	memset(buffer,0,2000);
	int ret_val;
	int x, y;
	//init daemon + redirect output
	// init_daemon();

	//connect
	connect_to_supervisor();
	printf("Finished\n");
	//recieve data
	while(connected){

		sizeof_recv = sizeof(remote_info_t);
		total_receved = 0;
		last_receved = 0;

		while(total_receved < sizeof_recv) { 
			last_receved = recv(supervisor_remote_sd, &command + total_receved, sizeof_recv - total_receved, 0);
			if(last_receved == 0) {
				printf("! Modules service thread closed its socket, im done !\n");
				connected = FALSE;
				break;
			} else if (last_receved == -1) {
				printf("! Error while recving\n");
				return 0;
			}
			total_receved += last_receved;
		}
		if(connected == FALSE){
			break;
		}

		switch(command.command_num){
			case 1:
				printf("COMMAND 1> STARTING MODULE\n");
				remote_start_module(command.size_of_recv);
				break;

			case 2:
				remote_restart_module(command.size_of_recv);
				break;

			case 3:
				remote_stop_module();
				break;

			case 4:
				remote_start_again();
				break;
		}
	}


	close(supervisor_remote_sd);
	return 0;
}