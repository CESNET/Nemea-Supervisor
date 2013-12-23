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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "supervisor.h"

#ifndef PERM_LOGSDIR
#define PERM_LOGSDIR 	(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) ///< Permissions of directory with stdout and stderr logs of modules
#endif

#ifndef PERM_LOGFILE
#define PERM_LOGFILE 	(S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) ///< Permissions of files with stdout and stderr logs of module
#endif

#define LOGSDIR_NAME 	"./modules_logs/" ///< Name of directory with logs
#define TRAP_PARAM 		"-i" ///< Interface parameter for libtrap
#define MAX_RESTARTS	3  ///< Maximum number of module restarts

// struct running_module;

/*******GLOBAL VARIABLES*******/
module_atr * 		MODULES;  ///< Information about modules from loaded xml config file
running_module * 	RUNNING_MODULES;  ///< Information about running modules

int		MODULES_cnt;  ///< Number of modules from loaded xml config file
int 	RUNNING_MODULES_cnt;  ///< Number of running modules

/**********************/

void LoadConfiguration(const char * config_file)
{
	xmlDocPtr xml_tree;
	xmlNodePtr current_node;

	xml_tree = xmlParseFile(config_file);
	
	if (xml_tree == NULL ) {
		fprintf(stderr,"Document not parsed successfully. \n");
		return;
	}
	
	current_node = xmlDocGetRootElement(xml_tree);
	
	if (current_node == NULL) {
		fprintf(stderr,"empty document\n");
		xmlFreeDoc(xml_tree);
		return;
	}
	
	if (xmlStrcmp(current_node->name, BAD_CAST "nemea-supervisor")) {
		fprintf(stderr,"document of the wrong type, root node != supervisor");
		xmlFreeDoc(xml_tree);
		return;
	}

	current_node = current_node->xmlChildrenNode;
	while(current_node != NULL)
	{
		if((!xmlStrcmp(current_node->name, BAD_CAST "modules")))
		{
			MODULES_cnt = atoi(xmlGetProp(current_node, "number"));
			MODULES = (module_atr * ) calloc (MODULES_cnt,sizeof(module_atr));
			break;
		}
		current_node = current_node-> next;
	}

	/*****************/

	xmlNodePtr module_ptr = current_node->xmlChildrenNode;
	xmlNodePtr module_atr, ifc_ptr, ifc_atr;
	int module_cnt = 0, ifc_cnt = 0;

	while(module_ptr != NULL)
	{// modules 
		if(xmlStrcmp(module_ptr->name,BAD_CAST "text"))
		{
			module_atr = module_ptr->xmlChildrenNode;
			while(module_atr != NULL)
			{// atributes of module
				if((!xmlStrcmp(module_atr->name,BAD_CAST "params")))
				{
					strcpy(MODULES[module_cnt].module_params, (char *) xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1));
				}
				else if((!xmlStrcmp(module_atr->name,BAD_CAST "name")))
				{
					strcpy(MODULES[module_cnt].module_name, (char *) xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1));
				}
				else if((!xmlStrcmp(module_atr->name,BAD_CAST "path")))
				{
					strcpy(MODULES[module_cnt].module_path, (char *) xmlNodeListGetString(xml_tree, module_atr->xmlChildrenNode, 1));
				}
				else if((!xmlStrcmp(module_atr->name,BAD_CAST "trapinterfaces")))
				{
					ifc_cnt=0;
					ifc_ptr = module_atr->xmlChildrenNode;
					while(ifc_ptr != NULL)
					{// interfaci
						if(xmlStrcmp(ifc_ptr->name,BAD_CAST "text"))
						{	
							// memset(MODULES[module_cnt].module_ifces,0,IFC_MAX);
							ifc_atr = ifc_ptr->xmlChildrenNode;
							while(ifc_atr != NULL)
							{// atributes of interface
								if((!xmlStrcmp(ifc_atr->name,BAD_CAST "id")))
								{
									strcpy(MODULES[module_cnt].module_ifces[ifc_cnt].ifc_id , (char *) xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1));
								}
								else if((!xmlStrcmp(ifc_atr->name,BAD_CAST "type")))
								{
									memset(MODULES[module_cnt].module_ifces[ifc_cnt].ifc_type,0,PARAMS_MAX);
									strcpy(MODULES[module_cnt].module_ifces[ifc_cnt].ifc_type, (char *) xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1));
								}
								else if((!xmlStrcmp(ifc_atr->name,BAD_CAST "direction")))
								{
									strcpy(MODULES[module_cnt].module_ifces[ifc_cnt].ifc_direction, (char *) xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1));
								}
								else if((!xmlStrcmp(ifc_atr->name,BAD_CAST "params")))
								{
									strcpy(MODULES[module_cnt].module_ifces[ifc_cnt].ifc_params, (char *) xmlNodeListGetString(xml_tree, ifc_atr->xmlChildrenNode, 1));
								}
								ifc_atr=ifc_atr->next;
							}
							ifc_cnt++;
						}

						ifc_ptr = ifc_ptr->next;
					}
					sprintf(MODULES[module_cnt].module_ifces[ifc_cnt].ifc_id,"%s","#");
				}

				module_atr = module_atr->next;
			}
			module_cnt++;
		}
		module_ptr = module_ptr->next;
	}

	xmlFreeDoc(xml_tree);
	return;
}


void Print_Configuration ()
{
	int x,y = 0;
	printf("---PRINTING CONFIGURATION---\n");
	for(x=0; x < MODULES_cnt; x++)
	{
		printf("%d_%s:  PATH:%s  PARAMS:%s\n", x, MODULES[x].module_name, MODULES[x].module_path, MODULES[x].module_params);
		y=0;
		while(MODULES[x].module_ifces[y].ifc_id[0] != '#')
		{
			printf("\tIFC%d\tID: %s\n",y, MODULES[x].module_ifces[y].ifc_id);
			printf("\t\tTYPE: %s\n", MODULES[x].module_ifces[y].ifc_type);
			printf("\t\tDIRECTION: %s\n", MODULES[x].module_ifces[y].ifc_direction);
			printf("\t\tPARAMS: %s\n", MODULES[x].module_ifces[y].ifc_params);
			y++;
		}
	}
}

char ** MakeModuleArguments(const int number_of_module)
{
	char atr[PARAMS_MAX];
	memset(atr,0,PARAMS_MAX);
	int x = 0;
	char * ptr = atr;

	while(MODULES[number_of_module].module_ifces[x].ifc_id[0] != '#')
	{
		if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_direction, "IN"))
		{
			if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_type, "TCP"))
			{
				strcpy(ptr,"t");
				ptr++;
			}
			else if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET"))
			{
				strcpy(ptr,"u");
				ptr++;
			}
			else
			{
				printf("%s\n", MODULES[number_of_module].module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
		x++;
	}

	x=0;
	while(MODULES[number_of_module].module_ifces[x].ifc_id[0] != '#')
	{
		if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_direction,"OUT"))
		{
			if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_type, "TCP"))
			{
				strcpy(ptr,"t");
				ptr++;
			}
			else if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_type, "UNIXSOCKET"))
			{
				strcpy(ptr,"u");
				ptr++;
			}
			else
			{
				printf("%s\n", MODULES[number_of_module].module_ifces[x].ifc_type);
				printf("Wrong ifc_type in module %d.\n", number_of_module);
				return NULL;
			}
		}
		x++;
	}

	strcpy(ptr,";");
	ptr++;

	x=0;
	while(MODULES[number_of_module].module_ifces[x].ifc_id[0] != '#')
	{
		if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_direction,"IN"))
		{
			sprintf(ptr,"%s;",MODULES[number_of_module].module_ifces[x].ifc_params);
			ptr += strlen(MODULES[number_of_module].module_ifces[x].ifc_params) + 1;
		}
		x++;
	}

	x=0;
	while(MODULES[number_of_module].module_ifces[x].ifc_id[0] != '#')
	{
		if(!strcmp(MODULES[number_of_module].module_ifces[x].ifc_direction,"OUT"))
		{
			sprintf(ptr,"%s;",MODULES[number_of_module].module_ifces[x].ifc_params);
			ptr += strlen(MODULES[number_of_module].module_ifces[x].ifc_params) + 1;
		}
		x++;
	}

	memset(ptr-1,0,1);

	char ** params = (char **) calloc (5,sizeof(char*));
	params[0] = (char *) calloc (100,sizeof(char));
	params[1] = (char *) calloc (100,sizeof(char));
	params[2] = (char *) calloc (100,sizeof(char));
	params[3] = (char *) calloc (100,sizeof(char));
	strcpy(params[0],MODULES[number_of_module].module_name);
	strcpy(params[1],TRAP_PARAM);
	strcpy(params[2],atr);


	if(MODULES[number_of_module].module_params == "NULL")
	{
		params[3] = NULL;
		params[4] = NULL;
	}
	else
	{
		strcpy(params[3],MODULES[number_of_module].module_params);
		params[4] = NULL;
	}
	return params;
}

int Print_menu()
{
	int ret_val = 0;
	printf("--------OPTIONS--------\n");
	printf("1. RUN CONFIGURATION\n");
	printf("2. START MODUL\n");
	printf("3. STOP MODUL\n");
	printf("4. RESTART \"DEAD\" MODULES\n");
	printf("5. STARTED MODULES STATUS\n");
	printf("6. AVAILABLE MODULES\n");
	printf("7. QUIT\n");
	scanf("%d",&ret_val);
	return ret_val;
}

void StartModule(const int module_number)
{
	char 	log_path[40];
	
	strcpy(RUNNING_MODULES[RUNNING_MODULES_cnt].module_name, MODULES[module_number].module_name);
	strcpy(RUNNING_MODULES[RUNNING_MODULES_cnt].module_path, MODULES[module_number].module_path);
	RUNNING_MODULES[RUNNING_MODULES_cnt].module_ifces = MODULES[module_number].module_ifces;
	RUNNING_MODULES[RUNNING_MODULES_cnt].module_restart_cnt = 0;
	RUNNING_MODULES[RUNNING_MODULES_cnt].module_number = module_number;

	sprintf(log_path,"%s%s%d",LOGSDIR_NAME, MODULES[module_number].module_name, RUNNING_MODULES_cnt);

	RUNNING_MODULES[RUNNING_MODULES_cnt].module_PID = fork();
	if(RUNNING_MODULES[RUNNING_MODULES_cnt].module_PID == 0)
	{
		int fd = open(log_path, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		dup2(fd,1); //stdout
		dup2(fd,2); //stderr
		close(fd);
		char ** params = MakeModuleArguments(module_number);
		printf("%s   %s   %s    %s   %s\n",params[0], params[1], params[2], params[3], params[4] );
		execvp(MODULES[module_number].module_path, params);
		printf("Err execl neprobehl\n");
	}
	else if (RUNNING_MODULES[RUNNING_MODULES_cnt].module_PID == -1)
	{
		RUNNING_MODULES[RUNNING_MODULES_cnt].module_status = FALSE;
		RUNNING_MODULES_cnt++;
		printf("Err Fork()\n");
	}
	else
	{
		RUNNING_MODULES[RUNNING_MODULES_cnt].module_status = TRUE;
		RUNNING_MODULES_cnt++;
	}	
}

void RestartModule(const int module_number)
{
	char 	log_path[40];
	sprintf(log_path,"%s%s%d",LOGSDIR_NAME, RUNNING_MODULES[module_number].module_name, module_number);

	RUNNING_MODULES[module_number].module_PID = fork();
	if(RUNNING_MODULES[module_number].module_PID == 0)
	{
		int fd = open(log_path, O_RDWR | O_CREAT | O_APPEND, PERM_LOGFILE);
		dup2(fd,1);
		dup2(fd,2);
		close(fd);
		char ** params = MakeModuleArguments(RUNNING_MODULES[module_number].module_number);
		printf("%s   %s   %s    %s   %s\n",params[0], params[1], params[2], params[3], params[4] );
		execvp(RUNNING_MODULES[module_number].module_path, params);
		printf("Err execl neprobehl\n");
	}

	else if (RUNNING_MODULES[module_number].module_PID == -1)
	{
		RUNNING_MODULES[module_number].module_status = FALSE;
		RUNNING_MODULES[module_number].module_restart_cnt++;
		printf("Err Fork()\n");
	}
	else
	{
		RUNNING_MODULES[module_number].module_status = TRUE;
		RUNNING_MODULES[module_number].module_restart_cnt++;
	}
}

void UpdateModuleStatus()
{
	int x;
	int status;
	pid_t result;

	for(x=0; x<RUNNING_MODULES_cnt; x++)
	{
		result = waitpid(RUNNING_MODULES[x].module_PID , &status, WNOHANG);
		if (result == 0) {
		  // Child still alive
			RUNNING_MODULES[x].module_status = TRUE;
		} else if (result == -1) {
		  // Error ???
			RUNNING_MODULES[x].module_status = FALSE;
		} else {
		  // Child exited
			RUNNING_MODULES[x].module_status = FALSE;
		}
	}
}


int main (int argc, char * argv [])
{
	int 	ret_val = 0;
	int 	x,y;
	char * 	config_file;
		
	if (argc <= 1) {
		printf("Usage: %s config_file\n", argv[0]);
		return(0);
	}

	config_file = argv[1];
	printf("---LOADING CONFIGURATION---\n");
	//load configuration
	LoadConfiguration (config_file);
	
	//logs directory
	struct stat st = {0};
	if (stat(LOGSDIR_NAME, &st) == -1) {
	    mkdir(LOGSDIR_NAME, PERM_LOGSDIR);
	}

	RUNNING_MODULES = (running_module * ) calloc (MAX_NUMBER_MODULES,sizeof(running_module));

	while ((ret_val = Print_menu()) != 7)
	{
		switch(ret_val)
		{
			case 1:
			{
				for (x=0; x<MODULES_cnt; x++)
				{
					StartModule(x);
				}
				break;
			}

			case 2:
			{
				printf("Type in module number\n");
				scanf("%d",&x);
				StartModule(x);		
				break;
			}

			case 3:
			{
				UpdateModuleStatus();
				char symbol;
				for(x=0;x<RUNNING_MODULES_cnt;x++)
				{
					if(RUNNING_MODULES[x].module_status==TRUE)
					{
						printf("%d%s running (PID: %d)\n",x, RUNNING_MODULES[x].module_name,RUNNING_MODULES[x].module_PID);
					}
				}
				printf("Type in number of module to kill: (# for cancel)\n");
				scanf("%s",&symbol);
				if(symbol == '#')
					break;
				else
					kill(RUNNING_MODULES[symbol - '0'].module_PID,2);
 
				break;
			}

			case 4:
			{
				UpdateModuleStatus();
				for(x=0; x<RUNNING_MODULES_cnt; x++)
				{
					if(RUNNING_MODULES[x].module_status == FALSE && (RUNNING_MODULES[x].module_restart_cnt >= MAX_RESTARTS))
					{
						printf("Module: %d%s was restarted 3 times and it is down again.\n",x, RUNNING_MODULES[x].module_name );
					}
					else if(RUNNING_MODULES[x].module_status == FALSE)
					{

						RestartModule(x);
					}
				}
				break;
			}
			case 5:
			{
				UpdateModuleStatus();
				if(RUNNING_MODULES_cnt == 0)
				{
					printf("No module running.\n");
					break;
				}

				for(x=0; x<RUNNING_MODULES_cnt; x++)
				{
					if(RUNNING_MODULES[x].module_status == TRUE)
					{
						printf("%d%s running (PID: %d)\n",x, RUNNING_MODULES[x].module_name,RUNNING_MODULES[x].module_PID);
					}
					else 
					{
						printf("%d%s stopped (PID: %d)\n",x, RUNNING_MODULES[x].module_name,RUNNING_MODULES[x].module_PID);
					}
				}
				break;
			}
			case 6:
			{
				Print_Configuration();
				break;
			}
			default:
			{
				printf("spatnej vstup\n");
			}
		}
		UpdateModuleStatus();
	}

	free(MODULES);
	free(RUNNING_MODULES);


	return 0;
}