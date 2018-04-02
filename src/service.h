/**
 * @file service.h
 * @brief This module provides functions for handling connections to libtrap's service interface of individual instances and gather statistics via this interface.
 */

#include "module.h"



//TODO
extern int send_ifc_stats_request(const inst_t *inst);

//TODO
extern int recv_ifc_stats(inst_t *inst);

//TODO
extern void connect_to_inst(inst_t *inst);

//TODO
extern void disconnect_from_inst(inst_t *inst);