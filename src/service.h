/**
 * @file service.h
 * @brief This module provides functions for handling connections to libtrap's service interface of individual instances and gathering statistics via this interface.
 */

#ifndef SERVICE_H
#define SERVICE_H
#include "module.h"

/**
 * @brief Sends request to receive statistics from instance's service interface
 * @return -1 on error, 0 on success
 * */
extern int send_ifc_stats_request(const inst_t *inst);

/**
 * @brief Receives statistics from instance's service interface. This works after calling send_ifc_stats_request
 * @param inst Instance to receive statistics for
 * @return -1 on error, 0 on success
 * */
extern int recv_ifc_stats(inst_t *inst);

/**
 * @brief Connect to service interface of given instance
 * @details Success is idicated by instance having service_ifc_connected=true, service_sd != -1 and service_ifc_conn_timer resetted
 * @param inst Instance to which supervisor connects
 * */
extern void connect_to_inst(inst_t *inst);

/**
 * @brief Disconnect from service interface of given instance
 * @details Success is indicated by instance having service_ifc_connected=true, service_ifc=-1
 * @param inst Instance from which supervisor disconnects
 * */
extern void disconnect_from_inst(inst_t *inst);

#endif