/**
 * @file service.c
 * @brief Implementation of functions defined in service.h
 */
#include <netdb.h>
#include <sys/un.h>
#include <libtrap/trap.h>

#include "service.h"
#include "inst_control.h"

/**
 * @brief Timeout period for communication with service interface UNIX socket
 * */
#define SERVICE_WAIT_BEFORE_TIMEOUT 25000



/**
 * @brief Types of service interface messages
 * */
typedef enum service_msg_header_type_e {
   SERVICE_GET_COM = 10,
   SERVICE_OK_REPLY = 12
} service_msg_header_type_t;

/**
 * @brief Service interface message header structure
 * */
typedef struct service_msg_header_s {
   service_msg_header_type_t com; ///< Type of service interface message
   uint32_t data_size; ///< Length of service interface message
} service_msg_header_t;


/**
 * @brief
 * @param
 * @return -1 on error, 0 on success
 * */
static int recv_data(inst_t *inst, uint32_t size, void *data);

/**
 * @brief Loads statistics from given JSON string
 * @param data JSON string to load statistics from
 * @param inst Instance to load statistics into
 * @return -1 on error, 0 on success
 * */
static int load_stats_json(char *data, inst_t *inst);


void disconnect_from_inst(inst_t *inst)
{
   if (inst->service_ifc_connected) {
      VERBOSE(V3,"Disconnecting from inst '%s'", inst->name);
      // Close if not closed already
      if (inst->service_sd != -1) {
         close(inst->service_sd);
         inst->service_sd = -1;
      }
      inst->service_ifc_connected = false;
   }
   inst->service_ifc_conn_timer = 0;
}

int recv_ifc_stats(inst_t *inst)
{
   char *buffer = NULL;
   int rc;
   uint32_t buffer_size = 0;
   service_msg_header_t resp_header;
   resp_header.com = SERVICE_GET_COM;
   resp_header.data_size = 0;

   // Check whether the inst is running and is connected with supervisor via service interface
   if (false == inst->running || false == inst->service_ifc_connected) {
      return -1;
   }

   VERBOSE(V3, "Receiving reply from inst '%s'", inst->name)
   // Receive reply header
   rc = recv_data(inst, sizeof(service_msg_header_t), (void *) &resp_header);
   if (rc == -1) {
      VERBOSE(N_ERR, "Error while receiving reply header from inst '%s'.",
              inst->name)
      goto err_cleanup;
   }

   // Check if the reply is OK
   if (resp_header.com != SERVICE_OK_REPLY) {
      VERBOSE(N_ERR, "Wrong reply from inst '%s'.", inst->name)
      goto err_cleanup;
   }

   if (resp_header.data_size > buffer_size) {
      // Reallocate buffer for incoming data
      buffer_size += (resp_header.data_size - buffer_size) + 1;
      buffer = (char *) calloc(1, buffer_size * sizeof(char));
   }

   VERBOSE(V3, "Receiving stats from inst '%s'", inst->name)
   // Receive inst stats in json format
   if (recv_data(inst, resp_header.data_size, (void *) buffer) == -1) {
      VERBOSE(N_ERR, "Error while receiving stats from inst %s.", inst->name)
      goto err_cleanup;
   }

   // Decode json and save stats into inst structure
   VERBOSE(V3, "Received JSON: %s", buffer)
   if (load_stats_json(buffer, inst) == -1) {
      VERBOSE(N_ERR, "Error while receiving stats from inst '%s'.", inst->name);
      goto err_cleanup;
   }

   NULLP_TEST_AND_FREE(buffer)

   return 0;

err_cleanup:
   disconnect_from_inst(inst);
   NULLP_TEST_AND_FREE(buffer)
   return -1;
}

int send_ifc_stats_request(const inst_t *inst)
{
   service_msg_header_t req_header;
   req_header.com = SERVICE_GET_COM;
   req_header.data_size = 0;

   ssize_t sent = 0;
   uint32_t req_size = sizeof(req_header);
   uint32_t num_of_timeouts = 0;
   uint32_t total_sent = 0;

   while (total_sent < req_size) {
      sent = send(inst->service_sd, (void *) (&req_header + total_sent),
                  req_size - total_sent, MSG_DONTWAIT);
      if (-1 == sent) {
         if (errno == EAGAIN || errno == EWOULDBLOCK) {
            num_of_timeouts++;
            if (num_of_timeouts >= 3) {
               VERBOSE(N_ERR, "Timeouted while sending to inst '%s'!", inst->name);
               return -1;
            } else {
               usleep(SERVICE_WAIT_BEFORE_TIMEOUT);
               continue;
            }
         }
         VERBOSE(N_ERR, "Error while sending to inst '%s'!", inst->name);
         return -1;
      }
      total_sent += sent;
   }
   return 0;

}

void connect_to_inst(inst_t *inst)
{
   /* sock_name size is length of "service_PID" where PID is
    *  max 5 chars (8 + 5 + 1 zero terminating) */
   char sock_name[14];
   int sockfd;
   //tcpip_socket_addr_t addr;
   struct sockaddr_un unix_sa;

   memset(sock_name, 0, 14 * sizeof(char));
   sprintf(sock_name, "service_%d", inst->pid);

   memset(&unix_sa, 0, sizeof(unix_sa));

   unix_sa.sun_family = AF_UNIX;
   snprintf(unix_sa.sun_path,
            sizeof(unix_sa.sun_path) - 1,
            trap_default_socket_path_format,
            sock_name);

   VERBOSE(V3, "Instance '%s' has socket %s", inst->name, unix_sa.sun_path)

   sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (sockfd == -1) {
      VERBOSE(N_ERR,"Error while opening socket for connection with inst %s.",
              inst->name)
      inst->service_ifc_connected = false;
      return;
   }
   if (connect(sockfd, (struct sockaddr *) &unix_sa, sizeof(unix_sa)) == -1) {
      VERBOSE(N_ERR,
              "Error while connecting to inst '%s' with socket '%s'",
              inst->name,
              unix_sa.sun_path)

      inst->service_ifc_connected = false;
      close(sockfd);
      return;
   }
   inst->service_sd = sockfd;
   inst->service_ifc_connected = true;
   inst->service_ifc_conn_timer = 0; // Successfully connected, reset connection timer
   VERBOSE(V3,"Connected to inst '%s'.", inst->name);
}

static int load_stats_json(char *data, inst_t *inst)
{
   size_t arr_idx = 0;

   json_error_t error;
   json_t *json_struct = NULL;
   json_t *ifces_arr = NULL;
   json_t *in_ifc_cnts  = NULL;
   json_t *out_ifc_cnts = NULL;
   json_t *value = NULL;

   uint32_t ifc_cnt = 0;
   const char *str = NULL;

   interface_t *ifc;
   ifc_in_stats_t *in_stats;
   ifc_out_stats_t *out_stats;

#define FETCH_VALUE_OR_ERR(json_object, key) do { \
   value = json_object_get((json_object), (key)) ;\
   if ((key) == NULL) { \
      VERBOSE(N_ERR, "Could not get JSON key '%s' on line %d", (key), __LINE__) \
      goto err_cleanup; \
   } \
} while (0);

#define IS_JSON_OBJ_OR_ERR(json_object) do { \
   if (json_is_object((json_object)) == 0) { \
      VERBOSE(N_ERR, "Loaded value is not JSON object on line %d", __LINE__) \
      goto err_cleanup; \
   } \
} while (0);

#define IS_JSON_ARR_OR_ERR(json_object) do { \
   if (json_is_array((json_object)) == 0) { \
      VERBOSE(N_ERR, "Loaded value is not JSON array on line %d", __LINE__) \
      goto err_cleanup; \
   } \
} while (0);

#define FETCH_IFC_ID_OR_ERR(json_object, stats) do { \
   FETCH_VALUE_OR_ERR((json_object), "ifc_id") \
   str = json_string_value(value); \
   if (str == NULL) { \
      VERBOSE(N_ERR, "Could not get JSON string value of 'id_ifc' on line %d", \
              __LINE__) \
      goto err_cleanup; \
   } \
   if ((stats)->id == NULL) { \
      (stats)->id = strdup(str); \
      if ((stats)->id == NULL) { \
         NO_MEM_ERR \
         goto err_cleanup; \
      } \
   } else if (strcmp((stats)->id, str) != 0) { \
      NULLP_TEST_AND_FREE((stats)->id) \
      (stats)->id = strdup(str); \
      if ((stats)->id == NULL) { \
         NO_MEM_ERR \
         goto err_cleanup; \
      } \
   } \
} while (0); \

   // Parse received instances counters in json format
   json_struct = json_loads(data , 0, &error);
   if (json_struct == NULL) {
      VERBOSE(N_ERR, "Could not convert instances stats to json structure on "
                     "line %d: %s", error.line, error.text);
      return -1;
   }
   IS_JSON_OBJ_OR_ERR(json_struct)

   // Check number of input interfaces the inst is running with
   FETCH_VALUE_OR_ERR(json_struct, "in_cnt")
   ifc_cnt = (uint32_t) json_integer_value(value);

   if (ifc_cnt != inst->in_ifces.total) {
      /* This could mean that Supervisor has different configuration
       *  than the inst is running with. */
      VERBOSE(N_ERR, "Instance '%s' has different number of IN interfaces (%d)"
                     " than configuration from supervisor (%d).",
              inst->name, ifc_cnt, inst->in_ifces.total)
      goto err_cleanup;
   }

   // Check number of output interfaces the inst is running with
   FETCH_VALUE_OR_ERR(json_struct, "out_cnt")
   ifc_cnt = (uint32_t) json_integer_value(value);

   if (ifc_cnt != inst->out_ifces.total) {
      /* This could mean that Supervisor has different configuration
       *  than the inst is running with. */
      VERBOSE(N_ERR, "Instance '%s' has different number of OUT interfaces (%d)"
                     " than configuration from supervisor (%d).",
              inst->name, ifc_cnt, inst->out_ifces.total)
      goto err_cleanup;
   }

   if (inst->in_ifces.total > 0) {
      /* Get value from the key "in" from json root elem (it should be an array
       *  of json objects - every object contains counters of one input interface) */
      FETCH_VALUE_OR_ERR(json_struct, "in")
      ifces_arr = value;
      IS_JSON_ARR_OR_ERR(ifces_arr)

      json_array_foreach(ifces_arr, arr_idx, in_ifc_cnts) {
         ifc = inst->in_ifces.items[arr_idx];
         if (ifc == NULL || (arr_idx + 1) > inst->in_ifces.total) {
            VERBOSE(N_ERR, "Instance stats specify more IN interfaces than supervisor"
                           " configured")
            goto err_cleanup;
         }
         IS_JSON_OBJ_OR_ERR(in_ifc_cnts)
         in_stats = ifc->stats;
         FETCH_VALUE_OR_ERR(in_ifc_cnts, "messages")
         in_stats->recv_msg_cnt = (uint64_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(in_ifc_cnts, "buffers")
         in_stats->recv_buff_cnt = (uint64_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(in_ifc_cnts, "ifc_type")
         in_stats->type = (char) json_integer_value(value);
         FETCH_VALUE_OR_ERR(in_ifc_cnts, "ifc_state")
         in_stats->state = (uint8_t) json_integer_value(value);
         FETCH_IFC_ID_OR_ERR(in_ifc_cnts, in_stats)
      }
   }


   if (inst->out_ifces.total > 0) {
      /* Get value from the key "out" from json root elem (it should be an array
       *  of json objects - every object contains counters of one output interface) */
      FETCH_VALUE_OR_ERR(json_struct, "out")
      ifces_arr = value;
      IS_JSON_ARR_OR_ERR(ifces_arr)

      json_array_foreach(ifces_arr, arr_idx, out_ifc_cnts) {
         ifc = inst->out_ifces.items[arr_idx];
         if (ifc == NULL || (arr_idx + 1) > inst->out_ifces.total) {
            VERBOSE(N_ERR, "Instance stats specify more OUT interfaces than supervisor"
                           " configured")
            goto err_cleanup;
         }
         IS_JSON_OBJ_OR_ERR(out_ifc_cnts)
         out_stats = ifc->stats;
         FETCH_VALUE_OR_ERR(out_ifc_cnts, "sent-messages")
         out_stats->sent_msg_cnt = (uint64_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(out_ifc_cnts, "dropped-messages")
         out_stats->dropped_msg_cnt = (uint64_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(out_ifc_cnts, "buffers")
         out_stats->sent_buff_cnt = (uint64_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(out_ifc_cnts, "autoflushes")
         out_stats->autoflush_cnt = (uint64_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(out_ifc_cnts, "num_clients")
         out_stats->num_clients = (uint32_t) json_integer_value(value);
         FETCH_VALUE_OR_ERR(out_ifc_cnts, "type")
         out_stats->type = (char) json_integer_value(value);
         FETCH_IFC_ID_OR_ERR(out_ifc_cnts, out_stats)
      }
   }

   json_decref(json_struct);
   return 0;

err_cleanup:
   if (json_struct != NULL) {
      json_decref(json_struct);
   }
   return -1;
}

static int recv_data(inst_t *inst, uint32_t size, void *data)
{
   int num_of_timeouts = 0;
   int total_received = 0;
   int last_received = 0;

   while (total_received < size) {
      last_received = (int) recv(inst->service_sd, data + total_received,
                                 size - total_received, MSG_DONTWAIT);
      if (last_received == 0) {
         VERBOSE(V3, "Instance service thread closed its socket, everything received!")
         return -1;
      } else if (last_received == -1) {
         if (errno == EAGAIN || errno == EWOULDBLOCK) {
            num_of_timeouts++;
            if (num_of_timeouts >= 3) {
               VERBOSE(N_ERR, "Timeout while receiving from inst '%s'!", inst->name)
               return -1;
            } else {
               usleep(SERVICE_WAIT_BEFORE_TIMEOUT);
               continue;
            }
         }
         VERBOSE(N_ERR, "Error while receiving from inst '%s'! (errno=%d)",
                 inst->name, errno)
         return -1;
      }
      total_received += last_received;
   }
   return 0;
}