#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include "module_info_test.h"

module_info_test_t * module_info = NULL;


// Definition of basic module information - module name, module description, number of input and output interfaces
#define MODULE_BASIC_INFO(BASIC) \
  BASIC("name","description",1,1)

// Definition of module parameters - every parameter has short_opt, long_opt, description, flag whether argument is required or it is optional
// and argument type which is NULL in case the parameter does not need argument
#define MODULE_PARAMS(PARAM) \
  PARAM('s', "long_opt", "description", 0, NULL) \
  PARAM('b', "long_opt2", "description2", 1, "argument_type") \
  PARAM('d', "long_opt3", "description3", 1, "argument_type") \
  PARAM('u', "long_opt4", "description4", 0, NULL) \
  PARAM('i', "long_opt5", "description5", 1, "argument_type") \
  PARAM('c', "long_opt6", "description6", 1, "argument_type")


int main()
{
	uint x = 0;

	// Allocate and initialize module_info structure and all its members according to the MODULE_BASIC_INFO and MODULE_PARAMS definitions
	INIT_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);

	printf("--- Module_info structure after initialization ---\n");
	printf("Basic info: %s %s %d %d\nParams:\n", module_info -> name, module_info -> description, module_info -> num_in_ifc, module_info -> num_out_ifc);
	for (x = 0; x < trap_module_params_cnt; x++) {
		printf("-%c --%s %s %d %s\n", module_info -> params[x].short_opt, module_info -> params[x].long_opt, module_info -> params[x].description, module_info -> params[x].param_required_argument, module_info -> params[x].argument_type);
	}

	// Generate long_options array of structures for getopt_long function
	GEN_LONG_OPT_STRUCT(MODULE_PARAMS);

	x = 0;
	printf("\n--- Long_options structure after initialization ---\n");
	while (long_options[x].name != 0) {
		printf("{%s, %d, 0, %c}\n", long_options[x].name, long_options[x].has_arg, (char)long_options[x].val);
		x++;
	}

	// Release allocated memory for module_info structure
	FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);
	return 0;
}