#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include "module_info_test.h"

module_info_test_t * module_info = NULL;

/*******/
#define MODULE_BASIC_INFO(BASIC) \
  BASIC("name","description",1,1)

#define MODULE_PARAMS(PARAM) \
  PARAM('s', "long_opt", "description", 0, NULL) \
  PARAM('b', "long_opt2", "description2", 1, "argument_type") \
  PARAM('d', "long_opt3", "description3", 1, "argument_type") \
  PARAM('u', "long_opt4", "description4", 0, NULL) \
  PARAM('i', "long_opt5", "description5", 1, "argument_type") \
  PARAM('c', "long_opt6", "description6", 1, "argument_type")
/*******/

int main()
{
	uint x = 0;

	INIT_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);

	printf("--- Module_info structure after initialization ---\n");
	printf("Basic info: %s %s %d %d\nParams:\n", module_info -> name, module_info -> description, module_info -> num_in_ifc, module_info -> num_out_ifc);
	for (x = 0; x < trap_module_params_cnt; x++) {
		printf("-%c --%s %s %d %s\n", module_info -> params[x].short_opt, module_info -> params[x].long_opt, module_info -> params[x].description, module_info -> params[x].param_required_argument, module_info -> params[x].argument_type);
	}

	GEN_LONG_OPT_STRUCT(MODULE_PARAMS);

	x = 0;
	printf("\n--- Long opt structure after initialization ---\n");
	while (long_options[x].name != 0) {
		printf("{%s, %d, 0, %c}\n", long_options[x].name, long_options[x].has_arg, (char)long_options[x].val);
		x++;
	}

	FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);
	return 0;
}