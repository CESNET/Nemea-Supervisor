/*
 * This is automatically generated callbacks file
 * It contains 3 parts: Configuration callbacks, RPC callbacks and state data callbacks.
 * Do NOT alter function signatures or any structures unless you know exactly what you are doing.
 */

#include <stdlib.h>
#include <libxml/tree.h>
#include <libnetconf_xml.h>
#include <string.h>

#include "../supervisor_api.h"
#include "../internal.h"
#include "ncnemea.h"

/* transAPI version which must be compatible with libnetconf */
int transapi_version = 6;

/* Signal to libnetconf that configuration data were modified by any callback.
 * 0 - data not modified
 * 1 - data have been modified
 */
int config_modified = 0;

/*
 * Determines the callbacks order.
 * Set this variable before compilation and DO NOT modify it in runtime.
 * TRANSAPI_CLBCKS_LEAF_TO_ROOT (default)
 * TRANSAPI_CLBCKS_ROOT_TO_LEAF
 */
const TRANSAPI_CLBCKS_ORDER_TYPE callbacks_order =
    TRANSAPI_CLBCKS_ORDER_DEFAULT;

/* Do not modify or set! This variable is set by libnetconf to announce edit-config's error-option
Feel free to use it to distinguish module behavior for different error-option values.
 * Possible values:
 * NC_EDIT_ERROPT_STOP - Following callback after failure are not executed, all successful callbacks executed till
                         failure point must be applied to the device.
 * NC_EDIT_ERROPT_CONT - Failed callbacks are skipped, but all callbacks needed to apply configuration changes are executed
 * NC_EDIT_ERROPT_ROLLBACK - After failure, following callbacks are not executed, but previous successful callbacks are
                         executed again with previous configuration data to roll it back.
 */
NC_EDIT_ERROPT_TYPE erropt = NC_EDIT_ERROPT_NOTSET;

/**
 * @brief Initialize plugin after loaded and before any other functions are called.

 * This function should not apply any configuration data to the controlled device. If no
 * running is returned (it stays *NULL), complete startup configuration is consequently
 * applied via module callbacks. When a running configuration is returned, libnetconf
 * then applies (via module's callbacks) only the startup configuration data that
 * differ from the returned running configuration data.

 * Please note, that copying startup data to the running is performed only after the
 * libnetconf's system-wide close - see nc_close() function documentation for more
 * information.

 * @param[out] running	Current configuration of managed device.

 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int transapi_init(xmlDocPtr * running)
{
	xmlNodePtr node = xmlDocGetRootElement(*running);
	netconf_supervisor_initialization(&node);
	VERBOSE(N_STDOUT,"-- Transapi init --\n");
	return EXIT_SUCCESS;
}

/**
 * @brief Free all resources allocated on plugin runtime and prepare plugin for removal.
 */
void transapi_close(void)
{
	VERBOSE(N_STDOUT,"-- Transapi close --\n");
	supervisor_termination(TRUE,FALSE);
	return;
}

/**
 * @brief Retrieve state data from device and return them as XML document
 *
 * @param model	Device data model. libxml2 xmlDocPtr.
 * @param running	Running datastore content. libxml2 xmlDocPtr.
 * @param[out] err  Double pointer to error structure. Fill error when some occurs.
 * @return State data as libxml2 xmlDocPtr or NULL in case of error.
 */
xmlDocPtr get_state_data(xmlDocPtr model __attribute__ ((unused)), xmlDocPtr running __attribute__ ((unused)),
			 struct nc_err ** err __attribute__ ((unused)))
{
	return netconf_get_state_data();
}

/*
 * Mapping prefixes with namespaces.
 * Do NOT modify this structure!
 */
char *namespace_mapping[] = { "nemea", "urn:cesnet:tmc:nemea:1.0", NULL, NULL };

/*
* CONFIGURATION callbacks
* Here follows set of callback functions run every time some change in associated part of running datastore occurs.
* You can safely modify the bodies of all function as well as add new functions for better lucidity of code.
*/

/**
 * @brief This callback will be run when node in path /nemea:nemea-supervisor/nemea:modules/nemea:module/nemea:enabled changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int
callback_nemea_nemea_supervisor_nemea_modules_nemea_module_nemea_enabled(void
									 **data __attribute__ ((unused)),
									 XMLDIFF_OP
									 op __attribute__ ((unused)),
									 xmlNodePtr
									 node __attribute__ ((unused)),
									 struct
									 nc_err
									 **error __attribute__ ((unused)))
{
	// if (output_file_stream != NULL) {
	// 	fprintf(output_file_stream, "Nemea plugin callback> callback_nemea_nemea_supervisor_nemea_modules_nemea_module_nemea_enabled\n");
	// 	fflush(output_file_stream);
	// }
	// xmlNodePtr module = NULL;
	// int index = -1;
	// module = node->parent;
	// if (module == NULL) {
	// 	(*error) = nc_err_new(NC_ERR_MISSING_ELEM);
	// 	return EXIT_FAILURE;
	// }
	// switch (op) {
	// case XMLDIFF_ADD:
	// 	//api_add_module(module);
	// 	break;
	// case XMLDIFF_REM:
	// 	//api_remove_module(module);
	// 	//cleanup module name in internal array
	// 	break;
	// case XMLDIFF_MOD:
	// 	//index = super_find_index(module);
	// 	if (index == -1) {
	// 		/* add new module that was not found */
	// 		//api_add_module(module);
	// 	} else {
	// 		xmlNodePtr t = node->children;
	// 		xmlChar *value = NULL;
	// 		for (t = node->children; t != NULL; t = t->next) {
	// 			if (t->type == XML_TEXT_NODE) {
	// 				value = xmlNodeGetContent(node);
	// 				break;
	// 			}
	// 		}

	// 		if (value != NULL) {
	// 			if (xmlStrcmp(value, BAD_CAST "true") == 0) {
	// 				fprintf(stderr, "Starting module %s.\n",
	// 					value);
	// 				//api_start_module();
	// 			} else {
	// 				fprintf(stderr, "Starting module %s.\n",
	// 					value);
	// 				//api_stop_module();
	// 			}
	// 			xmlFree(value);
	// 		}
	// 	}
	// 	break;
	// }
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /nemea:nemea-supervisor/nemea:modules/nemea:module changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_nemea_nemea_supervisor_nemea_modules_nemea_module(void **data __attribute__ ((unused)),
							       XMLDIFF_OP op __attribute__ ((unused)),
							       xmlNodePtr node __attribute__ ((unused)),
							       struct nc_err
							       **error __attribute__ ((unused)))
{
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /nemea:nemea-supervisor/nemea:modules changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_nemea_nemea_supervisor_nemea_modules(void **data __attribute__ ((unused)), XMLDIFF_OP op __attribute__ ((unused)),
						  xmlNodePtr node __attribute__ ((unused)),
						  struct nc_err **error __attribute__ ((unused)))
{
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /nemea:nemea-supervisor changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_nemea_nemea_supervisor(void **data, XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err **error)
{
	VERBOSE(N_STDOUT, "Callback supervisor... \n");
	reload_configuration(RELOAD_CALLBACK_ROOT_ELEM, &new_node);
	interactive_show_available_modules();
	return EXIT_SUCCESS;
}

/*
* Structure transapi_config_callbacks provide mapping between callback and path in configuration datastore.
* It is used by libnetconf library to decide which callbacks will be run.
* DO NOT alter this structure
*/
struct transapi_data_callbacks clbks = {
	.callbacks_count = 1,
	.data = NULL,
	.callbacks = {
		      {.path = "/nemea:nemea-supervisor",.func = callback_nemea_nemea_supervisor}
		     }
};

/*
* RPC callbacks
* Here follows set of callback functions run every time RPC specific for this device arrives.
* You can safely modify the bodies of all function as well as add new functions for better lucidity of code.
* Every function takes array of inputs as an argument. On few first lines they are assigned to named variables. Avoid accessing the array directly.
* If input was not set in RPC message argument in set to NULL.
*/

/*
* Structure transapi_rpc_callbacks provide mapping between callbacks and RPC messages.
* It is used by libnetconf library to decide which callbacks will be run when RPC arrives.
* DO NOT alter this structure
*/
struct transapi_rpc_callbacks rpc_clbks = {
	.callbacks_count = 0,
	.callbacks = {
		      }
};

void netconf_notify(int module_event, const char * module_name)
{
	char buffer[300];
	memset(buffer,0,300);
	switch (module_event) {
	case MODULE_EVENT_STARTED:
		VERBOSE(N_STDOUT,"Notify -started-\n");
		sprintf(buffer,"<moduleStatusChanged><moduleName>%s</moduleName><moduleStatus>started</moduleStatus></moduleStatusChanged>",module_name);
		ncntf_event_new(-1, NCNTF_GENERIC, buffer);
		break;
	case MODULE_EVENT_STOPPED:
		VERBOSE(N_STDOUT,"Notify -stopped-\n");
		sprintf(buffer,"<moduleStatusChanged><moduleName>%s</moduleName><moduleStatus>stopped</moduleStatus></moduleStatusChanged>",module_name);
		ncntf_event_new(-1, NCNTF_GENERIC, buffer);
		break;
	case MODULE_EVENT_RESTARTED:
		VERBOSE(N_STDOUT,"Notify -restarted-\n");
		sprintf(buffer,"<moduleStatusChanged><moduleName>%s</moduleName><moduleStatus>restarted</moduleStatus><reason>Module was not running and restart_counter &lt; MAX/minute.</reason></moduleStatusChanged>",module_name);
		ncntf_event_new(-1, NCNTF_GENERIC, buffer);
		break;
	case MODULE_EVENT_DISABLED:
		VERBOSE(N_STDOUT,"Notify -disabled-\n");
		sprintf(buffer,"<moduleStatusChanged><moduleName>%s</moduleName><moduleStatus>disabled</moduleStatus><reason>Restart_counter reached MAX/minute.</reason></moduleStatusChanged>",module_name);
		ncntf_event_new(-1, NCNTF_GENERIC, buffer);
		break;
	default:
		return;
	}
}