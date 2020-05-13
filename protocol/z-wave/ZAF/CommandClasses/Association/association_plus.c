/**
 * @file
 * Helper module for Command Class Association and Command Class Multi Channel Association.
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <association_plus.h>
#include <stdbool.h>
#include "SizeOf.h"
#include "Assert.h"
#include <ZW_TransportLayer.h>
#include <CC_Association.h>
#include <CC_AssociationGroupInfo.h>
#include <string.h>
//#define DEBUGPRINT
#include "DebugPrint.h"
#include "ZAF_Common_interface.h"
#include <ZAF_file_ids.h>
#include <nvm3.h>
#include <ZAF_nvm3_app.h>

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

typedef enum _NVM_ACTION_
{
  READ_DATA,
  WRITE_DATA
}
NVM_ACTION;

typedef struct _ASSOCIATION_NODE_LIST_
{
  uint8_t * pNodeId; /*IN pointer to list of nodes*/
  uint8_t noOfNodes;  /*IN number of nodes in List*/
  MULTICHAN_DEST_NODE_ID * pMulChanNodeId; /*IN pointer to list of multi channel nodes*/
  uint8_t noOfMulchanNodes;  /*IN number of  multi channel nodes in List*/
}
ASSOCIATION_NODE_LIST;

#define OFFSET_CLASSCMD                       0x00
#define OFFSET_CMD                            0x01
#define OFFSET_PARAM_1                        0x02
#define OFFSET_PARAM_2                        0x03
#define OFFSET_PARAM_3                        0x04
#define OFFSET_PARAM_4                        0x05

#define FREE_VALUE 0xFF

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

/* Default values */

static ASSOCIATION_GROUP groups[NUMBER_OF_ENDPOINTS + 1][MAX_ASSOCIATION_GROUPS];

ASSOCIATION_ROOT_GROUP_MAPPING* pGroupMappingTable = NULL;
uint8_t numberOfGroupMappingEntries = 0;

nvm3_Handle_t* pFileSystem;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

static bool isGroupIdValid(uint8_t groupId, uint8_t endpoint);
static void ExtractCmdClassNodeList(
    ASSOCIATION_NODE_LIST* plist,
    ZW_MULTI_CHANNEL_ASSOCIATION_SET_1BYTE_V2_FRAME* pCmd,
    uint8_t cmdLength,
    uint8_t* pMultiChannelAssociation);
static bool AssGroupMappingLookUp(uint8_t* pEndpoint, uint8_t* pGroupID);
static void AssociationStoreAll(void);
static void NVM_Action(NVM_ACTION action);
static void RemoveAssociationsFromGroup(
    uint8_t cmdClass,
    uint8_t ep,
    uint8_t groupId,
    ASSOCIATION_NODE_LIST * pListOfNodes);

static inline bool IsFree(destination_info_t * pNode)
{
  return (FREE_VALUE == pNode->node.nodeId);
}

static inline void Free(destination_info_t * pNode)
{
  pNode->node.nodeId = FREE_VALUE;
}

static inline bool HasEndpoint(destination_info_t * pNode)
{
  return (1 == pNode->nodeInfo.BitMultiChannelEncap);
}

/**
 * Returns a pointer to the node in association table.
 * @param endpoint Endpoint to which the association was made.
 * @param groupID Group in which the association was made.
 * @param index Index of the node in the group.
 * @return
 */
static inline destination_info_t * GetNode(uint8_t endpoint, uint8_t groupID, uint8_t index)
{
  return &groups[endpoint][groupID - 1].subGrp[index];
}

/**
 * @brief Reorders nodes in a given group.
 * @param groupIden Given group ID.
 * @param ep Given endpoint.
 * @param emptyIndx Location in index which must be filled up.
 */
static void
ReorderGroupAfterRemove(
  uint8_t groupIden,
  uint8_t ep,
  uint8_t emptyIndx)
{
  uint8_t move;

  destination_info_t * pNodeToMove;
  destination_info_t * pNode;
  const uint32_t iArraySize = sizeof_array(groups[ep][groupIden].subGrp);
  for(move = emptyIndx; move < (iArraySize - 1); move++)
  {
    pNode       = GetNode(ep, groupIden + 1, move);
    pNodeToMove = GetNode(ep, groupIden + 1, move + 1);
    if (IsFree(pNodeToMove)) break;
    *pNode = *pNodeToMove;
  }

  Free(&groups[ep][groupIden].subGrp[move]);
}

NODE_LIST_STATUS
handleAssociationGetnodeList(
  uint8_t groupId,
  uint8_t ep,
  destination_info_t ** ppList,
  uint8_t* pListLen)
{
  if((NUMBER_OF_ENDPOINTS + 1) < ep )
  {
    return NODE_LIST_STATUS_ERR_ENDPOINT_OUT_OF_RANGE;
  }

  if (NULL == ppList)
  {
    return NODE_LIST_STATUS_ERROR_LIST;
  }

  if (NULL == pListLen)
  {
    return NODE_LIST_STATUS_ERROR_LIST;
  }

  /*Check group number*/
  if (false == isGroupIdValid(groupId, ep))
  {
    return NODE_LIST_STATUS_ERR_GROUP_NBR_NOT_LEGAL; /*not legal number*/
  }

  AssGroupMappingLookUp(&ep, &groupId);

  *ppList = GetNode(ep, groupId, 0); // Get a pointer to the first node
  *pListLen = MAX_ASSOCIATION_IN_GROUP; /*default set to max*/

  for (uint8_t indx = 0; indx < MAX_ASSOCIATION_IN_GROUP; indx++)
  {
    if (IsFree(*ppList + indx))
    {

      *pListLen = indx; /*number of nodes in list*/
      break;  /* break out of loop*/
    }
  }
  if(0 == *pListLen)
  {
    return NODE_LIST_STATUS_ASSOCIATION_LIST_EMPTY;
  }
  return NODE_LIST_STATUS_SUCCESS;
}

/**
 * Returns whether a given group ID is valid or not.
 * @param groupId The group ID to check.
 * @param endpoint The endpoint to which the group belongs.
 * @return true if group ID is valid, false otherwise.
 */
static bool isGroupIdValid(uint8_t groupId, uint8_t endpoint)
{
  if ((0 == groupId) // Group ID zero is invalid
      || (CC_AGI_groupCount_handler(endpoint) <= (groupId - 1))) // Check with AGI.
  {
    return false; /*not legal number*/
  }
  return true;
}

uint8_t
handleGetMaxNodesInGroup(
  uint8_t groupIden,
  uint8_t ep)
{
  if ((1 == groupIden) && (0 != ep))
  {
    return 0;
  }
  return MAX_ASSOCIATION_IN_GROUP;
}

uint8_t
handleGetMaxAssociationGroups(uint8_t endpoint)
{
  return CC_AGI_groupCount_handler(endpoint);
}

/**
 * Returns the result of comparing two nodes.
 *
 * The function compares node ID and endpoint, but has one twist: It considers a node with an
 * endpoint as a lower node than a node without an endpoint.
 *
 * NOTICE that it does matter which node is passed to which argument.
 * @param pNodeOne Must be the existing node.
 * @param pNodeTwo Must be a new node to insert in the list of existing nodes.
 * @return
 */
static int32_t NodeCompare(destination_info_t * pNodeOne, destination_info_t * pNodeNew)
{
  if (pNodeOne->node.nodeId > pNodeNew->node.nodeId && pNodeOne->node.endpoint > 0 && 0 == pNodeNew->node.endpoint) {
    return -1;
  } else if (pNodeOne->node.nodeId < pNodeNew->node.nodeId && pNodeNew->node.endpoint > 0 && 0 == pNodeOne->node.endpoint) {
    return 1;
  }
  return memcmp(&pNodeOne->node, &pNodeNew->node, sizeof(MULTICHAN_DEST_NODE_ID));
}

bool
AssociationAddNode(
    uint8_t groupID,
    uint8_t endpoint,
    MULTICHAN_DEST_NODE_ID* pNodeToAdd,
    bool multiChannelAssociation)
{
  AssGroupMappingLookUp(&endpoint, &groupID);

  uint8_t n = handleGetMaxNodesInGroup(groupID, endpoint);

  destination_info_t * pCurrentNode;

  // Verify that we have at least one free entry.
  pCurrentNode = GetNode(endpoint, groupID, n - 1);
  if (!IsFree(pCurrentNode))
  {
    return false;
  }

  // Set the result to the highest positive value because of the tested expression in the for loop.
  int32_t result = INT32_MAX;

  int32_t indx;
  for (indx = n - 2; (indx >= 0 && result > 0); indx--) {
    pCurrentNode = GetNode(endpoint, groupID, indx);
    if (IsFree(pCurrentNode)) {
      continue;
    }

    destination_info_t node;
    node.node.nodeId = pNodeToAdd->nodeId;
    node.node.endpoint = pNodeToAdd->endpoint;
    node.node.BitAddress = pNodeToAdd->BitAddress;
    result = NodeCompare(pCurrentNode, &node);

    if (0 < result) {
      // New node is smaller than current node AND current entry is taken
      // => Move node in current entry
      destination_info_t * pNewLocation = GetNode(endpoint, groupID, indx + 1);
      *pNewLocation = *pCurrentNode;
    } else if (0 > result) {
      indx++;
    }
  }

  indx++;

  pCurrentNode = GetNode(endpoint, groupID, indx);

  memcpy((uint8_t*)&(pCurrentNode->node), (uint8_t*)pNodeToAdd, sizeof(MULTICHAN_DEST_NODE_ID));
  pCurrentNode->nodeInfo.BitMultiChannelEncap = multiChannelAssociation;
  pCurrentNode->nodeInfo.security = GetHighestSecureLevel(ZAF_GetSecurityKeys());
  return true;
}

e_cmd_handler_return_code_t
AssociationRemove(
  uint8_t groupId,
  uint8_t ep,
  ZW_MULTI_CHANNEL_ASSOCIATION_REMOVE_1BYTE_V2_FRAME* pCmd,
  uint8_t cmdLength)
{
  uint8_t j;
  uint8_t * pCmdByteWise = (uint8_t *)pCmd;
  ASSOCIATION_NODE_LIST list;
  uint8_t maxNumberOfGroups;

  /*Only setup lifeline for rootdevice*/
  if ((NUMBER_OF_ENDPOINTS + 1) < ep || ((1 == groupId) && (0 < ep)))
  {
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }

  maxNumberOfGroups = CC_AGI_groupCount_handler(ep);

  if ((0 < groupId) && (maxNumberOfGroups >= groupId))
  {
    AssGroupMappingLookUp(&ep, &groupId);

    if ((3 == cmdLength) || ((4 == cmdLength) && (0x00 == *(pCmdByteWise + 3))))
    {
      /*
       * The command is either [Class, Command, GroupID] or [Class, Command, GroupID, Marker].
       * In either case, we delete all nodes in the given group.
       */
      for (int8_t i = MAX_ASSOCIATION_IN_GROUP - 1; i >= 0; i--)
      {
        /*
         * The node can only be deleted according to the following truth table.
         * CCA   = Command Class Association Remove Command
         * CCMCA = Command Class Multi Channel Association Remove Command
         * MC    = Multi Channel associated node
         * T     = true
         * F     = false
         *      | CCA | CCMCA |
         * --------------------
         * MC=T |  V  |   V   |
         * --------------------
         * MC=F |  %  |   V   |
         * --------------------
         */
        destination_info_t * pCurrentNode = GetNode(ep, groupId, i);
        if (!((COMMAND_CLASS_ASSOCIATION == *pCmdByteWise) && (true == HasEndpoint(pCurrentNode))))
        {
          Free(pCurrentNode);
          ReorderGroupAfterRemove(groupId-1, ep, i);
        }
      }
    }
    else
    {
      // If this is the case, the command must be [Class, Command, GroupID, i3, i4, i5, ...].
      ExtractCmdClassNodeList(&list,(ZW_MULTI_CHANNEL_ASSOCIATION_SET_1BYTE_V2_FRAME*) pCmd, cmdLength, NULL);
      RemoveAssociationsFromGroup(*pCmdByteWise, ep, groupId, &list);
    }
    AssociationStoreAll();
  }
  else if (0 == groupId)
  {
    /*
     * When the group ID equals zero, it is desired to remove all nodes from all groups or given
     * nodes from all groups.
     * If the length is larger than 3, it means that only given nodes must be removed from all
     * groups.
     */
    ExtractCmdClassNodeList(&list,(ZW_MULTI_CHANNEL_ASSOCIATION_SET_1BYTE_V2_FRAME*) pCmd, cmdLength, NULL);

    for (ep = 0; ep < NUMBER_OF_ENDPOINTS + 1; ep++)
    {
      maxNumberOfGroups = CC_AGI_groupCount_handler(ep);
      for (j = 1; j <= maxNumberOfGroups; j++)
      {
        RemoveAssociationsFromGroup(*pCmdByteWise, ep, j, &list);
      }
    }
    AssociationStoreAll();
  }
  else
  {
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }
  return E_CMD_HANDLER_RETURN_CODE_HANDLED;
}

/**
 * @brief Removes given associations or all associations from a given group.
 * @param groupId ID of the group from where the associations must be removed.
 * @param listOfNodes List of nodes/associations that must be removed.
 */
static void
RemoveAssociationsFromGroup(
    uint8_t cmdClass,
    uint8_t ep,
    uint8_t groupId,
    ASSOCIATION_NODE_LIST * pListOfNodes)
{
  uint8_t numberOfNodes;

  numberOfNodes = ((0 == pListOfNodes->noOfNodes) ? 1 : pListOfNodes->noOfNodes);

  // Remove all Node ID Associations in the given list.
  for (uint8_t i = 0; i < numberOfNodes; i++)
  {
    for (int8_t indx = MAX_ASSOCIATION_IN_GROUP - 1; indx >= 0; indx--)
    {
      destination_info_t * pCurrentNode = GetNode(ep, groupId, indx);
      if (!IsFree(pCurrentNode) &&
          (false == HasEndpoint(pCurrentNode)) &&
          ((0 == pListOfNodes->noOfNodes && 0 == pListOfNodes->noOfMulchanNodes) || pCurrentNode->node.nodeId == pListOfNodes->pNodeId[i]))
      {
        Free(pCurrentNode);
        /*
         * Do reorder after freeing each entry because the command might originate from
         * CC Association and then endpoint destinations will not be deleted. Hence, they must be
         * ordered.
         */
        ReorderGroupAfterRemove(groupId-1, ep, indx);
      }
    }
  }

  if (COMMAND_CLASS_ASSOCIATION == cmdClass)
  {
    return;
  }

  numberOfNodes = ((0 == pListOfNodes->noOfMulchanNodes) ? 1 : pListOfNodes->noOfMulchanNodes);

  // Remove all Endpoint Node ID Associations in the given list.
  for (uint8_t i = 0; i < numberOfNodes; i++)
  {
    for (int8_t indx = MAX_ASSOCIATION_IN_GROUP - 1; indx >= 0; indx--)
    {
      destination_info_t * pCurrentNode = GetNode(ep, groupId, indx);
      if (!IsFree(pCurrentNode) &&
          (true == HasEndpoint(pCurrentNode)) &&
          ((0 == pListOfNodes->noOfNodes && 0 == pListOfNodes->noOfMulchanNodes) ||
              ((pCurrentNode->node.nodeId == pListOfNodes->pMulChanNodeId[i].nodeId) &&
                  (pCurrentNode->node.endpoint == pListOfNodes->pMulChanNodeId[i].endpoint) &&
                  (pCurrentNode->node.BitAddress == pListOfNodes->pMulChanNodeId[i].BitAddress))))
      {
        Free(pCurrentNode);

        /*
         * In case specific multi channel destinations are specified, we need to reorder those that
         * will not be removed.
         * If no multi channel destinations are specified, it means that all nodes in the group
         * will be removed. Hence, no need to reorder.
         */
        if (0 < pListOfNodes->noOfMulchanNodes)
        {
          ReorderGroupAfterRemove(groupId-1, ep, indx);
          break;
        }
      }
    }
  }
}

/**
 * @brief Reads/Writes association data to the NVM.
 * @param action The action to take.
 */
static void
NVM_Action(NVM_ACTION action)
{
  uint8_t i,j,k;
  Ecode_t errCode;

  ASSERT(pFileSystem != 0);

  if (READ_DATA == action)
  {
    SAssociationInfo  tSource;
    errCode = nvm3_readData(pFileSystem, ZAF_FILE_ID_ASSOCIATIONINFO, &tSource, sizeof(SAssociationInfo));
    ASSERT(ECODE_NVM3_OK == errCode);

    SAssociationInfo* pSource = &tSource;
    for(i = 0; i < MAX_ASSOCIATION_GROUPS; i++)
    {
      for(j = 0; j < NUMBER_OF_ENDPOINTS + 1; j++)
      {
        for(k = 0; k < MAX_ASSOCIATION_IN_GROUP; k++)
        {
          groups[j][i].subGrp[k].node.nodeId     = pSource->Groups[j][i].subGrp[k].node.nodeId;     //1Byte
          groups[j][i].subGrp[k].node.endpoint   = pSource->Groups[j][i].subGrp[k].node.endpoint;   //7bits
          groups[j][i].subGrp[k].node.BitAddress = pSource->Groups[j][i].subGrp[k].node.BitAddress; //1bit

          groups[j][i].subGrp[k].nodeInfo.BitMultiChannelEncap = pSource->Groups[j][i].subGrp[k].nodeInfoPacked.BitMultiChannelEncap; //1bit to uint8_t
          groups[j][i].subGrp[k].nodeInfo.security             = (security_key_t)pSource->Groups[j][i].subGrp[k].nodeInfoPacked.security;  //4bits to enum
        }
      }
    }
    //memcpy(groups, &pSource->Groups, sizeof_structmember(SAssociationInfo, Groups));
  }
  else
  {
    SAssociationInfo associationInfo;

    for(i = 0; i < MAX_ASSOCIATION_GROUPS; i++)
    {
      for(j = 0; j < NUMBER_OF_ENDPOINTS + 1; j++)
      {
        for(k = 0; k < MAX_ASSOCIATION_IN_GROUP; k++)
        {
          associationInfo.Groups[j][i].subGrp[k].node.nodeId     = groups[j][i].subGrp[k].node.nodeId;     //1Byte
          associationInfo.Groups[j][i].subGrp[k].node.endpoint   = groups[j][i].subGrp[k].node.endpoint;   //7bits
          associationInfo.Groups[j][i].subGrp[k].node.BitAddress = groups[j][i].subGrp[k].node.BitAddress; //1bit

          associationInfo.Groups[j][i].subGrp[k].nodeInfoPacked.BitMultiChannelEncap = groups[j][i].subGrp[k].nodeInfo.BitMultiChannelEncap; //uint8_t to 1 bit
          associationInfo.Groups[j][i].subGrp[k].nodeInfoPacked.security             = (uint8_t)groups[j][i].subGrp[k].nodeInfo.security;  //enum to 4bits

          DPRINTF("associationInfo.Groups[%d][%d].subGrp[%d].node.nodeId: %d\r\n", j,i,k, associationInfo.Groups[j][i].subGrp[k].node.nodeId);
        }
      }
    }

    errCode = nvm3_writeData(pFileSystem, ZAF_FILE_ID_ASSOCIATIONINFO, &associationInfo, sizeof(SAssociationInfo));
    ASSERT(ECODE_NVM3_OK == errCode);
  }
}

/**
 * @brief Stores all associations in the NVM.
 */
static void
AssociationStoreAll(void)
{
  NVM_Action(WRITE_DATA);
}

void
AssociationInit(bool forceClearMem, void * pFS)
{
  UNUSED(pFS);
  pFileSystem = ZAF_GetFileSystemHandle();
  ASSERT(pFileSystem != 0);

  Ecode_t errCode = 0;
  uint32_t objectType;
  size_t   dataLen = 0;
  if(!forceClearMem)
  {
    errCode = nvm3_getObjectInfo(pFileSystem, ZAF_FILE_ID_ASSOCIATIONINFO, &objectType, &dataLen);
  }

  if ((ECODE_NVM3_OK != errCode) || (ZAF_FILE_SIZE_ASSOCIATIONINFO != dataLen) || (true == forceClearMem))
  {
    //Write default Association Info file
    SAssociationInfo tAssociationInfo;
    memset(&tAssociationInfo, 0 , sizeof(SAssociationInfo));

    /*
     * Notice the +1 for the endpoint expression. This ensures that this loop will run for the
     * root device.
     */
    for (uint8_t endpoint = 0; endpoint < (NUMBER_OF_ENDPOINTS + 1); endpoint++)
    {
      for (uint8_t group = 0; group < MAX_ASSOCIATION_GROUPS; group++)
      {
        for (uint8_t node = 0; node < MAX_ASSOCIATION_IN_GROUP; node++)
        {
          tAssociationInfo.Groups[endpoint][group].subGrp[node].node.nodeId = 0xFF; // Free
        }
      }
    }

    errCode = nvm3_writeData(pFileSystem, ZAF_FILE_ID_ASSOCIATIONINFO, &tAssociationInfo, sizeof(SAssociationInfo));
    ASSERT(ECODE_NVM3_OK == errCode);
  }
  NVM_Action(READ_DATA);
}

e_cmd_handler_return_code_t handleAssociationSet(
    uint8_t ep,
    ZW_MULTI_CHANNEL_ASSOCIATION_SET_1BYTE_V2_FRAME* pCmd,
    uint8_t cmdLength)
{
  uint8_t i = 0;
  ASSOCIATION_NODE_LIST list;
  uint8_t multiChannelAssociation = false;

  // Set up lifeline for root device only.
  if (((NUMBER_OF_ENDPOINTS + 1) < ep || ((1 == pCmd->groupingIdentifier) && (0 < ep) )) ||
     (0 == pCmd->groupingIdentifier) || (CC_AGI_groupCount_handler(ep) < pCmd->groupingIdentifier))
  {
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }

  ExtractCmdClassNodeList(&list, pCmd, cmdLength, &multiChannelAssociation);

  if ((list.noOfNodes + list.noOfMulchanNodes) > handleGetMaxNodesInGroup(pCmd->groupingIdentifier, ep))
  {
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }

  for(i = 0; i < list.noOfNodes; i++)
  {
    MULTICHAN_DEST_NODE_ID node;
    node.nodeId = *(list.pNodeId + i);
    node.endpoint = 0;
    node.BitAddress = 0;
    if(false == AssociationAddNode( pCmd->groupingIdentifier, ep, &node, false ))
    {
      return E_CMD_HANDLER_RETURN_CODE_FAIL;
    }
  }

  for(i = 0; i < list.noOfMulchanNodes; i++)
  {
    if(false == AssociationAddNode( pCmd->groupingIdentifier, ep, &list.pMulChanNodeId[i], multiChannelAssociation ))
    {
      return E_CMD_HANDLER_RETURN_CODE_FAIL;
    }
  }
  AssociationStoreAll();
  return E_CMD_HANDLER_RETURN_CODE_HANDLED;
}

/**
 * @brief Extracts nodes and endpoints from a given (Multi Channel) Association frame.
 * @param[out] plist
 * @param[in] pCmd
 * @param[in] cmdLength
 * @param[out] pMultiChannelAssociation
 */
static void
ExtractCmdClassNodeList(
    ASSOCIATION_NODE_LIST* plist,
    ZW_MULTI_CHANNEL_ASSOCIATION_SET_1BYTE_V2_FRAME* pCmd,
    uint8_t cmdLength,
    uint8_t* pMultiChannelAssociation)
{
  uint8_t i = 0;
  uint8_t * pNodeId = NULL;

  plist->pNodeId = &pCmd->nodeId1;
  plist->noOfNodes  = 0;
  plist->pMulChanNodeId = NULL;
  plist->noOfMulchanNodes = 0;

  if (3 >= cmdLength)
  {
    /*
     * If the length is less than or equal to three, it means that it's a get or a remove. In the
     * first case we shouldn't end up here. In the second case, we must return, since the remove
     * command should remove all nodes.
     */
    plist->pNodeId = NULL;
    return;
  }

  cmdLength -= OFFSET_PARAM_2; /*calc length on node-Id's*/

  for (i = 0; i < cmdLength; i++)
  {
    pNodeId = plist->pNodeId + i;
    if (MULTI_CHANNEL_ASSOCIATION_SET_MARKER_V2 == *pNodeId)
    {
      if (MULTI_CHANNEL_ASSOCIATION_SET_MARKER_V2 == *(pNodeId + 1))
      {
        /* 2 MARKERS!! error en incomming frame!*/
        return;
      }
      plist->noOfMulchanNodes = (cmdLength - (i + 1)) / 2;
      if (0 != plist->noOfMulchanNodes)
      {
        if (NON_NULL(pMultiChannelAssociation))
        {
          *pMultiChannelAssociation = true;
        }
        plist->pMulChanNodeId = (MULTICHAN_DEST_NODE_ID*)(pNodeId + 1); /*Point after the marker*/
        i = cmdLength;
      }
    }
    else
    {
      plist->noOfNodes = i + 1;
    }
  }
}

void
AssociationInitEndpointSupport(
  bool forceClearMem,
  ASSOCIATION_ROOT_GROUP_MAPPING* pMapping,
  uint8_t nbrGrp,
  void * pFS)
{
  UNUSED(pFS);
  pGroupMappingTable = pMapping;
  numberOfGroupMappingEntries = nbrGrp;
  AssociationInit(forceClearMem, NULL);
}

/**
 * @brief Searches through the group mapping table and updates the variables given by the pointers.
 * @param[in/out] pEndpoint Pointer to an endpoint.
 * @param[in/out] pGroupID Pointer to a group ID.
 * @return true if the mapping was found, false otherwise.
 */
static bool
AssGroupMappingLookUp(
    uint8_t* pEndpoint,
    uint8_t* pGroupID)
{
  if ((0 == *pEndpoint) && (1 != *pGroupID) && NON_NULL(pGroupMappingTable))
  {
    /*Use Group mapping to get endpoint proupIden*/
    uint8_t i;
    for (i = 0; i < numberOfGroupMappingEntries; i++)
    {
      if (*pGroupID == pGroupMappingTable[i].rootGrpId)
      {
        *pEndpoint = pGroupMappingTable[i].endpoint;
        *pGroupID = pGroupMappingTable[i].endpointGrpId;
        return true;
      }
    }
  }
  return false;
}

void
AssociationGet(
    uint8_t endpoint,
    uint8_t * incomingFrame,
    uint8_t * outgoingFrame,
    uint8_t * outgoingFrameLength)
{
  uint8_t nodeCount;
  uint8_t nodeCountMax;
  uint8_t nodeCountNoEndpoint;
  uint8_t nodeFieldCount;
  uint8_t mappedEndpoint;
  uint8_t mappedGroupID;
  MULTICHAN_NODE_ID * pCurrentNode;

  if ((*(incomingFrame + 2) > CC_AGI_groupCount_handler(endpoint)) || (0 == *(incomingFrame + 2)))
  {
    // If the group is invalid, we return group 1
    *(incomingFrame + 2) = 1;
  }

  mappedEndpoint = endpoint;
  mappedGroupID = *(incomingFrame + 2);
  if (0 == endpoint)
  {
    AssGroupMappingLookUp(&mappedEndpoint, &mappedGroupID);
  }

  nodeCountMax = handleGetMaxNodesInGroup(*(incomingFrame + 2), endpoint);

  *outgoingFrame = *incomingFrame; // Set the command class.

  *(outgoingFrame + 2) = *(incomingFrame + 2); // The group
  *(outgoingFrame + 3) = nodeCountMax;
  *(outgoingFrame + 4) = 0; // Number of reports to follow.

  // Add node IDs without endpoints if any.
  nodeCountNoEndpoint = 0;
  for (nodeCount = 0; nodeCount < nodeCountMax; nodeCount++)
  {
    pCurrentNode = GetNode(mappedEndpoint, mappedGroupID, nodeCount);

    if (IsFree(pCurrentNode)) break;

    if (false == HasEndpoint(pCurrentNode))
    {
      // No endpoints in the association
      *(outgoingFrame + 5 + nodeCountNoEndpoint) = pCurrentNode->node.nodeId;
      nodeCountNoEndpoint++;
    }
  }

  switch (*(incomingFrame)) // Check command class.
  {
  case COMMAND_CLASS_ASSOCIATION:
    *(outgoingFrame + 1) = ASSOCIATION_REPORT_V2; // The response command.
    break;
  case COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V3:
    *(outgoingFrame + 1) = MULTI_CHANNEL_ASSOCIATION_REPORT_V3; // The response command.

    // Add endpoint nodes if any.
    nodeFieldCount = 0;
    for (nodeCount = 0; nodeCount < nodeCountMax; nodeCount++)
    {
      pCurrentNode = GetNode(mappedEndpoint, mappedGroupID, nodeCount);

      // Since the group is ordered, we can break on the first free entry.
      if (IsFree(pCurrentNode)) break;

      if (true == HasEndpoint(pCurrentNode))
      {
        // The association contains endpoints.
        *(outgoingFrame + 6 + nodeCountNoEndpoint + nodeFieldCount++) = *((uint8_t *)pCurrentNode);
        *(outgoingFrame + 6 + nodeCountNoEndpoint + nodeFieldCount++) = *(((uint8_t *)pCurrentNode) + 1);
      }
    }

    if (nodeFieldCount)
    {
      *(outgoingFrame + 5 + nodeCountNoEndpoint) = MULTI_CHANNEL_ASSOCIATION_REPORT_MARKER_V3;
      *outgoingFrameLength = sizeof(ZW_MULTI_CHANNEL_ASSOCIATION_REPORT_1BYTE_V3_FRAME) - 3 + nodeCountNoEndpoint + nodeFieldCount;

      // We return if there are endpoint associations.
      return;
    }
    break;
  default:
    // We should never get here, but if we do it means that we got an invalid command class.
    // Set the length to zero.
    *outgoingFrameLength = 0;
    break;
  }

  // If there are no endpoint associations we end up here.
  *outgoingFrameLength = sizeof(ZW_ASSOCIATION_REPORT_1BYTE_FRAME) - 1 + nodeCountNoEndpoint;
}

static destination_info_t * singlecastDestinations[MAX_ASSOCIATION_IN_GROUP];
static uint32_t singlecastCount;
static uint32_t j;

void AssociationGetDestinationInit(destination_info_t * pFirstDestination)
{
  j = 0;
  for (uint32_t i = 0; i < MAX_ASSOCIATION_IN_GROUP; i++)
  {
    if (IsFree(pFirstDestination + i)) {
      singlecastCount = i;
      break;
    }
    singlecastDestinations[i] = pFirstDestination + i;
  }
}

destination_info_t * AssociationGetSinglecastDestination(void)
{
  static uint32_t i = 0;
  destination_info_t * pNode;
  do {
    pNode = singlecastDestinations[i % singlecastCount];
    i++;
    if (NULL != pNode) {
      break;
    }
  } while (true);
  return pNode;
}

uint32_t AssociationGetSinglecastNodeCount(void)
{
  uint32_t count = singlecastCount;
  for (uint32_t i = 0; i < singlecastCount; i++)
  {
    if (NULL == singlecastDestinations[i]) {
      count--;
    }
  }
  return count;
}

bool AssociationGetBitAdressingDestination(destination_info_t ** ppNodeList,
                                           uint8_t * pListLength,
                                           destination_info_t * pNode)
{
  destination_info_t * pEntry = *ppNodeList;
  uint8_t previousNodeId = 0;

  uint8_t i;
  for (i = 0; i < *pListLength; i++, j++) {
    uint8_t endpoint = (pEntry + i)->node.endpoint;
    uint8_t nodeId = (pEntry + i)->node.nodeId;

    if (0 == i) {
      // New node ID
      if (endpoint > 0) {
        // In this case, the first node in the list has and endpoint.
        // Hence, it must be the first association of MAYBE several with endpoints.
        memset((uint8_t *)pNode, 0, sizeof(destination_info_t));
        pNode->node.nodeId = nodeId;
        pNode->node.endpoint = (1 << (endpoint -1));
      } else {
        // Current node has no endpoints. Since we know that our associations are sorted with
        // endpoint destinations first, we know that there are no more endpoint destinations.
        // Do not change list pointer or list length because we did not use anything from the list.
        pNode->node.BitAddress = 0;
        return false;
      }
    } else if (previousNodeId != nodeId) {
      // The node ID has changed.

      // No matter what, we must point pList to this node and update list length.
      *ppNodeList = pEntry + i;
      *pListLength -= i;

      if (endpoint > 0) {
        // Next node also has endpoints. Hence, return true.
        return true;
      } else {
        return false;
      }
    } else {
      // If the node ID did not change, this association MUST be at least the SECOND one with an
      // endpoint.
      pNode->node.endpoint |= (1 << (endpoint -1 ));
      pNode->node.BitAddress = 1;
      // Mark this one and the previous one as used for bit addressing
      singlecastDestinations[j - 1] = NULL;
      singlecastDestinations[j] = NULL;
    }
    previousNodeId = nodeId;
  }

  // If we fall through the loop without a return it means that there a no more nodes in the list
  // and that the last node was with an endpoint.

  // Update list and list length
  *ppNodeList = NULL;
  *pListLength = 0;

  return false;
}
