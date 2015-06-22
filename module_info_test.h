/** Structure with information about one module parameter
 *  Every parameter contains short_opt, long_opt, description,
 *  flag whether the parameter requires argument and argument type.
 */
typedef struct trap_module_info_parameter_s {
   char   short_opt;
   char  *long_opt;
   char  *description;
   int param_required_argument;
   char  *argument_type;
} trap_module_info_parameter_t;

/** Structure with information about module
 *  This struct contains basic information about the module, such as module's
 *  name, number of interfaces etc. It's supposed to be filled with static data
 *  and passed to trap_init function.
 */
typedef struct trap_module_info_s {
   char *name;           ///< Name of the module (short string)
   char *description;    /**< Detialed description of the module, can be a long
                              string with several lines or even paragraphs. */
   int num_ifc_in;  ///< Number of input interfaces
   int num_ifc_out; ///< Number of output interfaces
   // TODO more ... (e.g. UniRec specifiers)
   trap_module_info_parameter_t ** params;
} trap_module_info_t;

/** Macro generating one line of long_options field for getopt_long function
 */
#define GEN_LONG_OPT_STRUCT_LINE(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) {p_long_opt, p_required_argument, 0, p_short_opt},

/** Macro generating long_options field for getopt_long function according to given macro with parameters
 */
#define GEN_LONG_OPT_STRUCT(PARAMS) \
   static struct option long_options[] = { \
      PARAMS(GEN_LONG_OPT_STRUCT_LINE) \
      {0, 0, 0, 0} \
   };

/** Macro for allocation and initialization of module basic information (name, description and number of input/output interfaces) in global variable module_info
 */
#define ALLOCATE_BASIC_INFO(p_name, p_description, p_input, p_output) \
  if (p_name != NULL) { \
    module_info->name = strdup(p_name); \
  } else { \
    module_info->name = NULL; \
  } \
  if (p_description != NULL) { \
    module_info->description = strdup(p_description); \
  } else { \
    module_info->description = NULL; \
  } \
  module_info->num_ifc_in = p_input; \
  module_info->num_ifc_out = p_output;

/** Macro for allocation and initialization of module parameters information (short_opt, long_opt, description etc.) in global variable module_info
 */
#define ALLOCATE_PARAM_ITEMS(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) \
  if (module_info->params[trap_info_cnt] == NULL) { \
    module_info->params[trap_info_cnt] = (trap_module_info_parameter_t *) calloc (1, sizeof(trap_module_info_parameter_t)); \
  } \
  if (p_short_opt != 0) { \
    module_info->params[trap_info_cnt]->short_opt = p_short_opt; \
  } else { \
    module_info->params[trap_info_cnt]->short_opt = 0; \
  } \
  if (p_long_opt != NULL) { \
    module_info->params[trap_info_cnt]->long_opt = strdup(p_long_opt); \
  } else { \
    module_info->params[trap_info_cnt]->long_opt = NULL; \
  } \
  if (p_description != NULL) { \
    module_info->params[trap_info_cnt]->description = strdup(p_description); \
  } else { \
    module_info->params[trap_info_cnt]->description = NULL; \
  } \
  if (p_required_argument == 1) { \
    module_info->params[trap_info_cnt]->param_required_argument = p_required_argument; \
  } else { \
    module_info->params[trap_info_cnt]->param_required_argument = 0; \
  } \
  if (p_argument_type != NULL) { \
    module_info->params[trap_info_cnt]->argument_type = strdup(p_argument_type); \
  } else { \
    module_info->params[trap_info_cnt]->argument_type = NULL; \
  } \
  trap_info_cnt++;
  
/** Macro releasing memory allocated for module basic information in global variable module_info
 */
#define FREE_BASIC_INFO(p_name, p_description, p_input, p_output) \
  if (module_info->name != NULL) { \
  free(module_info->name); \
  module_info->name = NULL; \
  } \
  if (module_info->description != NULL) { \
    free(module_info->description); \
  module_info->description = NULL; \
  }

/** Macro releasing memory allocated for module parameters information in global variable module_info
  */
#define FREE_PARAM_ITEMS(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) \
  if (module_info->params[trap_info_cnt]->long_opt != NULL) { \
  free(module_info->params[trap_info_cnt]->long_opt); \
  module_info->params[trap_info_cnt]->long_opt= NULL; \
  } \
  if (module_info->params[trap_info_cnt]->description != NULL) { \
  free(module_info->params[trap_info_cnt]->description); \
  module_info->params[trap_info_cnt]->description= NULL; \
  } \
  if (module_info->params[trap_info_cnt]->argument_type != NULL) { \
  free(module_info->params[trap_info_cnt]->argument_type); \
  module_info->params[trap_info_cnt]->argument_type= NULL; \
  } \
  if (module_info->params[trap_info_cnt] != NULL) { \
    free(module_info->params[trap_info_cnt]); \
  } \
  trap_info_cnt++;

/** Macro counting number of module parameters - memory allocation purpose
 */
#define COUNT_MODULE_PARAMS(p_short_opt, p_long_opt, p_description, p_required_argument, p_argument_type) trap_module_params_cnt++;

/** Macro initializing whole module_info structure in global variable module_info
 *  First argument is macro defining module basic information (name, description, number of input/output interfaces)
 *  Second argument is macro defining all module parameters and their values
 *  Last pointer is NULL to detect end of parameters without counter
 */
#define INIT_MODULE_INFO_STRUCT(BASIC, PARAMS) \
  int trap_info_cnt = 0; \
  int trap_module_params_cnt = 0; \
  module_info = (trap_module_info_t *) calloc (1, sizeof(trap_module_info_t)); \
  BASIC(ALLOCATE_BASIC_INFO) \
  PARAMS(COUNT_MODULE_PARAMS) \
  module_info->params = (trap_module_info_parameter_t **) calloc (trap_module_params_cnt + 1, sizeof(trap_module_info_parameter_t *)); \
  PARAMS(ALLOCATE_PARAM_ITEMS)

/** Macro releasing whole module_info structure allocated in global variable module_info
 *  First argument is macro defining module basic information (name, description, number of input/output interfaces)
 *  Second argument is macro defining all module parameters and their values
*/
#define FREE_MODULE_INFO_STRUCT(BASIC, PARAMS) \
  trap_info_cnt = 0; \
  BASIC(FREE_BASIC_INFO) \
  if (module_info->params != NULL) { \
    PARAMS(FREE_PARAM_ITEMS) \
  free(module_info->params); \
  module_info->params = NULL; \
  } \
  free(module_info); \
  module_info = NULL;
  