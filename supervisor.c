/**
 * \file supervisor.c
 * \brief Supervisor implementation.
 * \author Marek Svepes <svepemar@fit.cvut.cz>
 * \date 2013
 */
/*
 * Copyright (C) 2013 CESNET
 *
 * LICENSE TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "supervisor.h"
#include "graph.h"

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

#define LOGSDIR_NAME 	"./modules_logs/" ///< Name of directory with logs
#define TRAP_PARAM 		"-i" ///< Interface parameter for libtrap
#define MAX_RESTARTS	1000  ///< Maximum number of module restarts

#define UNIX_PATH_FILENAME_FORMAT   "/tmp/trap-localhost-%s.sock"
#define DAEMON_UNIX_PATH_FILENAME_FORMAT	"/tmp/supervisor_daemon.sock"

 #define VERBOSE(format,args...) if (verbose_flag) { \
   printf(format,##args); \
}

/*******GLOBAL VARIABLES*******/
module_atr_t * 		modules;  ///< Information about modules from loaded xml config file
running_module_t * 	running_modules;  ///< Information about running modules

int		modules_cnt;  ///< Number of modules from loaded xml config file
int 	modules_array_size;
int 	running_modules_cnt;  ///< Number of running modules

pthread_mutex_t running_modules_lock; ///< mutex for locking counters
int service_thread_continue; ///< condition variable of main loop of the service_thread
int configuration_running; ///< if whole configuration was executed than ~ 1, else ~ 0

graph_node_t * graph_first_node;
graph_node_t * graph_last_node;

int verbose_level;

pthread_t * service_thread_id;

// supervisor flags
int 	help_flag; 			// -h argument
int 	file_flag; 			// -f "file" arguments
char  	config_file[100];
int 	show_cpu_usage_flag; // --show-cpuusage
int 	verbose_flag; 		// -v
int 	daemon_falg; 		// --daemon

/**********************/

union tcpip_socket_addr {
   struct addrinfo tcpip_addr; ///< used for TCPIP socket
   struct sockaddr_un unix_addr; ///< used for path of UNIX socket
};

void start_service_thread();
void stop_service_thread();
int parse_arguments(const int * argc, char ** argv);
void print_help();
void daemon_mode();

/**if choice TRUE -> parse file, else parse buffer*/
int load_configuration (const int choice, const char * buffer)
{
	xmlChar * key;
	int x,y;
	xmlDocPtr xml_tree;
	xmlNodePtr current_node;

	if(choice){
		xml_tree = xmlParseFile(buffer);
	} else {
		xml_tree = xmlParseMemory(buffer, strlen(buffer));
	}

	if (xml_tree == NULL ) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return FALSE;
	}
	
	current_node = xmlDocGetRootElement(xml_tree);
	
	if (current_node == NULL) {
		fprintf(stderr,"empty document\n");
		xmlFreeDoc(xml_tree);
		return FALSE;
	}
	
	if (xmlStrcmp(current_node->name, BAD_CAST "nemea-supervisor")) {
		fprintf(stderr,"document of the wrong type, root node != nemea-supervisor");
		xmlFreeDoc(xml_tree);
		return FALSE;
	}

	current_node = current_node->xmlChildrenNode;
	while (1) {
		if (!xmlStrcmp(current_node->name, BAD_CAST "modules")) {
			break;
		}
		current_node = current_node-> next;
		if(current_node == NULL){
			fprintf(stderr,"Tag \"modules\" wasnt found.\n");
			xmlFreeDoc(xml_tree);
			return FALSE;
		}
	}

	/*****************/

	xmlNodePtr module_ptr = current_node->xmlChildrenNode;
	xmlNodePtr module_atr, ifc_ptr, ifc_atr;
	int ifc_cnt = 0;

	while (module_ptr != NULL) {
		if (!xmlStrcmp(module_ptr->name,BAD_CAST "module"))	{

			//check allocated memory, if we dont have enough -> realloc
			if(modules_cnt == modules_array_size) {
				VERBOSE("REALLOCING MODULES ARRAY --->\n");
				modules_array_size += modules_array_size/2;
				modules = (module_atr_t * ) realloc (modules, (modules_array_size)*sizeof(module_atr_t));
				for(y=modules_cnt; y<modules_array_size; y++) {
					modules[y].module_ifces = (interface_t *)calloc(IFC_MAX, sizeof(interface_t));
					modules[y].module_running = FALSE;
				}
			}

			module_atr = module_ptr->xmlChildrenNode;

			while (module_atr != NULL) {
				if ((!xmlStrcmp(module_atr->name,BAD_CAST "params"))) {
					key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
					strcpy(modules[modules_cnt].module_params, (char *) key);
					xmlFree(key);
				} else if ((!xmlStrcmp(module_atr->name,BAD_CAST "name"))) {
					key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
					strcpy(modules[modules_cnt].module_name, (char *) key);
					xmlFree(key);
				} else if ((!xmlStrcmp(module_atr->name,BAD_CAST "path"))) {
					key = xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1);
					strcpy(modules[modules_cnt].module_path, (char *) key);
					xmlFree(key);
				} else if ((!xmlStrcmp(module_atr->name,BAD_CAST "trapinterfaces")))	{
					ifc_cnt=0;
					ifc_ptr = module_atr->xmlChildrenNode;

					while (ifc_ptr != NULL) {
						if (!xmlStrcmp(ifc_ptr->name,BAD_CAST "interface")) {
							ifc_atr = ifc_ptr->xmlChildrenNode;

							while (ifc_atr != NULL) {
								if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "note"))) {
									key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
									strcpy(modules[modules_cnt].module_ifces[ifc_cnt].ifc_note , (char *) key);
									xmlFree(key);
								} else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "type"))) {
									key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
									memset(modules[modules_cnt].module_ifces[ifc_cnt].ifc_type,0,PARAMS_MAX);
									strcpy(modules[modules_cnt].module_ifces[ifc_cnt].ifc_type, (char *) key);
									xmlFree(key);
								} else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "direction"))) {
									key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
									strcpy(modules[modules_cnt].module_ifces[ifc_cnt].ifc_direction, (char *) key);
									xmlFree(key);
								} else if ((!xmlStrcmp(ifc_atr->name,BAD_CAST "params"))) {
									key =xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1);
									strcpy(modules[modules_cnt].module_ifces[ifc_cnt].ifc_params, (char *) key);
									xmlFree(key);
								}
								ifc_atr=ifc_atr->next;
							}

							ifc_cnt++;
						}
						ifc_ptr = ifc_ptr->next;
					}

					sprintf(modules[modules_cnt].module_ifces[ifc_cnt].ifc_note,"%s","#");
				}

				module_atr = module_atr->next;
			}
			modules_cnt++;
		}
		module_ptr = module_ptr->next;
	}

	xmlFreeDoc(xml_tree);
	return TRUE;
}


void print_configuration ()
{
	int x,y = 0;
	printf("---PRINTING CONFIGURATION---\n");

	for (x=0; x < modules_cnt; x++) {
		printf("%d_%s:  PATH:%s  PARAMS:%s\n", x, modules[x].module_name, modules[x].module_path, modules[x].module_params);
		y=0;

		while (modules[x].module_ifces[y].ifc_note[0] != '#') {
			printf("\tIFC%d\tID: %s\n",y, modules[x].module_ifces[y].ifc_note);
			printf("\t\tTYPE: %s\n", modules[x].module_ifces[y].ifc_type);
			printf("\t\tDIRECTION: %s\n", modules[x].module_ifces[y].ifc_direction);
			printf("\t\tPARAMS: %s\n", modules[x].module_ifces[y].ifc_params);
			y++;
		}
	}
}

char ** make_module_arguments (const int number_of_module)
{
	char atr[PARAMS_MAX];
	memset(atr,0,PARAMS_MAX);
	int x = 0,y=0;
	char * ptr = atr;

	while (modules[number_of_module].module_ifces[x].ifc_note[0] != '#') {
		if (!strcmp(modules[number_of_module].module_ifces[x].ifc_direction, "IN")) {
			if (!strcmp(modules[number_of_module].module_ifces[x].ifc_type, "TCP")) {
				strcpy(ptr,"t");
				ptr++;
			} else if (!strcmp(modules[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET")) {
				strcpy(ptr,"u");
				ptr++;
			} else {
				printf("%s\n", modules[number_of_module].module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
		x++;
	}

	x=0;
	while (modules[number_of_module].module_ifces[x].ifc_note[0] != '#') {
		if (!strcmp(modules[number_of_module].module_ifces[x].ifc_direction,"OUT")) {
			if (!strcmp(modules[number_of_module].module_ifces[x].ifc_type, "TCP")) {
				strcpy(ptr,"t");
				ptr++;
			} else if (!strcmp(modules[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET")) {
				strcpy(ptr,"u");
				ptr++;
			} else {
				printf("%s\n", modules[number_of_module].module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
		x++;
	}

	x=0;
	while (modules[number_of_module].module_ifces[x].ifc_note[0] != '#') {
		if (!strcmp(modules[number_of_module].module_ifces[x].ifc_direction,"SERVICE")) {
			if (!strcmp(modules[number_of_module].module_ifces[x].ifc_type, "TCP")) {
				strcpy(ptr,"s");
				ptr++;
			} else if (!strcmp(modules[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET")) {
				strcpy(ptr,"s");
				ptr++;
			} else {
				printf("%s\n", modules[number_of_module].module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
		x++;
	}

	strcpy(ptr,";");
	ptr++;

	x=0;
	while (modules[number_of_module].module_ifces[x].ifc_note[0] != '#') {
		if (!strcmp(modules[number_of_module].module_ifces[x].ifc_direction,"IN")) {
			sprintf(ptr,"%s;",modules[number_of_module].module_ifces[x].ifc_params);
			ptr += strlen(modules[number_of_module].module_ifces[x].ifc_params) + 1;
		}
		x++;
	}

	x=0;
	while (modules[number_of_module].module_ifces[x].ifc_note[0] != '#') {
		if (!strcmp(modules[number_of_module].module_ifces[x].ifc_direction,"OUT")) {
			sprintf(ptr,"%s;",modules[number_of_module].module_ifces[x].ifc_params);
			ptr += strlen(modules[number_of_module].module_ifces[x].ifc_params) + 1;
		}
		x++;
	}

	x=0;
	while (modules[number_of_module].module_ifces[x].ifc_note[0] != '#') {
		if (!strcmp(modules[number_of_module].module_ifces[x].ifc_direction,"SERVICE")) {
			sprintf(ptr,"%s;",modules[number_of_module].module_ifces[x].ifc_params);
			ptr += strlen(modules[number_of_module].module_ifces[x].ifc_params) + 1;
		}
		x++;
	}
	memset(ptr-1,0,1);

	char ** params = NULL;
	if (strcmp(modules[number_of_module].module_params,"NULL") == 0) {

		params = (char **) calloc (5,sizeof(char*));
		params[0] = (char *) calloc (50,sizeof(char)); 	// binary name for exec
		params[1] = (char *) calloc (5,sizeof(char)); 	// libtrap param "-i"
		params[2] = (char *) calloc (100,sizeof(char)); // atributes for "-i" param
		// params[3] 									// input file for some modules etc.
		// params[4] 									// verbose level
		// params[5] = NULL								// NULL pointer for exec
		strcpy(params[0],modules[number_of_module].module_name);
		strcpy(params[1],TRAP_PARAM);
		strcpy(params[2],atr);


		switch(verbose_level){
			case 0:
				params[3] = NULL;
				break;
			case 1:
				params[3] = (char *) calloc (5,sizeof(char));
				sprintf(params[3],"-v");
				break;
			case 2:
				params[3] = (char *) calloc (5,sizeof(char));
				sprintf(params[3],"-vv");
				break;
			case 3:
				params[3] = (char *) calloc (5,sizeof(char));
				sprintf(params[3],"-vvv");
				break;
		}
		params[4] = NULL;
	} else {

		int params_counter;
		char buffer[100];
		int num_module_params = 0;
		int module_params_length = strlen(modules[number_of_module].module_params);
		for(x=0; x<module_params_length; x++) {
			if(modules[number_of_module].module_params[x] == 32){
				num_module_params++;
			}
		}
		num_module_params++;


		params = (char **) calloc (3+1+1+num_module_params,sizeof(char*));
		params[0] = (char *) calloc (50,sizeof(char)); 	// binary name for exec
		params[1] = (char *) calloc (5,sizeof(char)); 	// libtrap param "-i"
		params[2] = (char *) calloc (100,sizeof(char)); // atributes for "-i" param
		strcpy(params[0],modules[number_of_module].module_name);
		strcpy(params[1],TRAP_PARAM);
		strcpy(params[2],atr);

		switch(verbose_level){
			case 0:
				params_counter = 3;
				params[3] = NULL;
				break;
			case 1:
				params_counter = 4;
				params[3] = (char *) calloc (5,sizeof(char));
				sprintf(params[3],"-v");
				break;
			case 2:
				params_counter = 4;
				params[3] = (char *) calloc (5,sizeof(char));
				sprintf(params[3],"-vv");
				break;
			case 3:
				params_counter = 4;
				params[3] = (char *) calloc (5,sizeof(char));
				sprintf(params[3],"-vvv");
				break;
		}

		y=0;
		memset(buffer,0,100);
		for(x=0; x<module_params_length; x++){
			if(modules[number_of_module].module_params[x] == 32){
				params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
				sprintf(params[params_counter],"%s",buffer);
				params_counter++;
				memset(buffer,0,100);
				y=0;
			} else {
				buffer[y] = modules[number_of_module].module_params[x];
				y++;
			}
		}
		params[params_counter] = (char *) calloc (strlen(buffer)+1,sizeof(char));
		sprintf(params[params_counter],"%s",buffer);
		params_counter++;


		params[params_counter] = NULL;
	}
	return params;
}

int print_menu ()
{
	int ret_val = 0;
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
	if(!scanf("%d",&ret_val)){
		printf("Wrong input.\n");
		return -1;
	}
	return ret_val;
}


void start_module (const int module_number)
{
	if(modules[module_number].module_running == TRUE){
		VERBOSE("Module %d has been already started.\n", module_number);
		return;
	} else {
		modules[module_number].module_running = TRUE;
	}

	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	char log_path_stdout[40];
	char log_path_stderr[40];
	int x = 0;

	running_modules[running_modules_cnt].module_enabled = TRUE;
	strcpy(running_modules[running_modules_cnt].module_name, modules[module_number].module_name);
	strcpy(running_modules[running_modules_cnt].module_path, modules[module_number].module_path);
	running_modules[running_modules_cnt].module_ifces = modules[module_number].module_ifces;
	running_modules[running_modules_cnt].module_restart_cnt = 0;
	running_modules[running_modules_cnt].module_number = module_number;
	memset(&running_modules[running_modules_cnt].module_service_sd,0,1);

	while(strcmp(running_modules[running_modules_cnt].module_ifces[x].ifc_note, "#") != 0){
		if(strcmp(running_modules[running_modules_cnt].module_ifces[x].ifc_direction, "IN") == 0){
			running_modules[running_modules_cnt].module_num_in_ifc++;
		} else if (strcmp(running_modules[running_modules_cnt].module_ifces[x].ifc_direction, "OUT") == 0) {
			running_modules[running_modules_cnt].module_num_out_ifc++;
		}
		x++;
	}

	running_modules[running_modules_cnt].module_counters_array = (int *) calloc (3*running_modules[running_modules_cnt].module_num_out_ifc + running_modules[running_modules_cnt].module_num_in_ifc,sizeof(int));
	sprintf(log_path_stdout,"%s%s%d_stdout",LOGSDIR_NAME, modules[module_number].module_name, running_modules_cnt);
	sprintf(log_path_stderr,"%s%s%d_stderr",LOGSDIR_NAME, modules[module_number].module_name, running_modules_cnt);
	
	fflush(stdout);
	running_modules[running_modules_cnt].module_PID = fork();
	if (running_modules[running_modules_cnt].module_PID == 0) {
		int fd_stdout = open(log_path_stdout, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		int fd_stderr = open(log_path_stderr, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		dup2(fd_stdout,1); //stdout
		dup2(fd_stderr,2); //stderr
		close(fd_stdout);
		close(fd_stderr);
		char ** params = make_module_arguments(module_number);
		printf("%s | %s   %s   %s    %s   %s\n", asctime (timeinfo), params[0], params[1], params[2], params[3], params[4] );
		fflush(stdout);
		execvp(modules[module_number].module_path, params);
		printf("Error while executing module binary.\n");
		running_modules[running_modules_cnt].module_enabled = FALSE;
		exit(1);
	} else if (running_modules[running_modules_cnt].module_PID == -1) {
		running_modules[running_modules_cnt].module_status = FALSE;
		running_modules_cnt++;
		printf("Error in fork.\n");
	} else {
		running_modules[running_modules_cnt].module_status = TRUE;
		running_modules_cnt++;
	}
}

void restart_module (const int module_number)
{

	VERBOSE("Restarting module %d%s\n", module_number, running_modules[module_number].module_name);
	char log_path_stdout[40];
	char log_path_stderr[40];
	sprintf(log_path_stdout,"%s%s%d_stdout",LOGSDIR_NAME, running_modules[module_number].module_name, module_number);
	sprintf(log_path_stderr,"%s%s%d_stderr",LOGSDIR_NAME, running_modules[module_number].module_name, module_number);

	memset(running_modules[module_number].module_counters_array,0,(running_modules[module_number].module_num_in_ifc + (3*running_modules[module_number].module_num_out_ifc)) * sizeof(int));
	running_modules[module_number].last_cpu_usage_user_mode = 0;
	running_modules[module_number].last_cpu_usage_kernel_mode = 0;

	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	fflush(stdout);
	running_modules[module_number].module_PID = fork();
	if (running_modules[module_number].module_PID == 0) {
		int fd_stdout = open(log_path_stdout, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		int fd_stderr = open(log_path_stderr, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		dup2(fd_stdout,1); //stdout
		dup2(fd_stderr,2); //stderr
		close(fd_stdout);
		close(fd_stderr);
		char ** params = make_module_arguments(running_modules[module_number].module_number);
		printf("%s | %s   %s   %s    %s   %s\n", asctime (timeinfo), params[0], params[1], params[2], params[3], params[4] );
		fflush(stdout);
		execvp(running_modules[module_number].module_path, params);
		printf("Error while executing module binary.\n");
		running_modules[module_number].module_enabled = FALSE;
		exit(1);
	} else if (running_modules[module_number].module_PID == -1) {
		running_modules[module_number].module_status = FALSE;
		running_modules[module_number].module_restart_cnt++;
		printf("Err Fork()\n");
	} else {
		running_modules[module_number].module_status = TRUE;
		running_modules[module_number].module_restart_cnt++;
	}
}

void stop_module (const int module_number)
{
	if(running_modules[module_number].module_status){
		running_modules[module_number].module_enabled = FALSE;
		kill(running_modules[module_number].module_PID,2);
		running_modules[module_number].module_status = FALSE;
		running_modules[module_number].module_service_ifc_isconnected = FALSE;
	}
}

void update_module_status ()
{
	int x;
	int status;
	pid_t result;

	for (x=0; x<running_modules_cnt; x++) {
		result = waitpid(running_modules[x].module_PID , &status, WNOHANG);
		if (result == 0) {
		  // Child still alive
		} else if (result == -1) {
		  // Error
			running_modules[x].module_status = FALSE;
			running_modules[x].module_service_ifc_isconnected = FALSE;
		} else {
		  // Child exited
			running_modules[x].module_status = FALSE;
			running_modules[x].module_service_ifc_isconnected = FALSE;
		}
	}
}

void sigpipe_handler(int sig)
{
	if(sig == SIGPIPE){
		VERBOSE("SIGPIPE catched..\n");
	}
}

int api_initialization(const int * argc, char ** argv)
{	
	show_cpu_usage_flag = FALSE;
	verbose_flag = FALSE;
	help_flag = FALSE;
	file_flag = FALSE;
	int ret_val = parse_arguments(argc, argv);
	if(ret_val){
		if(help_flag){
			print_help();
			return 1;
		} else if (file_flag == FALSE) {
			fprintf(stderr,"Wrong format of arguments.\n %s [-h] [-v] [--show-cpuusage] -f config_file.xml\n", argv[0]);
			return 1;
		}
	} else {
		return 1;
	}

	int daemon_arg;
	if (daemon_falg) {
		daemon_init(&daemon_arg, NULL);
	}


	int y;
	VERBOSE("---LOADING CONFIGURATION---\n");
	modules_cnt = 0;
	//init modules structures alloc size 10
	modules_array_size = 10;
	modules = (module_atr_t * ) calloc (modules_array_size,sizeof(module_atr_t));
	for(y=0;y<modules_array_size;y++) {
		modules[y].module_ifces = (interface_t *)calloc(IFC_MAX, sizeof(interface_t));
		modules[y].module_running = FALSE;
	}

	//load configuration
	load_configuration (TRUE,config_file);
	
	//logs directory
	struct stat st = {0};
	if (stat(LOGSDIR_NAME, &st) == -1) {
	    mkdir(LOGSDIR_NAME, PERM_LOGSDIR);
	}

	verbose_level = 0;
	running_modules = (running_module_t * ) calloc (MAX_NUMBER_MODULES,sizeof(running_module_t));
	memset(running_modules,0,MAX_NUMBER_MODULES*sizeof(running_module_t));
	pthread_mutex_init(&running_modules_lock,NULL);
	service_thread_continue = TRUE;
	configuration_running = FALSE;

	VERBOSE("-- Starting service thread --\n");
	start_service_thread();


   	void sigpipe_handler(int sig);
   	struct sigaction sa;

   	sa.sa_handler = sigpipe_handler;
   	sa.sa_flags = 0;
   	sigemptyset(&sa.sa_mask);

   	if(sigaction(SIGPIPE,&sa,NULL) == -1){
      	printf("ERROR sigaction !!\n");
   	}

   	graph_first_node = NULL;
   	graph_last_node = NULL;

   	if (daemon_falg) {
		daemon_mode(&daemon_arg);
		return 2;
	}

	return 0;
}

void api_start_configuration()
{
	if(configuration_running){
		printf("Configuration is already running, you cannot start another module from loaded xml config_file.\n");
		return;
	}
	pthread_mutex_lock(&running_modules_lock);
	VERBOSE("Starting configuration...\n");
	configuration_running = TRUE;
	int x = 0;
	for (x=0; x<modules_cnt; x++) {
		start_module(x);
	}
	VERBOSE("Configuration is running.\n");
	pthread_mutex_unlock(&running_modules_lock);
}

void api_stop_configuration()
{
	pthread_mutex_lock(&running_modules_lock);
	VERBOSE("Stopping configuration...\n");
	int x = 0;
	for (x=0; x<running_modules_cnt; x++) {
		stop_module(x);
	}
	VERBOSE("Configuration stopped.\n");
	pthread_mutex_unlock(&running_modules_lock);
}

void api_start_module()
{
	if(configuration_running){
		VERBOSE("Configuration is already running, you cannot start another module loaded from xml config_file.\n");
		return;
	}
	pthread_mutex_lock(&running_modules_lock);
	int x = 0;
	printf("Type in module number\n");
	if(!scanf("%d",&x)){
		printf("Wrong input.\n");
		return;
	}
	if(x>=modules_cnt || x<0){
		printf("Wrong input, type in 0 - %d.\n", modules_cnt-1);
		pthread_mutex_unlock(&running_modules_lock);
		return;
	}
	start_module(x);
	pthread_mutex_unlock(&running_modules_lock);
}

void api_stop_module()
{
	pthread_mutex_lock(&running_modules_lock);
	int x = 0;
	char symbol;
	for (x=0;x<running_modules_cnt;x++) {
		if (running_modules[x].module_status) {
			printf("%d%s running (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_PID);
		}
	}
	printf("Type in number of module to kill: (# for cancel)\n");
	if(!scanf("%s",&symbol)){
		printf("Wrong input.\n");
		return;
	}
	if (symbol != '#'){
		stop_module(symbol - '0');
	} else if((symbol - '0')>=running_modules_cnt || (symbol - '0')<0){
		printf("Wrong input, type in 0 - %d.\n", running_modules_cnt);
		return;
	}
	pthread_mutex_unlock(&running_modules_lock);
}

void api_set_module_enabled()
{
	int x;
	pthread_mutex_lock(&running_modules_lock);
	printf("Type in module number\n");
	if(!scanf("%d",&x)){
		printf("Wrong input.\n");
		return;
	}
	if(x>=running_modules_cnt || x<0){
		printf("Wrong input, type in 0 - %d.\n", running_modules_cnt-1);
		pthread_mutex_unlock(&running_modules_lock);
		return;
	}
	running_modules[x].module_enabled=TRUE;
	pthread_mutex_unlock(&running_modules_lock);
}

void restart_modules()
{
	int x = 0;
	for (x=0; x<running_modules_cnt; x++) {
		if (running_modules[x].module_status == FALSE && (running_modules[x].module_restart_cnt >= MAX_RESTARTS)) {
			VERBOSE("Module: %d%s was restarted %d times and it is down again.\n",x, running_modules[x].module_name, MAX_RESTARTS);
		} else if (running_modules[x].module_status == FALSE && running_modules[x].module_enabled == TRUE) {
			restart_module(x);
		}
	}
}

void api_show_running_modules_status()
{
	int x = 0;
	if (running_modules_cnt == 0) {
		printf("No module running.\n");
		return;
	}
	for (x=0; x<running_modules_cnt; x++) {
		if (running_modules[x].module_status == TRUE) {
			printf("%d%s running (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_PID);
		} else {
			printf("%d%s stopped (PID: %d)\n",x, running_modules[x].module_name,running_modules[x].module_PID);
		}
	}
}

void api_show_available_modules()
{
	print_configuration();
}

void api_quit()
{
	int x;
	printf("-- Aborting service thread --\n");
	stop_service_thread();
	sleep(3);
	api_stop_configuration();
	for(x=0;x<running_modules_cnt;x++){
		free(running_modules[x].module_counters_array);
	}
	for(x=0;x<modules_array_size;x++){
		free(modules[x].module_ifces);
	}
	free(modules);
	free(running_modules);
	destroy_graph(graph_first_node);
	free(service_thread_id);
}

int api_print_menu()
{
	return print_menu();
}

void api_update_module_status()
{
	update_module_status();
}

char * get_param_by_delimiter(const char *source, char **dest, const char delimiter)
{
   char *param_end = NULL;
   unsigned int param_size = 0;

   if (source == NULL) {
      return NULL;
   }

   param_end = strchr(source, delimiter);
   if (param_end == NULL) {
      /* no delimiter found, copy the whole source */
      *dest = strdup(source);
      return NULL;
   }

   param_size = param_end - source;
   *dest = (char *) calloc(1, param_size + 1);
   if (*dest == NULL) {
      return (NULL);
   }
   strncpy(*dest, source, param_size);
   return param_end + 1;
}

void update_cpu_usage(long int * last_total_cpu_usage)
{
	int utime = 0, stime = 0;
	long int new_total_cpu_usage = 0;
	FILE * proc_stat_fd = fopen("/proc/stat","r");
	int num = 0;
	int x;
	char path[20];
	memset(path,0,20);
	int rozdil_total = 0;

	if(fscanf(proc_stat_fd,"cpu") != 0) {
		return;
	}

	for(x=0;x<10;x++){
		if(!fscanf(proc_stat_fd,"%d",&num)){
			continue;
		}
		new_total_cpu_usage += num;
	}
	rozdil_total = new_total_cpu_usage - *last_total_cpu_usage;
	if(show_cpu_usage_flag) {
		printf("total_cpu difference total cpu usage> %d\n", rozdil_total);
	}
	*last_total_cpu_usage = new_total_cpu_usage;
	fclose(proc_stat_fd);
  
	for(x=0;x<running_modules_cnt;x++){
		if(running_modules[x].module_status){
			sprintf(path,"/proc/%d/stat",running_modules[x].module_PID);
			proc_stat_fd = fopen(path,"r");
			if(!fscanf(proc_stat_fd,"%*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %*[^' '] %d %d", &utime , &stime)){
				continue;
			}
			if(show_cpu_usage_flag) {
				printf("Last_u%d Last_s%d, New_u%d New_s%d |>>   u%d%% s%d%%\n", running_modules[x].last_cpu_usage_user_mode,
														running_modules[x].last_cpu_usage_kernel_mode,
														utime, stime,
														100 * (utime - running_modules[x].last_cpu_usage_user_mode)/rozdil_total, 
														100 * (stime - running_modules[x].last_cpu_usage_kernel_mode)/rozdil_total);
			}
			running_modules[x].last_cpu_usage_user_mode = utime;
			running_modules[x].last_cpu_usage_kernel_mode = stime;
			fclose(proc_stat_fd);
		}
	}
}

int service_get_data(int sd, int running_module_number)
{
   int sizeof_recv = (running_modules[running_module_number].module_num_in_ifc + 3*running_modules[running_module_number].module_num_out_ifc) * sizeof(int);
   int total_receved = 0;
   int last_receved = 0;

   while(total_receved < sizeof_recv){
      last_receved = recv(sd, running_modules[running_module_number].module_counters_array + total_receved, sizeof_recv - total_receved, 0);
      if(last_receved == 0){
         VERBOSE("------- ! Modules service thread closed its socket, im done !\n");
         return 0;
      }
      total_receved += last_receved;
   }
   return 1;
}

void connect_to_module_service_ifc(int module, int num_ifc)
{
	char * dest_port;
	int sockfd;
	union tcpip_socket_addr addr;

	get_param_by_delimiter(running_modules[module].module_ifces[num_ifc].ifc_params, &dest_port, ',');
	VERBOSE("-- connecting to module %d%s on port %s\n",module,running_modules[module].module_name, dest_port);

	memset(&addr, 0, sizeof(addr));

	addr.unix_addr.sun_family = AF_UNIX;
	snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, UNIX_PATH_FILENAME_FORMAT, dest_port);
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (connect(sockfd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) != -1) {
	//   p = (struct addrinfo *) &addr.unix_addr;
	}
	running_modules[module].module_service_sd = sockfd;
	running_modules[module].module_has_service_ifc = TRUE;
	running_modules[module].module_service_ifc_isconnected = TRUE;
	free(dest_port);
}


void * service_thread_routine(void* arg)
{
	long int last_total_cpu_usage = 0;
	int sizeof_intptr = 4;
	int num_served_modules = 0;
	int x,y;

	while(service_thread_continue == 1)
	{
		pthread_mutex_lock(&running_modules_lock);
		VERBOSE("------>\n");
		usleep(100000);
		update_module_status();
		restart_modules();
		usleep(100000);
		update_cpu_usage(&last_total_cpu_usage); 
		if(running_modules_cnt > num_served_modules)
		{
			x=0;
			while(1)
			{
				if((strcmp(running_modules[num_served_modules].module_ifces[x].ifc_direction, "SERVICE") == 0)){
					connect_to_module_service_ifc(num_served_modules,x);
					graph_node_t * new_node = add_graph_node (graph_first_node, graph_last_node, (void *) &running_modules[num_served_modules]);

					if(graph_first_node == NULL) {
						graph_first_node = new_node;
						graph_last_node = new_node;
						graph_first_node->next_node = NULL;
					} else {
						graph_last_node->next_node = new_node;
						graph_last_node = new_node;
						graph_last_node->next_node = NULL;
					}
				    break;
				} else if (strcmp(running_modules[num_served_modules].module_ifces[x].ifc_note, "#") == 0) {
					running_modules[num_served_modules].module_has_service_ifc = FALSE;
					break;
				} else {
					x++;
				}
			}

			num_served_modules++;
			pthread_mutex_unlock(&running_modules_lock);
		} else {
			int request[1];
			memset(request,0,1);
			request[0] = 1;
			for(x=0;x<running_modules_cnt;x++){
				if(running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
					if(running_modules[x].module_service_ifc_isconnected == FALSE){
						y=0;
						while(1){
							if((strcmp(running_modules[x].module_ifces[y].ifc_direction, "SERVICE") == 0)){
								break;
							} else {
								y++;
							}
						}
						close(running_modules[x].module_service_sd);
						connect_to_module_service_ifc(x,y);
					}

					if(send(running_modules[x].module_service_sd,(void *) request, sizeof_intptr, 0) == -1){
						VERBOSE("Error while sending request to module %d%s.\n",x,running_modules[x].module_name);
						if (errno == ENOTCONN) {
							running_modules[x].module_service_ifc_isconnected = FALSE;
						}
					}
				}
			}

			update_module_status();
			for(x=0;x<running_modules_cnt;x++){
				if(running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
					if(running_modules[x].module_service_ifc_isconnected){
						if(!(service_get_data(running_modules[x].module_service_sd, x))){
					    	//TODO
					    	continue;
					    }
					}
				}
			}
			pthread_mutex_unlock(&running_modules_lock);
			//fflush(running_modules[0].fd_std_out);
			// fsync(running_modules[0].fd_std_out);
			update_graph_values(graph_first_node);
			// check_graph_values(graph_first_node);

			for(x=0;x<running_modules_cnt;x++){
				VERBOSE("NAME:  %s, PID: %d, EN: %d, SIFC: %d, S: %d, ISC: %d | ", running_modules[x].module_name,
					running_modules[x].module_PID,
					running_modules[x].module_enabled,
					running_modules[x].module_has_service_ifc,
					running_modules[x].module_status,
					running_modules[x].module_service_ifc_isconnected);
				if(running_modules[x].module_has_service_ifc && running_modules[x].module_service_ifc_isconnected){
					VERBOSE("CNT_RM:  ");
					for(y=0;y<running_modules[x].module_num_in_ifc;y++){
						VERBOSE("%d  ", running_modules[x].module_counters_array[y]);
					}
					VERBOSE("CNT_SM:  ");
					for(y=0;y<running_modules[x].module_num_out_ifc;y++){
						VERBOSE("%d  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc]);
					}
					VERBOSE("CNT_SB:  ");
					for(y=0;y<running_modules[x].module_num_out_ifc;y++){
						VERBOSE("%d  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc + running_modules[x].module_num_out_ifc]);
					}
					VERBOSE("CNT_AF:  ");
					for(y=0;y<running_modules[x].module_num_out_ifc;y++){
						VERBOSE("%d  ", running_modules[x].module_counters_array[y + running_modules[x].module_num_in_ifc + 2*running_modules[x].module_num_out_ifc]);
					}
				}
				VERBOSE("\n");
			}

			sleep(2);
		}
	}


	for(x=0;x<running_modules_cnt;x++){
		if(running_modules[x].module_has_service_ifc == TRUE && running_modules[x].module_status == TRUE) {
			VERBOSE("-- disconnecting from module %d%s\n",x, running_modules[x].module_name);
			close(running_modules[x].module_service_sd);
		}
	}

	pthread_exit(NULL);
}



void start_service_thread()
{
	service_thread_id = (pthread_t *) calloc (1,sizeof(pthread_t));
	pthread_create(service_thread_id,NULL,service_thread_routine, NULL);
}

void stop_service_thread()
{
	service_thread_continue = 0;
}

void api_show_graph()
{
	generate_graph_code(graph_first_node);
	show_graph();
}


void run_temp_configuration()
{
	pthread_mutex_lock(&running_modules_lock);
	int x = modules_cnt;
	char * buffer1 = (char *)calloc(1000, sizeof(char));
	char * buffer2 = (char *)calloc(1000, sizeof(char));
	char * ptr = buffer1;
	printf("Paste in xml code with modules configuration starting with <modules> tag and ending with </modules> tag.\n");
	while(1){
		if(!scanf("%s",ptr)){
			printf("Wrong input.\n");
			continue;
		}
		if(strstr(ptr,"</modules>") != NULL){
			break;
		}
		ptr += strlen(ptr);
		sprintf(ptr," ");
		ptr++;
	}
	sprintf(buffer2,"<?xml version=\"1.0\"?>\n<nemea-supervisor>\n%s\n</nemea-supervisor>",buffer1);
	free(buffer1);

	if(load_configuration(FALSE, buffer2) == FALSE){
		printf("Xml code was not parsed successfully.\n");
		free(buffer2);
		pthread_mutex_unlock(&running_modules_lock);
		return;
	}
	free(buffer2);
	while(x<modules_cnt){
		start_module(x);
		x++;
	}
	pthread_mutex_unlock(&running_modules_lock);
}

void api_run_temp_conf()
{
	run_temp_configuration();
}

void api_set_verbose_level()
{
	int x;
	printf("Type in number in range 0-3 to set verbose level of modules:\n");
	if(!scanf("%d",&x)){
		printf("Wrong input.\n");
		return;
	}
	if(x>3 || x<0){
		printf("Wrong input.\n");
	} else {
		verbose_level = x;
	}
}

int parse_arguments(const int * argc, char ** argv)
{
	if(*argc <= 1){
		fprintf(stderr,"Wrong format of arguments.\n %s [-h] [-v] [--show-cpuusage] -f config_file.xml\n", argv[0]);
		return FALSE;
	}
	int x;
	for(x=1; x<*argc; x++) {
		if (strcmp(argv[x],"-h") == 0) {
			help_flag = TRUE;
		} else if (strcmp(argv[x],"-f") == 0 && *argc>x+1) {
			if (strstr(argv[x+1],".xml") != NULL) {
				file_flag = TRUE;
				strcpy(config_file, argv[x+1]);
				x++;
			}
		} else if (strcmp(argv[x],"--show-cpuusage") == 0) {
			show_cpu_usage_flag = TRUE;
		} else if (strcmp(argv[x],"-v") == 0) {
			verbose_flag = TRUE;
		} else if (strcmp(argv[x],"--daemon") == 0) {
			daemon_falg = TRUE;
		} else {
			fprintf(stderr,"Wrong format of arguments.\n %s [-h] [-v] [--show-cpuusage] -f config_file.xml\n", argv[0]);
			return FALSE;
		}
	}

	return TRUE;
}

void print_help()
{
	printf("--------------------------\n"
		   "NEMEA Supervisor:\n"
		   "Expected arguments to run supervisor are: ./supervisor [-h] [-v] [--show-cpuusage] -f config_file.xml\n"
		   "Main thread is waiting for input with number of command to execute.\n"
		   "Functions:\n"
		   "\t- 0. SET VERBOSE LEVEL\n"
		   "\t- 1. RUN CONFIGURATION\n"
		   "\t- 2. STOP CONFIGURATION\n"
		   "\t- 3. START MODUL\n"
		   "\t- 4. STOP MODUL\n"
		   "\t- 5. SET MODULE ENABLED\n"
		   "\t- 6. STARTED MODULES STATUS\n"
		   "\t- 7. AVAILABLE MODULES\n"
		   "\t- 8. SHOW GRAPH\n"
		   "\t- 9. RUN TEMP CONF\n"
		   "\t- 10. QUIT\n"
		   "--------------------\n"
		   "0 - setter of VERBOSE level of executed modules\n"
		   "1 - command executes every loaded module from configuration\n"
		   "2 - command stops whole configuration\n"
		   "3 - command starts single module from configuration, expected input is number of module (command num. 7 can show available modules)\n"
		   "4 - command stops single running module\n"
		   "5 - setter of \"ENABLE\" flag of selected module - this flag is important for autorestarts\n"
		   "6 - command prints status of started modules\n"
		   "7 - command prints loaded modules configuration\n"
		   "8 - command generates code for dot program and shows graph of running modules using display program\n"
		   "9 - command parses external configuration of pasted modules, expected input is same xml code as in config_file, first tag is <modules> and last tag </modules>\n"
		   "10 - shut down command\n\n"
		   "Example of input for command num. 9:\n"
		   "<modules>\n"
				"\t<module>\n"
					"\t\t<params>NULL</params>\n"
					"\t\t<name>flowcounter</name>\n"
					"\t\t<path>../modules/flowcounter/flowcounter</path>\n"
					"\t\t<trapinterfaces>\n"
						"\t\t\t<interface>\n"
							"\t\t\t\t<note>whatever</note>\n"
							"\t\t\t\t<type>TCP</type>\n"
							"\t\t\t\t<direction>IN</direction>\n"
							"\t\t\t\t<params>localhost,8000</params>\n"
						"\t\t\t</interface>\n"
						"\t\t\t<interface>\n"
							"\t\t\t\t<note>whatever</note>\n"
							"\t\t\t\t<type>UNIXSOCKET</type>\n"
							"\t\t\t\t<direction>SERVICE</direction>\n"
							"\t\t\t\t<params>9022,1</params>\n"
						"\t\t\t</interface>\n"
					"\t\t</trapinterfaces>\n"
				"\t</module>\n"
			"</modules>\n");
}

int daemon_init(int * d_sd, int * fd)
{
	// create socket

	union tcpip_socket_addr addr;
	struct addrinfo *p;

	memset(&addr, 0, sizeof(addr));

	addr.unix_addr.sun_family = AF_UNIX;
	snprintf(addr.unix_addr.sun_path, sizeof(addr.unix_addr.sun_path) - 1, DAEMON_UNIX_PATH_FILENAME_FORMAT);
	/* if socket file exists, it could be hard to create new socket and bind */
	unlink(DAEMON_UNIX_PATH_FILENAME_FORMAT); /* error when file does not exist is not a problem */
	*d_sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (bind(*d_sd, (struct sockaddr *) &addr.unix_addr, sizeof(addr.unix_addr)) != -1) {
		p = (struct addrinfo *) &addr.unix_addr;
		chmod(DAEMON_UNIX_PATH_FILENAME_FORMAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	} else {
		/* error bind() failed */
		p = NULL;
	}

	if (p == NULL) {
		// if we got here, it means we didn't get bound
		VERBOSE("selectserver: failed to bind");
		return 10;
	}

	// listen
	if (listen(*d_sd, 1) == -1) {
		//perror("listen");
		VERBOSE("Listen failed");
		return 10;
	}


	// create daemon

	pid_t process_id = 0;
	pid_t sid = 0;

	fflush(stdout);
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
		// Return failure
		exit(1);
	}
	// chdir("./modules_logs/");

	int fd_fd = open ("supervisor_log", O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
	dup2(fd_fd, 1);
	dup2(fd_fd, 2);
	close(fd_fd);

	return 0;
}


int daemon_get_client (int * d_sd)
{
   struct sockaddr_storage remoteaddr; // client address
   socklen_t addrlen;
   int newclient;

   	// handle new connection
   	addrlen = sizeof remoteaddr;
	newclient = accept(*d_sd, (struct sockaddr *)&remoteaddr, &addrlen);
	if (newclient == -1) {
		VERBOSE("Accepting new client failed.");
	}
	printf("Client has connected.\n");
	return newclient;
}


void daemon_mode(int * arg)
{
	int daemon_sd = *arg;
	int client_sd;
	int supervisor_output_file;
	int terminated = FALSE;

	int ret_val;
	int request;
	char buffer[50];

	int connected = FALSE;

	while(terminated == FALSE){
		// get client
		client_sd = daemon_get_client(&daemon_sd);
		if(client_sd == -1){
			connected = FALSE;
			continue;
		}
		connected = TRUE;

		while(connected){
			// serve client
			ret_val = recv(client_sd,&request,sizeof(request),0);
			if(ret_val == 0){
				printf("Client has disconnected.\n");
				connected = FALSE;
				break;
			} else if (ret_val == -1){
				printf("Error while recving.\n");
				connected = FALSE;
				break;
			}
			switch(request){

				// api_set_verbose_level();
				// api_start_module();		
				// api_stop_module();
				// api_set_module_enabled();
				// api_run_temp_conf();

				case 0:
					sprintf(buffer,"OK");
					break;

				case 1:
					api_start_configuration();
					sprintf(buffer,"OK");
					break;

				case 2:
					api_stop_configuration();
					sprintf(buffer,"OK");
					break;

				case 3:
					sprintf(buffer,"OK");
					break;

				case 4:
					sprintf(buffer,"OK");
					break;

				case 5:
					sprintf(buffer,"OK");
					break;

				case 6:
					api_show_running_modules_status();
					sprintf(buffer,"OK");
					break;

				case 7:
					api_show_available_modules();
					sprintf(buffer,"OK");
					break;

				case 8:
					api_show_graph();
					sprintf(buffer,"OK");
					break;

				case 9:
					sprintf(buffer,"OK");
					break;

				case 10:
					api_quit();
					sprintf(buffer,"OK");
					connected = FALSE;
					terminated = TRUE;
					break;

				default:
					sprintf(buffer,"Wrong input");
					break;

			}

			ret_val = send(client_sd,buffer,50,0);
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
		}
	}

	close(supervisor_output_file);
	close(client_sd);
	close(daemon_sd);
	unlink(DAEMON_UNIX_PATH_FILENAME_FORMAT);
	return;
}