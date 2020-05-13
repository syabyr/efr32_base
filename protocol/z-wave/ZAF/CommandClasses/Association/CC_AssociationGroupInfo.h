/**
 * @file
 * Handler for Command Class Association Group Info.
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CC_ASSOCIATION_GROUP_INFO_H_
#define _CC_ASSOCIATION_GROUP_INFO_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <ZW_classcmd.h>
#include <CC_Common.h>
#include <ZW_TransportEndpoint.h>

/**
 * For backwards compatibility.
 */
#define handleCommandClassAssociationGroupInfo(a, b, c) CC_AGI_handler(a, b, c)
#define GetApplGroupName(a, b, c)                       CC_AGI_groupNameGet_handler(a, b, c)
#define GetApplGroupInfo(a, b, c)                       CC_AGI_groupInfoGet_handler(a, b, c)
#define GetApplAssoGroupsSize(a)                        CC_AGI_groupCount_handler(a)

/**
 * @brief Read AGI group name
 * @param[out] pGroupName OUT point to group name
 * @param[in] groupId IN group identifier
 * @param[in] endpoint IN end-point number
 */
extern uint8_t CC_AGI_groupNameGet_handler(
    char * pGroupName,
    uint8_t groupId,
    uint8_t endpoint);

/**
 * @brief Get application specific Group Info
 * @param[in] groupId group identifier
 * @param[in] endpoint is the endpoint number
 * @param[out] report pointer to data of type VG_ASSOCIATION_GROUP_INFO_REPORT_VG
 */
extern void CC_AGI_groupInfoGet_handler(
  uint8_t groupId,
  uint8_t endpoint,
  VG_ASSOCIATION_GROUP_INFO_REPORT_VG* report);

/**
 * @brief Returns the number of association groups for a given endpoint.
 * @param[in] endpoint A given endpoint where 0 is the root device.
 * @return Number of association groups.
 */
extern uint8_t CC_AGI_groupCount_handler(uint8_t endpoint);

/**
 * @brief Set Application specific Group Command List
 * @param[out] pGroupList pointer to the list
 * @param[in] groupId group identifier
 * @param[in] endpoint is the endpoint number
 * @return command status (true/false)
 */
extern bool GetApplGroupCommandList(
    uint8_t * pGroupList,
    uint8_t groupId,
    uint8_t endpoint);

/**
 * @brief Application specific Group Command List Size
 * @param[in] groupId group identifier
 * @param[in] endpoint is the endpoint number
 * @return size
 */
extern uint8_t GetApplGroupCommandListSize(
    uint8_t groupId,
    uint8_t endpoint);

/**
 * @brief Handler for Association Group Info Command Class.
 * @param[in] rxOpt receive options of type RECEIVE_OPTIONS_TYPE_EX
 * @param[in] pCmd Payload from the received frame
 * @param[in] cmdLength Number of command bytes including the command
 * @return receive frame status.
 */
received_frame_status_t CC_AGI_handler(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength);

/**
 * Corrects a given group ID if it's invalid.
 *
 * According to CC:0059.01.05.12.002 in SDS13782-4 "Association Group Command List Report" SHOULD
 * respond with commands for group 1 if the given group ID is invalid (0 or higher than the number
 * of groups for a given endpoint).
 * @param endpoint Endpoint to match against.
 * @param pGroupId Pointer to group ID.
 */
extern void ZAF_CC_AGI_CorrectGroupIdIfInvalid(uint8_t endpoint, uint8_t * pGroupId);

#endif
