/**
* @file
*
*
* @startuml
* title Application <-> ZwStack interface
* box "Application task" #LightBlue
* participant Application
* end box
* participant TxQueue
* participant CommandQueue
* participant StatusQueue
* participant RxQueue
* box "ZwStack task" #Pink
* participant ZwStack
* end box
* participant Radio
* group Receiver task context
* == Tx ==
*   [->Application: Reason to send frame
*   activate Application
*   Application->TxQueue: Put frame on queue
*   activate ZwStack
*   TxQueue<->ZwStack: Take frame off queue
*   note left: OS context switches to ZwStack
*   ZwStack->Radio: Tx Frame
*   deactivate ZwStack
*   activate Radio
*   TxQueue->Application: Application continues\nfrom QueueSendToback()
*   note left: OS context swithces to Application
*   [<-Application: Finishes
*   deactivate Application
*   ...All tasks are sleeping...
*   Radio->ZwStack: Tx Complete
*   deactivate Radio
*   activate ZwStack
*   ZwStack->Radio: Listen for ack
*   deactivate ZwStack
*   activate Radio
*   ...All tasks are sleeping...
*   Radio->ZwStack: Ack received
*   deactivate Radio
*   activate ZwStack
*   ZwStack->StatusQueue: Put 'Frame successfully sent' on queue
*   deactivate ZwStack
*   activate Application
*   StatusQueue<->Application: Take status off queue
*   note left: Application awakened
*   [<-Application: Finished processing status
*   deactivate Application
* == Command ==
*   [->Application: Reason to send command
*   activate Application
*   Application->CommandQueue: Put command on queue
*   activate ZwStack
*   CommandQueue<->ZwStack: Take command off queue
*   note left: OS context switches to ZwStack
*   ...ZwStack processes command...
*   ZwStack->StatusQueue: Put 'Command status' on queue
*   deactivate ZwStack
*   StatusQueue->Application: Take status off queue
*   note left: OS context switches to Application
*   [<-Application: Finished processing status
*   deactivate Application
* == Rx ==
*   Radio->ZwStack: Frame received
*   note right: ZwStack awakened
*   activate ZwStack
*   ZwStack->RxQueue: Put frame on queue
*   deactivate ZwStack
*   activate Application
*   RxQueue<->Application: Take frame off queue
*   note left: Application awakened
*   ...Application thread processes frame...
* @enduml
*
*
* @copyright 2019 Silicon Laboratories Inc.
*/

#ifndef _ZW_APPLICATION_TRANSPORT_INTERFACE_H_
#define _ZW_APPLICATION_TRANSPORT_INTERFACE_H_

#include <stddef.h>
#include <ZW_typedefs.h>
#include "ZW_classcmd.h"
#include "QueueNotifying.h"
#include "ZW_transport_api.h"
#include "NodeMask.h"
#include "ZW_basis_api.h"
#include "ZW_radio_api.h"  // For SNetworkStatistics
#include "ZW_PowerManager_api.h"
#include "Assert.h"


typedef struct SCommandClassVersions
{
  uint8_t SecurityVersion;
  uint8_t Security2Version;
  uint8_t TransportServiceVersion;
} SCommandClassVersions;

typedef struct SProtocolVersion
{
  uint8_t Major;
  uint8_t Minor;
  uint8_t Revision;
} SProtocolVersion;

typedef enum EProtocolType
{
  EPROTOCOLTYPE_ZWAVE = 0,
  EPROTOCOLTYPE_ZWAVE_AV,
  EPROTOCOLTYPE_ZWAVE_FOR_IP
} EProtocolType;

typedef enum ELibraryType
{
  ELIBRARYTYPE_CONTROLLER_STATIC     = 1, // DEPRECATED
  ELIBRARYTYPE_CONTROLLER_PORTABLE   = 2, // DEPRECATED
  ELIBRARYTYPE_SLAVE                 = 3, // Previously Slave Enhanced
  ELIBRARYTYPE_SLAVE_BEFORE_ENHANCED = 4, // DEPRECATED
  ELIBRARYTYPE_INSTALLER             = 5, // DEPRECATED
  ELIBRARYTYPE_SLAVE_ROUTING         = 6, // DEPRECATED
  ELIBRARYTYPE_CONTROLLER            = 7, // Previously Controller Bridge
  ELIBRARYTYPE_DUT                   = 8, // DEPRECATED
  ELIBRARYTYPE_AVREMOTE              = 10, // DEPRECATED
  ELIBRARYTYPE_AVDEVICE              = 11  // DEPRECATED
} ELibraryType;

typedef struct SProtocolInfo
{
  SCommandClassVersions CommandClassVersions; /**< Versions of Command Classes supplied  by protocol */
  SProtocolVersion      ProtocolVersion;      /**< Protocol version */
  EProtocolType         eProtocolType;
  ELibraryType          eLibraryType;         /*< Protocol library type */
} SProtocolInfo;

typedef enum EInclusionState_t
{
  EINCLUSIONSTATE_EXCLUDED = 0,
  EINCLUSIONSTATE_UNSECURE_INCLUDED,
  EINCLUSIONSTATE_SECURE_INCLUDED
} EInclusionState_t;

typedef struct SNetworkInfo
{
  EInclusionState_t   eInclusionState;
  uint8_t             SucNodeId;
  uint8_t             SecurityKeys;     // Which security keys the node has
  uint8_t             NodeId;
  uint32_t            HomeId;
  uint16_t            MaxPayloadSize;
} SNetworkInfo;

typedef struct SRadioStatus
{
  int8_t iRadioPowerLevel;  // Radio power in db. 0 -> Max power.
} SRadioStatus;


typedef struct SApplicationHandles
{
  SQueueNotifying*          pZwTxQueue;    /**< Notifying Queue object (contains FreeRTOS queue)
                                                - Queue for ZW frames from application to Protocol
                                                for transmission */

  QueueHandle_t             ZwRxQueue;     /**< FreeRTOS Queue handle - Queue for ZW frames
                                                forwarded from protocol to application */

  SQueueNotifying*          pZwCommandQueue; /**< Notifying Queue object (contains FreeRTOS queue)
                                                  - Queue for commands from Application to
                                                  protocol */

  QueueHandle_t             ZwCommandStatusQueue;   /**< FreeRTOS Queue handle - Queue for status
                                                         replies from protocol to application
                                                         (status on commands from application to
                                                         protocol and Tx request from App to
                                                         protocol) */

  const SNetworkStatistics* pNetworkStatistics;  /**< Network statistics supplied by protocol */

  const SProtocolInfo*      pProtocolInfo;

  const SNetworkInfo*       pNetworkInfo;

  const SRadioStatus*       pRadioStatus;

  uint8_t m_randomSeed;
} SApplicationHandles;


////////////////

typedef struct SCommandClassList_t
{
  uint8_t         iListLength;
  const uint8_t * pCommandClasses;
} SCommandClassList_t;

typedef struct SCommandClassSet_t
{
  SCommandClassList_t UnSecureIncludedCC;       /**< List of UNsecure supported command classes. Available when node is NOT included or UNsecure included */
  SCommandClassList_t SecureIncludedUnSecureCC; /**< List of UNsecure supported command classes in secure network. Available when node is secure included */
  SCommandClassList_t SecureIncludedSecureCC;   /**< List of Secure supported command classes. Available when node is secure included */
} SCommandClassSet_t;


typedef enum EListenBeforeTalkThreshold_t
{
  ELISTENBEFORETALKTRESHOLD_DEFAULT = 127
} EListenBeforeTalkThreshold_t;

typedef enum EtxPowerLevel_t
{
  ETXPOWERLEVEL_DEFAULT = 127
} EtxPowerLevel_t;


typedef struct SRadioConfig_t
{
  int8_t iListenBeforeTalkThreshold;  /**< Db (negative) or EListenBeforeTalkThreshold_t */
  int8_t iTxPowerLevelMax;            /**< Db (negative) or EtxPowerLevel_t */
  int8_t iTxPowerLevelAdjust;        /**< Db (negative) or EtxPowerLevel_t */
  ZW_Region_t eRegion;				        /**< RF Region setting */
} SRadioConfig_t;

typedef struct SAppNodeInfo_t
{
  uint8_t             DeviceOptionsMask;
  APPL_NODE_TYPE      NodeType;
  SCommandClassSet_t  CommandClasses;
} SAppNodeInfo_t;

typedef struct SVirtualSlaveNodeInfo_t
{
  uint8_t             NodeId;
  bool                bListening; // True if this node is always on air
  APPL_NODE_TYPE      NodeType;
  SCommandClassList_t CommandClasses;
} SVirtualSlaveNodeInfo_t;

typedef struct SVirtualSlaveNodeInfoTable_t
{
  uint8_t                         iTableLength;
  const SVirtualSlaveNodeInfo_t ** ppNodeInfo;  // Array of pointers to node info. Pointers may be NULL
} SVirtualSlaveNodeInfoTable_t;                 // This allows "nulling" a pointer while modifying a virtual slave node info
                                                // Or changing pointer to point to a different one.

// This struct content must be set up by application before enabling protocol (enabling radio)
// Direct content (the pointers) may not be changed runtime, but the data they point to can be edited by application run time
typedef struct SProtocolConfig_t
{
  const SVirtualSlaveNodeInfoTable_t *        pVirtualSlaveNodeInfoTable; // NULL is acceptable if no virtual slave nodes
  const uint8_t *                             pSecureKeysRequested;   // values are ref SECURITY_KEY_S2_ACCESS_BIT, ref SECURITY_KEY_S2_AUTHENTICATED_BIT, ref SECURITY_KEY_S2_UNAUTHENTICATED_BIT,  - consider making a bit field struct for it
  const SAppNodeInfo_t *                      pNodeInfo;
  const SRadioConfig_t *                      pRadioConfig;
} SProtocolConfig_t;

///////////

#define APPLICATION_INTERFACE_TRANSMIT_ENUM_OFFSET (0x00)
#define APPLICATION_INTERFACE_COMMAND_ENUM_OFFSET (0x40)
#define APPLICATION_INTERFACE_RECEIVE_ENUM_OFFSET (0x80)
#define APPLICATION_INTERFACE_STATUS_ENUM_OFFSET (0xC0)

/**
 * Max theoretical Z-Wave frame payload size in a Z-Wave protocol using 3CH network
 * The real Z-Wave frame payload type depends on various parameters (routed, multicast, explore, security and/or number of RF channels)
 * Customer must not use this value in their application. They must use the value MaxPayloadSize from the SNetworkInfo structure.
*/
#define ZW_MAX_PAYLOAD_SIZE        160

typedef enum EZwaveTransmitType
{
  EZWAVETRANSMITTYPE_STD = APPLICATION_INTERFACE_TRANSMIT_ENUM_OFFSET,
  EZWAVETRANSMITTYPE_EX,
  EZWAVETRANSMITTYPE_BRIDGE,
 // Multi types requires SZwaveTransmitPackage.NodeMask to be setup
  EZWAVETRANSMITTYPE_MULTI,
  EZWAVETRANSMITTYPE_MULTI_EX,
  EZWAVETRANSMITTYPE_MULTI_BRIDGE,
  EZWAVETRANSMITTYPE_EXPLOREINCLUSIONREQUEST,
  EZWAVETRANSMITTYPE_EXPLOREEXCLUSIONREQUEST,
  EZWAVETRANSMITTYPE_NETWORKUPDATEREQUEST,
  EZWAVETRANSMITTYPE_NODEINFORMATION,
  EZWAVETRANSMITTYPE_NODEINFORMATIONREQUEST,
  EZWAVETRANSMITTYPE_TESTFRAME,
  EZWAVETRANSMITTYPE_SETSUCNODEID,
  EZWAVETRANSMITTYPE_SENDSUCNODEID,
  EZWAVETRANSMITTYPE_ASSIGNRETURNROUTE,
  EZWAVETRANSMITTYPE_DELETERETURNROUTE,
  EZWAVETRANSMITTYPE_SENDREPLICATION,
  EZWAVETRANSMITTYPE_SENDREPLICATIONRECEIVECOMPLETE,
  EZWAVETRANSMITTYPE_REQUESTNEWROUTEDESTINATIONS,
  EZWAVETRANSMITTYPE_SEND_SLAVE_NODE_INFORMATION,
  EZWAVETRANSMITTYPE_SEND_SLAVE_DATA,
  EZWAVETRANSMITTYPE_INCLUDEDNODEINFORMATION,
  NUM_EZWAVETRANSMITTYPE
} EZwaveTransmitType;


typedef struct STransmitFrameConfig
{
  void* Handle;                     // Will be returned with transmit status
                                    // Allows application to recognize frames
  uint8_t TransmitOptions;
  uint8_t iFrameLength;
  uint8_t aFrame[170];
} STransmitFrameConfig;

// Basis API
typedef struct SExploreInclusionRequest
{
  uint8_t Reserved;               // Not required set
} SExploreInclusionRequest;

typedef struct SExploreExclusionRequest
{
  uint8_t Reserved;               // Not required set
} SExploreExclusionRequest;

typedef struct SNetworkUpdateRequest
{
  void* Handle;                     // Will be returned with transmit status
                                    // Allows application to recognize frames
} SNetworkUpdateRequest;

typedef struct SNodeInfoRequest
{
  void* Handle;                     // Will be returned with transmit status
                                    // Allows application to recognize frames
  uint8_t DestNodeId;
} SNodeInfoRequest;

typedef struct SNodeInfo
{
  void* Handle;                     // Will be returned with transmit status
                                    // Allows application to recognize frames
  uint8_t DestNodeId;
  uint8_t TransmitOptions;
} SNodeInfo;

/**
 * Contains info related to sending an INIF.
 */
typedef struct SIncludedNodeInfo
{
  void * Handle; /**< Callback handle that Will be invoked with transmit status. */
}
SIncludedNodeInfo;

typedef struct STest
{
  void* Handle;                     // Will be returned with transmit status
                                    // Allows application to recognize frames
  uint8_t DestNodeId;
  uint8_t PowerLevel;
} STest;

// Transport API
typedef struct SSendData
{
  STransmitFrameConfig FrameConfig;
  uint8_t DestNodeId;
} SSendData;

typedef struct SSendDataEx
{
  STransmitFrameConfig FrameConfig;
  uint8_t DestNodeId;
  uint8_t SourceNodeId;
  uint8_t TransmitSecurityOptions;
  uint8_t TransmitOptions2;
  enum SECURITY_KEY eKeyType;
} SSendDataEx;

typedef struct SSendDataBridge
{
  STransmitFrameConfig FrameConfig;
  uint8_t DestNodeId;
  uint8_t SourceNodeId;
} SSendDataBridge;

typedef struct SSendDataMulti
{
  STransmitFrameConfig FrameConfig;
  NODE_MASK_TYPE NodeMask;
} SSendDataMulti;

typedef struct SSendDataMultiEx
{
  STransmitFrameConfig FrameConfig;
  uint8_t SourceNodeId;
  uint8_t GroupId;
  enum SECURITY_KEY eKeyType;
} SSendDataMultiEx;

typedef struct SSendDataMultiBridge
{
  STransmitFrameConfig FrameConfig;
  NODE_MASK_TYPE NodeMask;
  uint8_t SourceNodeId;
} SSendDataMultiBridge;

// Controller  API
typedef struct SSetSucNodeId
{
  void* Handle;                 // Will be returned with transmit status
                                // Allows application to recognize frames
  uint8_t SucNodeId;
  bool    bSucEnable;
  bool    bTxLowPower;
  uint8_t Capabilities;         /* The capabilities of the new SUC */
} SSetSucNodeId;

typedef struct SSendSucNodeId
{
  void* Handle;                 // Will be returned with transmit status
                                // Allows application to recognize frames
  uint8_t DestNodeId;
  uint8_t TransmitOptions;
} SSendSucNodeId;

typedef struct SAssignReturnRoute
{
  void* Handle;                 // Will be returned with transmit status
                                // Allows application to recognize frames
  uint8_t ReturnRouteReceiverNodeId;  // Routing slave to recieve route
  uint8_t RouteDestinationNodeId;     // Destination of route (if 0 destination will be self). Destination can be a SUC.
  uint8_t aPriorityRouteRepeaters[4]; // Route to be assigned as priority route - set to zeroes to NOT supply a priority route (recommended)
  uint8_t PriorityRouteSpeed;
} SAssignReturnRoute;

typedef struct SDeleteReturnRoute
{
  void* Handle;                 // Will be returned with transmit status
                                // Allows application to recognize frames
  uint8_t DestNodeId;           // Node to have its return routes deleted..
  bool bDeleteSuc;              // Delete SUC return routes only, or Delete standard return routes only */
} SDeleteReturnRoute;

typedef struct SSendReplication
{
  STransmitFrameConfig FrameConfig;
  uint8_t DestNodeId;           // Destination NodeId - Single cast only.
} SSendReplication;

typedef struct SSendReplicationReceiveComplete
{
  uint8_t Reserved;             // Not required set
} SSendReplicationReceiveComplete;

// Slave API
typedef struct SRequestNewRouteDestinations
{
  void* Handle;                 // Will be returned with transmit status
                                // Allows application to recognize frames
  uint8_t iDestinationCount;    // Number of new destinations
                                // Array containing new destinations
  uint8_t aNewDestinations[ZW_MAX_CACHED_RETURN_ROUTE_DESTINATIONS];                 // Will be returned with transmit status
} SRequestNewRouteDestinations;

typedef struct SSendSlaveNodeInformation
{
  void* Handle;              // Will be returned with transmit status
                             // Allows application to recognize frames
  uint8_t sourceId;
  uint8_t destinationId;
  uint8_t txOptions;
} SSendSlaveNodeInformation;


typedef union UTransmitParameters
{
  // Basis API
  SExploreInclusionRequest ExploreInclusionRequest;
  SExploreExclusionRequest ExploreExclusionRequest;
  SNetworkUpdateRequest NetworkUpdateRequest;
  SNodeInfoRequest NodeInfoRequest;
  SNodeInfo NodeInfo;
  SIncludedNodeInfo IncludedNodeInfo;
  STest Test;
  // Transport API
  SSendData SendData;
  SSendDataEx SendDataEx;
  SSendDataBridge SendDataBridge;
  SSendDataMulti SendDataMulti;
  SSendDataMultiEx SendDataMultiEx;
  SSendDataMultiBridge SendDataMultiBridge;
  // Controller API
  SSetSucNodeId SetSucNodeId;
  SSendSucNodeId SendSucNodeId;
  SAssignReturnRoute AssignReturnRoute;
  SDeleteReturnRoute DeleteReturnRoute;
  SSendReplication SendReplication;
  SSendReplicationReceiveComplete SendReplicationReceiveComplete;
  SSendSlaveNodeInformation  SendSlaveNodeInformation;
  // Slave API
  SRequestNewRouteDestinations RequestNewRouteDestinations;
} UTransmitParameters;

typedef struct SZwaveTransmitPackage
{
  EZwaveTransmitType eTransmitType;
  UTransmitParameters uTransmitParams;
} SZwaveTransmitPackage;


typedef struct SZWaveTransmitStatus
{
  void*           Handle;
  bool            bIsTxFrameLegal;  // False if frame rejected by protocol, can be due to content/configuration or due to timing (e.g. inclusion request when not in learn mode)
  uint8_t         TxStatus;
  TX_STATUS_TYPE  ExtendedTxStatus;
} SZWaveTransmitStatus;

typedef struct SZWaveGenerateRandomStatus
{
  uint8_t iLength;
  uint8_t aRandomNumber[32];
} SZWaveGenerateRandomStatus;

typedef struct SZWaveNodeInfoStatus
{
  uint8_t NodeId;
  NODEINFO NodeInfo;  // if NodeInfo.nodeType.generic = 0, node does not exist.
} SZWaveNodeInfoStatus;

typedef enum ELearnStatus
{
  ELEARNSTATUS_ASSIGN_COMPLETE,
  ELEARNSTATUS_ASSIGN_NODEID_DONE,        /*Node ID have been assigned*/
  ELEARNSTATUS_ASSIGN_RANGE_INFO_UPDATE , /*Node is doing Neighbor discovery*/
  ELEARNSTATUS_ASSIGN_INFO_PENDING,
  ELEARNSTATUS_ASSIGN_WAITING_FOR_FIND,
  ELEARNSTATUS_SMART_START_IN_PROGRESS,
  ELEARNSTATUS_LEARN_IN_PROGRESS,
  ELEARNSTATUS_LEARN_MODE_COMPLETED_TIMEOUT,
  ELEARNSTATUS_LEARN_MODE_COMPLETED_FAILED
} ELearnStatus;

typedef struct SZWaveLearnModeStatus
{
  ELearnStatus Status;   /* Status of learn mode */
} SZWaveLearnModeStatus;

typedef struct SZWaveInvalidTxRequestStatus
{
  uint8_t InvalidTxRequest;   /* Invalid value received (value should have been a valid EZWAVETRANSMITTYPE) */
} SZWaveInvalidTxRequestStatus;

typedef struct SZWaveInvalidCommandStatus
{
  uint8_t InvalidCommand;     /* Invalid value received (value should have been a valid EZWAVECOMMANDTYPE) */
} SZWaveInvalidCommandStatus;

typedef struct SZWaveGeneric8bStatus
{
  uint8_t result;     /* generic value of any API that uses a byte as a return value*/
} SZWaveGeneric8bStatus;

typedef struct SZWaveGenericBoolStatus
{
  bool result;     /* generic value of any API that uses a boolean as a return value*/
} SZWaveGenericBoolStatus;

typedef struct SZWaveGetRoutingInfoStatus
{
  uint8_t RoutingInfo[MAX_NODEMASK_LENGTH];
} SZWaveGetRoutingInfoStatus;

typedef struct SZWaveGetPriorityRouteStatus
{
  uint8_t bAnyRouteFound;
  uint8_t repeaters[MAX_REPEATERS];
  uint8_t routeSpeed;
} SZWaveGetPriorityRouteStatus;

typedef struct SZWaveSetPriorityRouteStatus
{
  uint8_t bRouteUpdated;
} SZWaveSetPriorityRouteStatus;

typedef struct SZWaveGetVirtualNodesStatus
{
  uint8_t vNodesMask[MAX_NODEMASK_LENGTH];
} SZWaveGetVirtualNodesStatus;

typedef struct SZWaveAesEcbStatus
{
  uint8_t outputData[16];
} SZWaveAesEcbStatus;

typedef struct SZWaveGetBackgroundRssiStatus
{
  int8_t rssi[NUM_CHANNELS];
} SZWaveGetBackgroundRssiStatus;

typedef struct SZWaveNetworkManagementStatus
{
  void *pHandle;
  /*learn status can be sourceID, destinationID , data length and data can be up to NODEPARM_MAX*/
  uint8_t statusInfo[3 + NODEPARM_MAX];
} SZWaveNetworkManagementStatus;

typedef struct SNvmBackupRestoreStatus
{
  uint8_t status;
} SNvmBackupRestoreStatus;

typedef struct SZWaveGetIncludedNodes
{
  NODE_MASK_TYPE node_id_list;
} SZWaveGetIncludedNodes;

typedef enum EZwaveCommandStatusType
{
  EZWAVECOMMANDSTATUS_TX = APPLICATION_INTERFACE_STATUS_ENUM_OFFSET,
  EZWAVECOMMANDSTATUS_GENERATE_RANDOM,
  EZWAVECOMMANDSTATUS_NODE_INFO,
  EZWAVECOMMANDSTATUS_LEARN_MODE_STATUS,
  EZWAVECOMMANDSTATUS_SET_DEFAULT,              // Received when protocol has finished starting up, and after receiving a set default command, has no content
  EZWAVECOMMANDSTATUS_INVALID_TX_REQUEST, // Received Tx Request that was not a EZWAVETRANSMITTYPE
  EZWAVECOMMANDSTATUS_INVALID_COMMAND,    // Receveid command that was not a EZWAVECOMMANDTYPE value
  EZWAVECOMMANDSTATUS_SET_RF_RECEIVE_MODE,
  EZWAVECOMMANDSTATUS_IS_NODE_WITHIN_DIRECT_RANGE,
  EZWAVECOMMANDSTATUS_GET_NEIGHBOR_COUNT,
  EZWAVECOMMANDSTATUS_ARE_NODES_NEIGHBOURS,
  EZWAVECOMMANDSTATUS_IS_FAILED_NODE_ID,
  EZWAVECOMMANDSTATUS_GET_ROUTING_TABLE_LINE,
  EZWAVECOMMANDSTATUS_SET_ROUTING_INFO,
  EZWAVECOMMANDSTATUS_STORE_NODE_INFO,
  EZWAVECOMMANDSTATUS_GET_PRIORITY_ROUTE,
  EZWAVECOMMANDSTATUS_SET_PRIORITY_ROUTE,
  EZWAVECOMMANDSTATUS_SET_SLAVE_LEARN_MODE,
  EZWAVECOMMANDSTATUS_SET_SLAVE_LEARN_MODE_RESULT,
  EZWAVECOMMANDSTATUS_IS_VIRTUAL_NODE,
  EZWAVECOMMANDSTATUS_GET_VIRTUAL_NODES,
  EZWAVECOMMANDSTATUS_GET_CONTROLLER_CAPABILITIES,
  EZWAVECOMMANDSTATUS_IS_PRIMARY_CTRL,
  EZWAVECOMMANDSTATUS_NETWORK_MANAGEMENT,
  EZWAVECOMMANDSTATUS_GET_BACKGROUND_RSSI,
  EZWAVECOMMANDSTATUS_AES_ECB,
  EZWAVECOMMANDSTATUS_REMOVE_FAILED_NODE_ID,
  EZWAVECOMMANDSTATUS_REPLACE_FAILED_NODE_ID,
  EZWAVECOMMANDSTATUS_NETWORK_LEARN_MODE_START,
  EZWAVECOMMANDSTATUS_ZW_SET_MAX_INCL_REQ_INTERVALS,
  EZWAVECOMMANDSTATUS_NVM_BACKUP_RESTORE,
  EZWAVECOMMANDSTATUS_PM_SET_POWERDOWN_CALLBACK,
  EZWAVECOMMANDSTATUS_ZW_GET_INCLUDED_NODES,
  EZWAVECOMMANDSTATUS_ZW_REQUESTNODENEIGHBORUPDATE,
  NUM_EZWAVECOMMANDSTATUS
} EZwaveCommandStatusType;

typedef union UCommandStatus
{
  SZWaveTransmitStatus TxStatus;
  SZWaveGenerateRandomStatus GenerateRandomStatus;
  SZWaveNodeInfoStatus NodeInfoStatus;
  SZWaveLearnModeStatus LearnModeStatus;
  SZWaveInvalidTxRequestStatus InvalidTxRequestStatus;
  SZWaveInvalidCommandStatus InvalidCommandStatus;
  SZWaveGeneric8bStatus SetRFReceiveModeStatus;
  SZWaveGeneric8bStatus IsNodeWithinDirectRange;
  SZWaveGeneric8bStatus GetNeighborCountStatus;
  SZWaveGeneric8bStatus AreNodesNeighborStatus;
  SZWaveGeneric8bStatus IsFailedNodeIDStatus;
  SZWaveGeneric8bStatus ReplaceFailedNodeStatus;
  SZWaveGetRoutingInfoStatus GetRoutingInfoStatus;
  SZWaveGeneric8bStatus SetRoutingInfoStatus;
  SZWaveGeneric8bStatus StoreNodeInfoStatus;
  SZWaveGetPriorityRouteStatus GetPriorityRouteStatus;
  SZWaveSetPriorityRouteStatus SetPriorityRouteStatus;
  SZWaveGeneric8bStatus SetSlaveLearnModeStatus;
  SZWaveGeneric8bStatus IsVirtualNodeStatus;
  SZWaveGetVirtualNodesStatus GetVirtualNodesStatus;
  SZWaveGeneric8bStatus GetControllerCapabilitiesStatus;
  SZWaveGeneric8bStatus IsPrimaryCtrlStatus;
  SZWaveAesEcbStatus  AesEcbStatus;
  SZWaveGetBackgroundRssiStatus GetBackgroundRssiStatus;
  SZWaveNetworkManagementStatus NetworkManagementStatus;
  SZWaveGeneric8bStatus FailedNodeIDStatus;
  SNvmBackupRestoreStatus NvmBackupRestoreStatus;
  SZWaveGenericBoolStatus SetPowerDownCallbackStatus;
  SZWaveGetIncludedNodes  GetIncludedNodes;
  SZWaveGeneric8bStatus RequestNodeNeigborUpdateStatus;
} UCommandStatus;

typedef struct SZwaveCommandStatusPackage
{
  EZwaveCommandStatusType eStatusType;
  UCommandStatus Content;
} SZwaveCommandStatusPackage;


typedef union UReceiveCmdPayload
{
  ZW_APPLICATION_TX_BUFFER rxBuffer;
  uint8_t padding[ZW_MAX_PAYLOAD_SIZE];
} UReceiveCmdPayload;

// Receive structs

typedef struct SReceiveSingle
{
  UReceiveCmdPayload Payload;
  uint8_t iLength;
  RECEIVE_OPTIONS_TYPE RxOptions;
} SReceiveSingle;

typedef struct SReceiveMulti
{
  NODE_MASK_TYPE NodeMask;
  UReceiveCmdPayload Payload;
  uint8_t iCommandLength;
  RECEIVE_OPTIONS_TYPE RxOptions;
} SReceiveMulti;

//Maximum size for the node info frame contained in the SReceiveNodeUpdate struct.
#define MAX_NODE_INFO_LENGTH 159

/**
*
* The Z Wave protocol MAY notify an application by sending
* \ref SReceiveNodeUpdate when a Node Information Frame has been received.
* The Z Wave protocol MAY refrain from sending the information if the protocol
* is currently expecting node information.
*
* A controller application MAY use the information provided by
* \ref SReceiveNodeUpdate to update local data structures.
*
* The Z Wave protocol MUST notify a controller application by calling
* \ref SReceiveNodeUpdate when a new node has been added or deleted
* from the controller through the network management features.
*
* The Z Wave protocol MUST pass \ref SReceiveNodeUpdate to application in
* response to \ref SNodeInfoRequest being passed to protocol by the controller
* application.
* The Z Wave protocol MAY notify a controller application by sending
* \ref SReceiveNodeUpdate when a Node Information Frame has been received.
* The Z Wave protocol MAY refrain from sending the information if the protocol
* is currently expecting a Node Information frame.
*
* \ref SReceiveNodeUpdate MUST be sent in a controller node operating
* as SIS each time a node is added or deleted by the primary controller.
* \ref SReceiveNodeUpdate MUST be sent in a controller node operating
* as SIS each time a node is added/deleted by an inclusion controller.
*
* A controller application MAY send a ZW_RequestNetWorkUpdate command
* to a SIS or SIS node. In response, the SIS MUST return update information
* for each node change since the last update handled by the requesting
* controller node.
* The application of the requesting controller node MAY receive multiple instances
* of \ref SReceiveNodeUpdate in response to application passing
* \ref SNetworkUpdateRequest to protocol.
*
* The Z Wave protocol MUST NOT send \ref SReceiveNodeUpdate in a
* controller node acting as primary controller or inclusion controller
* when a node is added or deleted.
*
* Any controller application MUST implement this function.
*
*
* \param[in] bNodeID
* \param[in] pCmd Pointer of the updated node's node info.
* \param[in] bLen The length of the pCmd parameter.
*
* serialapi{ZW->HOST: REQ | 0x49 | bStatus | bNodeID | bLen | basic | generic | specific | commandclasses[ ]}
*
* \ref SReceiveNodeUpdate via the Serial API also have the possibility for
* receiving the status UPDATE_STATE_NODE_INFO_REQ_FAILED, which means that a node
* did not acknowledge a \ref SNodeInfoRequest .
*
*/
typedef struct SReceiveNodeUpdate
{
  uint8_t Status;   // The status of the update process, value could be one of the following :
                    // \ref UPDATE_STATE_ADD_DONE A new node has been added to the network.
                    // \ref UPDATE_STATE_DELETE_DONE A node has been deleted from the network.
                    // \ref UPDATE_STATE_NODE_INFO_RECEIVED A node has sent its node info either unsolicited
                    // or as a response to a \ref NodeInfoRequest being passed to protocol.
                    // -\ref UPDATE_STATE_SUC_ID The SIS node Id was updated.

  uint8_t NodeId;   // The updated node's node ID (1..232).
  uint8_t iLength;                          // length of aPayload.
  uint8_t aPayload[MAX_NODE_INFO_LENGTH]; // the updated node's node info.
} SReceiveNodeUpdate;


/* Used by protocol to request/inform Application
* of Security based Events.Currently only an event for Client Side Authentication(CSA)
* has been defined - E_APPLICATION_SECURITY_EVENT_S2_INCLUSION_REQUEST_DSK_CSA.
*
* \ref E_APPLICATION_SECURITY_EVENT_S2_INCLUSION_REQUEST_DSK_CSA Security Event
*   Is posted by protocol when in S2 inclusion with CSA enabled and the
*   Server side DSK is needed.
*   Application must call ZW_SetSecurityS2InclusionCSA_DSK(s_SecurityS2InclusionCSAPublicDSK_t *)
*   with the retrieved Server / Controller side DSK.
*/
typedef struct SReceiveSecurityEvent
{
  e_application_security_event_t Event; // Event type
  uint8_t iLength;                      // Length of security event payload
  uint8_t aEventData[48];               // Security Event payload
} SReceiveSecurityEvent;


typedef enum EZwaveReceiveType
{
  EZWAVERECEIVETYPE_SINGLE = APPLICATION_INTERFACE_RECEIVE_ENUM_OFFSET,
  EZWAVERECEIVETYPE_MULTI,
  EZWAVERECEIVETYPE_NODE_UPDATE,
  EZWAVERECEIVETYPE_SECURITY_EVENT,
  NUM_EZWAVERECEIVETYPE
} EZwaveReceiveType;

typedef union UReceiveParameters
{
  SReceiveSingle Rx;
  SReceiveMulti RxMulti;
  SReceiveNodeUpdate RxNodeUpdate;
  SReceiveSecurityEvent RxSecurityEvent;
} UReceiveParameters;

typedef struct SZwaveReceivePackage
{
  EZwaveReceiveType eReceiveType;
  UReceiveParameters uReceiveParams;
} SZwaveReceivePackage;



// Command structs
// Generates true random word
typedef struct SCommandGenerateRandom
{
  uint8_t iLength;  // number of random bytes to generate
} SCommandGenerateRandom;

typedef struct SCommandNodeInfo
{
  uint8_t NodeId;
} SCommandNodeInfo;

typedef struct SCommandClearNetworkStatistics
{
  uint8_t Reserved;             // Not required set
} SCommandClearNetworkStatistics;

typedef enum ELearnMode
{
  ELEARNMODE_DISABLED = 0,
  ELEARNMODE_CLASSIC = 1,
  ELEARNMODE_NETWORK_WIDE_INCLUSION = 2,
  ELEARNMODE_NETWORK_WIDE_EXCLUSION = 3
} ELearnMode;

typedef struct SCommandSetLearnMode
{
  ELearnMode  eLearnMode;
  uint8_t useCB;
} SCommandSetLearnMode;

typedef struct SCommandSetSmartStartLearnMode
{
  E_NETWORK_LEARN_MODE_ACTION  eLearnMode;
  EResetReason_t eWakeUpReason;
} SCommandSetSmartStartLearnMode;

typedef struct SCommandSetRfPowerLevel
{
    uint8_t powerLevelDBm;
} SCommandSetRfPowerLevel;

typedef struct SCommandSetPromiscuousMode
{
  uint8_t Enable;
} SCommandSetPromiscuousMode;

typedef struct SCommandSetRfReceiveMode
{
  uint8_t mode;
} SCommandSetRfReceiveMode;

typedef struct SCommandGeniric8bParameter
{
  uint8_t value;
} SCommandGeniric8bParameter;

typedef struct SCommandAreNodesNeighbours
{
  uint8_t NodeA;
  uint8_t NodeB;
} SCommandAreNodesNeighbours;

typedef struct SCommandGetRoutingInfo
{
  uint8_t nodeID;
  uint8_t options;
} SCommandGetRoutingInfo;

typedef struct SCommandSetRoutingInfo
{
  uint8_t nodeID;
  uint8_t length;
  uint8_t nodeMask[MAX_NODEMASK_LENGTH];
} SCommandSetRoutingInfo;

typedef struct SCommandStoreNodeInfo
{
  uint8_t nodeID;
  uint8_t nodeInfo[6];
} SCommandStoreNodeInfo;

typedef struct SCommandStoreHomeID
{
  uint8_t homeID[4];
  uint8_t nodeID;
} SCommandStoreHomeID;

typedef struct SCommandSetPriorityRoute
{
  uint8_t nodeID;
  uint8_t repeaters[MAX_REPEATERS];
  uint8_t routeSpeed;
  uint8_t clearGolden;
} SCommandSetPriorityRoute;

typedef struct SCommandAesEcb
{
  uint8_t key[16];
  uint8_t inputData[16];
} SCommandAesEcb;

typedef struct SCommandNetworkManagement
{
  void *pHandle;              // Will be returned with transmit status
                             // Allows application to recognize frames
  uint8_t mode;
  uint8_t nodeID;            // when command is AddNodeDskToNetwork then nodeID is dsk[0]
  uint8_t dsk[7];            // Dsk bytes from 1 to 7
} SCommandNetworkManagement;

typedef struct SCommandGetPriorityRoute
{
  uint8_t *pPriRouteBuffer;              // Will be returned with transmit status
  uint8_t nodeID;
} SCommandGetPriorityRoute;

typedef struct SCommandFailedNodeIDCmd
{
  uint8_t nodeID;
  uint8_t normalPower;      //Only for replaced failed node api
} SCommandFailedNodeIDCmd;

typedef struct SCommandPMRegister {
    SPowerLock_t* handle;
    pm_type_t type;
} SCommandPMRegister;

typedef struct SCommandStayAwake {
    SPowerLock_t* handle;
    unsigned int msec;
} SCommandStayAwake;

typedef struct SCommandPMCancel {
    SPowerLock_t* handle;
} SCommandPMCancel;

typedef struct SCommandPMSetPowerDownCallback {
   void (*callback)(void);
} SCommandPMSetPowerDownCallback;

typedef struct SCommandSetLBTThreshold {
    uint8_t channel;
    int8_t level;
}
SCommandSetLBTThreshold;

typedef struct SCommandSetMaxInclReqInterval {
    uint32_t inclusionRequestInterval;
} SCommandSetMaxInclReqInterval;

typedef struct SCommandNvmBackupRestore {
   uint32_t offset;
   uint32_t length;
   uint8_t  *nvmData;
} SCommandNvmBackupRestore;

typedef struct SCommandSetSecurityKeys
{
  uint8_t keys;
} SCommandSetSecurityKeys;

typedef struct SCommandRequestNodeNeighborUpdate
{
  void* Handle;              // Will be returned with transmit status
                             // Allows application to recognize frames
  uint8_t NodeId;           // Node to have its neighbors discovered..
} SCommandRequestNodeNeighborUpdate;

typedef enum EZwaveCommandType
{
  EZWAVECOMMANDTYPE_GENERATE_RANDOM = APPLICATION_INTERFACE_COMMAND_ENUM_OFFSET, //64
  EZWAVECOMMANDTYPE_NODE_INFO,//65
  EZWAVECOMMANDTYPE_CLEAR_NETWORK_STATISTICS,//66
  EZWAVECOMMANDTYPE_SET_LEARN_MODE,//67
  EZWAVECOMMANDTYPE_SET_DEFAULT,  // 68
  EZWAVECOMMANDTYPE_SEND_DATA_ABORT, //69
  EZWAVECOMMANDTYPE_SET_PROMISCUOUS_MODE, //70
  EZWAVECOMMANDTYPE_SET_RF_RECEIVE_MODE, // 71
  EZWAVECOMMANDTYPE_IS_NODE_WITHIN_DIRECT_RANGE, //72
  EZWAVECOMMANDTYPE_GET_NEIGHBOR_COUNT,//73
  EZWAVECOMMANDTYPE_ARE_NODES_NEIGHBOURS,//74
  EZWAVECOMMANDTYPE_IS_FAILED_NODE_ID,//75
  EZWAVECOMMANDTYPE_GET_ROUTING_TABLE_LINE, //76
  EZWAVECOMMANDTYPE_SET_ROUTING_INFO,//77
  EZWAVECOMMANDTYPE_STORE_NODE_INFO,//78
  EZWAVECOMMANDTYPE_STORE_HOMEID,//79
  EZWAVECOMMANDTYPE_LOCK_ROUTE_RESPONSE, //80
  EZWAVECOMMANDTYPE_GET_PRIORITY_ROUTE,//81
  EZWAVECOMMANDTYPE_SET_PRIORITY_ROUTE,//82
  EZWAVECOMMANDTYPE_SET_SLAVE_LEARN_MODE,//83
  EZWAVECOMMANDTYPE_IS_VIRTUAL_NODE,//84
  EZWAVECOMMANDTYPE_GET_VIRTUAL_NODES,//85
  EZWAVECOMMANDTYPE_GET_CONTROLLER_CAPABILITIES,//86
  EZWAVECOMMANDTYPE_SET_ROUTING_MAX,//87
  EZWAVECOMMANDTYPE_IS_PRIMARY_CTRL,//88
  EZWAVECOMMANDTYPE_ADD_NODE_TO_NETWORK,//89
  EZWAVECOMMANDTYPE_REMOVE_NODE_FROM_NETWORK, //90
  EZWAVECOMMANDTYPE_AES_ECB,//91
  EZWAVECOMMANDTYPE_GET_BACKGROUND_RSSI,//92
  EZWAVECOMMANDTYPE_REMOVE_FAILED_NODE_ID,//93
  EZWAVECOMMANDTYPE_REPLACE_FAILED_NODE_ID,//94
  EZWAVECOMMANDTYPE_PM_STAY_AWAKE,//95
  EZWAVECOMMANDTYPE_PM_CANCEL,//96
  EZWAVECOMMANDTYPE_PM_REGISTER,//97
  EZWAVECOMMANDTYPE_ZW_UPDATE_CTRL_NODE_INFORMATION,  //98
  EZWAVECOMMANDTYPE_ZW_SET_LBT_THRESHOLD,  //99
  EZWAVECOMMANDTYPE_ADD_NODE_DSK_TO_NETWORK, //100
  EZWAVECOMMANDTYPE_NETWORK_LEARN_MODE_START, //101
  EZWAVECOMMANDTYPE_CREAT_NEW_PRIMARY_CTRL, //102
  EZWAVECOMMANDTYPE_CONTROLLER_CHANGE, //103
  EZWAVECOMMANDTYPE_CLEAR_TX_TIMERS,  //104
  EZWAVECOMMANDTYPE_ZW_SET_MAX_INCL_REQ_INTERVALS,  //105
  EZWAVECOMMANDTYPE_NVM_BACKUP_OPEN,// 106
  EZWAVECOMMANDTYPE_NVM_BACKUP_READ,// 107
  EZWAVECOMMANDTYPE_NVM_BACKUP_WRITE,// 108
  EZWAVECOMMANDTYPE_NVM_BACKUP_CLOSE,// 109
  EZWAVECOMMANDTYPE_PM_SET_POWERDOWN_CALLBACK, // 110
  EZWAVECOMMANDTYPE_SET_SECURITY_KEYS, // 111
  EZWAVECOMMANDTYPE_SOFT_RESET, // 112
  EZWAVECOMMANDTYPE_BOOTLOADER_REBOOT, // 113
  EZWAVECOMMANDTYPE_REMOVE_NODEID_FROM_NETWORK, // 114
  EZWAVECOMMANDTYPE_ZW_GET_INCLUDED_NODES,  //115
  EZWAVECOMMANDTYPE_REQUESTNODENEIGHBORUPDATE, //116
  NUM_EZWAVECOMMANDTYPE
} EZwaveCommandType;

typedef union UCommandParameters
{
  SCommandGenerateRandom GenerateRandom;
  SCommandNodeInfo NodeInfo;
  SCommandClearNetworkStatistics ClearNetworkStatistics;
  SCommandSetLearnMode SetLearnMode;
  SCommandSetSmartStartLearnMode SetSmartStartLearnMode;
  SCommandSetPromiscuousMode SetPromiscuousMode;
  SCommandSetRfReceiveMode SetRfReceiveMode;
  SCommandGeniric8bParameter IsNodeWithinDirectRange;
  SCommandGeniric8bParameter GetNeighborCount;
  SCommandAreNodesNeighbours  AreNodesNeighbours;
  SCommandNetworkManagement IsFailedNodeID;
  SCommandNetworkManagement SetSlaveLearnMode;
  SCommandGetRoutingInfo  GetRoutingInfo;
  SCommandSetRoutingInfo  SetRoutingInfo;
  SCommandStoreNodeInfo   StoreNodeInfo;
  SCommandStoreHomeID StoreHomeID;
  SCommandGeniric8bParameter LockRouteResponse;
  SCommandGetPriorityRoute GetPriorityRoute;
  SCommandSetPriorityRoute SetPriorityRoute;
  SCommandGeniric8bParameter IsVirtualNode;
  SCommandGeniric8bParameter SetRoutingMax;
  SCommandNetworkManagement  NetworkManagement;
  SCommandFailedNodeIDCmd  FailedNodeIDCmd;
  SCommandAesEcb   AesEcb;
  SCommandPMRegister PMRegister;
  SCommandStayAwake PMStayAwake;
  SCommandPMCancel PMCancel;
  SCommandPMSetPowerDownCallback PMSetPowerDownCallback;
  SCommandGeniric8bParameter UpdateCtrlNodeInformation;
  SCommandSetLBTThreshold SetLBTThreshold;
  SCommandSetRfPowerLevel SetRfPowerLevel;
  SCommandSetMaxInclReqInterval SetMaxInclReqInterval;
  SCommandNvmBackupRestore    NvmBackupRestore;
  SCommandSetSecurityKeys SetSecurityKeys;
  SCommandRequestNodeNeighborUpdate  RequestNodeNeighborUpdate;  
} UCommandParameters;


typedef struct SZwaveCommandPackage
{
  EZwaveCommandType eCommandType;
  UCommandParameters uCommandParams;
} SZwaveCommandPackage;


// Each item type to be put on queues between app and protocol, has been given a value range for ID enums.
// They do not overlap, as this makes it easy to detect if an item has errournously been put on the wrong queue.
STATIC_ASSERT(NUM_EZWAVETRANSMITTYPE < (APPLICATION_INTERFACE_TRANSMIT_ENUM_OFFSET + 0x40), STATIC_ASSERT_FAILED_interface_tx_enum_overlap);
STATIC_ASSERT(NUM_EZWAVECOMMANDTYPE < (APPLICATION_INTERFACE_COMMAND_ENUM_OFFSET + 0x40), STATIC_ASSERT_FAILED_interface_command_enum_overlap);
STATIC_ASSERT(NUM_EZWAVERECEIVETYPE < (APPLICATION_INTERFACE_RECEIVE_ENUM_OFFSET + 0x40), STATIC_ASSERT_FAILED_interface_receive_enum_overlap);
STATIC_ASSERT(NUM_EZWAVECOMMANDSTATUS < (APPLICATION_INTERFACE_STATUS_ENUM_OFFSET + 0x40), STATIC_ASSERT_FAILED_interface_status_enum_overlap);


#endif /* _ZW_APPLICATION_TRANSPORT_INTERFACE_H_ */
