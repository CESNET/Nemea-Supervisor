typedef struct parameter_s {
	char   short_opt;
	char  *long_opt;
	char  *description;
	int param_required_argument; // 1 - required, 0 - optional
	char  *argument_type; // NULL - no argument
} parameter_t;


typedef struct module_info_test_s {
	char  *name;
	char *description;
	uint num_in_ifc;
	uint num_out_ifc;
	parameter_t * params;
} module_info_test_t;

#define GEN_LONG_OPT_STRUCT_LINE(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) {p_long_opt, p_required_argument, 0, p_short_opt},

#define GEN_LONG_OPT_STRUCT(PARAMS) \
   static struct option long_options[] = { \
      PARAMS(GEN_LONG_OPT_STRUCT_LINE) \
      {0, 0, 0, 0} \
   };

#define ALLOCATE_BASIC_INFO(p_name, p_description, p_input, p_output) \
  if (p_name != NULL) { \
  	module_info -> name = strdup(p_name); \
  } else { \
  	module_info -> name = NULL; \
  } \
  if (p_description != NULL) { \
  	module_info -> description = strdup(p_description); \
  } else { \
  	module_info -> description = NULL; \
  } \  
  module_info -> num_in_ifc = p_input; \
  module_info -> num_out_ifc = p_output;

#define ALLOCATE_PARAM_ITEMS(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) \
  if (p_short_opt != 0) { \
  	module_info -> params[trap_info_cnt].short_opt = p_short_opt; \
  } else { \
  	module_info -> params[trap_info_cnt].short_opt = 0; \
  } \
  if (p_long_opt != NULL) { \
  	module_info -> params[trap_info_cnt].long_opt = strdup(p_long_opt); \
  } else { \
  	module_info -> params[trap_info_cnt].long_opt = NULL; \
  } \
  if (p_description != NULL) { \
  	module_info -> params[trap_info_cnt].description = strdup(p_description); \
  } else { \
  	module_info -> params[trap_info_cnt].description = NULL; \
  } \
  if (p_required_argument == 1) { \
  	module_info -> params[trap_info_cnt].param_required_argument = p_required_argument; \
  } else { \
  	module_info -> params[trap_info_cnt].param_required_argument = 0; \
  } \
  if (p_argument_type != NULL) { \
  	module_info -> params[trap_info_cnt].argument_type = strdup(p_argument_type); \
  } else { \
  	module_info -> params[trap_info_cnt].argument_type = NULL; \
  } \
  trap_info_cnt++;
  
#define FREE_BASIC_INFO(p_name, p_description, p_input, p_output) \
  if (module_info -> name != NULL) { \
	free(module_info -> name); \
	module_info -> name = NULL; \
  } \
  if (module_info -> description != NULL) { \
  	free(module_info -> description); \
	module_info -> description = NULL; \
  }

#define FREE_PARAM_ITEMS(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) \
  if (module_info -> params[trap_info_cnt].long_opt != NULL) { \
	free(module_info -> params[trap_info_cnt].long_opt); \
	module_info -> params[trap_info_cnt].long_opt= NULL; \
  } \
  if (module_info -> params[trap_info_cnt].description != NULL) { \
 	free(module_info -> params[trap_info_cnt].description); \
	module_info -> params[trap_info_cnt].description= NULL; \
  } \
  if (module_info -> params[trap_info_cnt].argument_type != NULL) { \
	free(module_info -> params[trap_info_cnt].argument_type); \
	module_info -> params[trap_info_cnt].argument_type= NULL; \
  } \
  trap_info_cnt++;

#define COUNT_MODULE_PARAMS(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) trap_module_params_cnt++;

#define INIT_MODULE_INFO_STRUCT(BASIC, PARAMS) \
  int trap_info_cnt = 0; \
  int trap_module_params_cnt = 0; \
  module_info = (module_info_test_t *) calloc (1, sizeof(module_info_test_t)); \
  BASIC(ALLOCATE_BASIC_INFO) \
  PARAMS(COUNT_MODULE_PARAMS) \
  module_info -> params = (parameter_t *) calloc (trap_module_params_cnt, sizeof(parameter_t)); \
  PARAMS(ALLOCATE_PARAM_ITEMS)

#define FREE_MODULE_INFO_STRUCT(BASIC, PARAMS) \
  trap_info_cnt = 0; \
  BASIC(FREE_BASIC_INFO) \
  if (module_info -> params != NULL) { \
  	PARAMS(FREE_PARAM_ITEMS) \
	free(module_info -> params); \
	module_info -> params = NULL; \
  } \
  free(module_info); \
  module_info = NULL;