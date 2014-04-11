/*
 * This is automatically generated callbacks file
 * It contains 3 parts: Configuration callbacks, RPC callbacks and state data callbacks.
 * Do NOT alter function signatures or any structures unless you know exactly what you are doing.
 */

#include <stdlib.h>
#include <libxml/tree.h>
#include <libnetconf_xml.h>

#include "../supervisor.h"
#include "../supervisor_api.h"

extern running_module_t *running_modules;
extern int running_modules_cnt;

/* transAPI version which must be compatible with libnetconf */
int transapi_version = 4;

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
	char *argv[] = {
		"supervisor", "-f", "/tmp/supervisor-running-config.xml"
	};
	int argc = 3;
	LIBXML_TEST_VERSION;
	if (*running == NULL) {
		/* empty running */
		xmlDocPtr nd = xmlNewDoc(BAD_CAST "1.0");
		xmlNodePtr rd = NULL;
		if (nd != NULL) {
			rd = xmlNewNode(NULL, BAD_CAST "nemea-supervisor");
			if (rd != NULL) {
				xmlDocSetRootElement(nd, rd);
				xmlDocDump(stderr, nd);
				if (xmlSaveFormatFile(argv[2], nd, 0) == -1) {
					xmlFreeDoc(nd);
					fprintf(stderr,
						"Save running into temp file failed (%s).\n",
						argv[2]);
					return EXIT_FAILURE;
				}
			} else {
				fprintf(stderr,
					"Creation of empty conf failed.\n");
				return EXIT_FAILURE;
			}
		} else {
			fprintf(stderr, "Creation of empty conf failed.\n");
			return EXIT_FAILURE;
		}

	} else {
		xmlDocDump(stderr, *running);
		if (xmlSaveFormatFile(argv[2], *running, 0) == -1) {
			xmlDocDump(stderr, *running);
			fprintf(stderr,
				"Save running into temp file failed.\n");
			return EXIT_FAILURE;
		}
	}

	if (api_initialization(&argc, argv) == 0) {
		fprintf(stderr, "Supervisor API initialization failed.\n");
		//return EXIT_FAILURE;
	}
	unlink(argv[2]);
	return EXIT_SUCCESS;
}

/**
 * @brief Free all resources allocated on plugin runtime and prepare plugin for removal.
 */
void transapi_close(void)
{
	fprintf(stderr, "Supervisor API cleanup.\n");
	api_quit();
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
xmlDocPtr get_state_data(xmlDocPtr model, xmlDocPtr running,
			 struct nc_err ** err)
{
	int i;
	const char *templ =
	    "<?xml version=\"1.0\"?><nemea-supervisor xmlns=\"urn:cesnet:tmc:nemea:1.0\"><modules/></nemea-supervisor>";
	xmlDocPtr resp = NULL;
	xmlNodePtr modules, module;

	if (running_modules_cnt > 0) {
		resp = xmlParseMemory(templ, strlen(templ));
		if (resp == NULL) {
			return NULL;
		}
		modules = xmlDocGetRootElement(resp);
		modules = modules->children;
		for (i = 0; i < running_modules_cnt; ++i) {
			module =
			    xmlNewChild(modules, NULL, BAD_CAST "module", NULL);
			xmlNewChild(module, NULL, BAD_CAST "name",
				    running_modules[i].module_name);
			if (running_modules[i].module_status == TRUE) {
				xmlNewChild(module, NULL, BAD_CAST "running",
					    "true");
			} else {
				xmlNewChild(module, NULL, BAD_CAST "running",
					    "false");
			}
			/* TODO check and free */
			if (xmlAddChild(modules, module) == NULL) {
				xmlFree(module);
			}
		}
	}
	return resp;
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
									 **data,
									 XMLDIFF_OP
									 op,
									 xmlNodePtr
									 node,
									 struct
									 nc_err
									 **error)
{
	xmlNodePtr module = NULL;
	int index = -1;
	module = node->parent;
	if (module == NULL) {
		(*error) = nc_err_new(NC_ERR_MISSING_ELEM);
		return EXIT_FAILURE;
	}
	switch (op) {
	case XMLDIFF_ADD:
		//api_add_module(module);
		break;
	case XMLDIFF_REM:
		//api_remove_module(module);
		//cleanup module name in internal array
		break;
	case XMLDIFF_MOD:
		//index = super_find_index(module);
		if (index == -1) {
			/* add new module that was not found */
			//api_add_module(module);
		} else {
			xmlNodePtr t = node->children;
			xmlChar *value = NULL;
			for (t = node->children; t != NULL; t = t->next) {
				if (t->type == XML_TEXT_NODE) {
					value = xmlNodeGetContent(node);
					break;
				}
			}

			if (value != NULL) {
				if (xmlStrcmp(value, BAD_CAST "true") == 0) {
					fprintf(stderr, "Starting module %s.\n",
						value);
					//api_start_module();
				} else {
					fprintf(stderr, "Starting module %s.\n",
						value);
					//api_stop_module();
				}
				xmlFree(value);
			}
		}
		break;
	}
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
int callback_nemea_nemea_supervisor_nemea_modules_nemea_module(void **data,
							       XMLDIFF_OP op,
							       xmlNodePtr node,
							       struct nc_err
							       **error)
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
int callback_nemea_nemea_supervisor_nemea_modules(void **data, XMLDIFF_OP op,
						  xmlNodePtr node,
						  struct nc_err **error)
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
int callback_nemea_nemea_supervisor(void **data, XMLDIFF_OP op, xmlNodePtr node,
				    struct nc_err **error)
{
	return EXIT_SUCCESS;
}

/*
* Structure transapi_config_callbacks provide mapping between callback and path in configuration datastore.
* It is used by libnetconf library to decide which callbacks will be run.
* DO NOT alter this structure
*/
struct transapi_data_callbacks clbks = {
	.callbacks_count = 4,
	.data = NULL,
	.callbacks = {
		      {.path =
		       "/nemea:nemea-supervisor/nemea:modules/nemea:module/nemea:enabled",.
		       func =
		       callback_nemea_nemea_supervisor_nemea_modules_nemea_module_nemea_enabled},
		      {.path =
		       "/nemea:nemea-supervisor/nemea:modules/nemea:module",.
		       func =
		       callback_nemea_nemea_supervisor_nemea_modules_nemea_module},
		      {.path = "/nemea:nemea-supervisor/nemea:modules",.func =
		       callback_nemea_nemea_supervisor_nemea_modules},
		      {.path = "/nemea:nemea-supervisor",.func =
		       callback_nemea_nemea_supervisor}
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
