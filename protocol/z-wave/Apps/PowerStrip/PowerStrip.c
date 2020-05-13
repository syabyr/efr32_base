/**
 * Z-Wave Certified Application Power Strip
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include "config_rf.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "SizeOf.h"
#include "Assert.h"
#include "DebugPrintConfig.h"
//#define DEBUGPRINT
#include "DebugPrint.h"
#include "config_app.h"
#include "ZAF_app_version.h"
#include <ZAF_file_ids.h>
#include "nvm3.h"
#include "ZAF_nvm3_app.h"
#include <em_system.h>

#include <ZW_slave_api.h>
#include <ZW_classcmd.h>
#include <ZW_TransportLayer.h>

#include <ZAF_uart_utils.h>

#include <misc.h>

/*IO control*/
#include <board.h>

#include <ev_man.h>
#include <AppTimer.h>
#include <SwTimer.h>
#include <EventDistributor.h>
#include <ZW_system_startup_api.h>
#include <ZW_application_transport_interface.h>

#include <association_plus.h>
#include <agi.h>
#include <CC_Association.h>
#include <CC_AssociationGroupInfo.h>
#include <CC_Basic.h>
#include <CC_BinarySwitch.h>
#include <CC_DeviceResetLocally.h>
#include <CC_Indicator.h>
#include <CC_MultiChan.h>
#include <CC_MultiChanAssociation.h>
#include <CC_MultilevelSwitch.h>
#include <CC_Notification.h>
#include <CC_PowerLevel.h>
#include <CC_Security.h>
#include <CC_Supervision.h>
#include <CC_Version.h>
#include <CC_ZWavePlusInfo.h>
#include <CC_FirmwareUpdate.h>
#include <CC_ManufacturerSpecific.h>

#include <notification.h>
#include <multilevel_switch.h>

#include "zaf_event_helper.h"
#include "zaf_job_helper.h"
#include <ZAF_Common_helper.h>
#include <ZAF_network_learn.h>
#include "ZAF_TSE.h"
#include <ota_util.h>
#include <ZAF_CmdPublisher.h>
#include <em_wdog.h>
#include "events.h"

/****************************************************************************/
/* Application specific button and LED definitions                          */
/****************************************************************************/

#define OUTLET1_TOGGLE_BTN        APP_BUTTON_A
#define OUTLET2_DIMMER_BTN        APP_BUTTON_B
#define NOTIFICATION_TOGGLE_BTN   APP_BUTTON_C

#define OUTLET1_STATUS_LED   APP_LED_A
#define OUTLET2_LEVEL_LED_R  APP_RGB_R
#define OUTLET2_LEVEL_LED_G  APP_RGB_G
#define OUTLET2_LEVEL_LED_B  APP_RGB_B

/* Ensure we did not allocate the same physical button or led to more than one function */
STATIC_ASSERT((APP_BUTTON_LEARN_RESET != OUTLET1_TOGGLE_BTN) &&
              (APP_BUTTON_LEARN_RESET != OUTLET2_DIMMER_BTN) &&
              (APP_BUTTON_LEARN_RESET != NOTIFICATION_TOGGLE_BTN) &&
              (OUTLET1_TOGGLE_BTN != OUTLET2_DIMMER_BTN) &&
              (OUTLET1_TOGGLE_BTN != NOTIFICATION_TOGGLE_BTN) &&
              (OUTLET2_DIMMER_BTN != NOTIFICATION_TOGGLE_BTN),
              STATIC_ASSERT_FAILED_button_overlap);
STATIC_ASSERT((APP_LED_INDICATOR != OUTLET1_STATUS_LED) &&
              (APP_LED_INDICATOR != OUTLET2_LEVEL_LED_R) &&
              (APP_LED_INDICATOR != OUTLET2_LEVEL_LED_G) &&
              (APP_LED_INDICATOR != OUTLET2_LEVEL_LED_B) &&
              (OUTLET1_STATUS_LED != OUTLET2_LEVEL_LED_R) &&
              (OUTLET1_STATUS_LED != OUTLET2_LEVEL_LED_G) &&
              (OUTLET1_STATUS_LED != OUTLET2_LEVEL_LED_B) &&
              (OUTLET2_LEVEL_LED_R != OUTLET2_LEVEL_LED_G) &&
              (OUTLET2_LEVEL_LED_R != OUTLET2_LEVEL_LED_B) &&
              (OUTLET2_LEVEL_LED_G != OUTLET2_LEVEL_LED_B),
              STATIC_ASSERT_FAILED_led_overlap);

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

typedef enum _STATE_APP_
{
  STATE_APP_STARTUP,
  STATE_APP_IDLE,
  STATE_APP_LEARN_MODE,
  STATE_APP_RESET,
  STATE_APP_TRANSMIT_DATA
} STATE_APP;

#define LEVEL_MIN           (1)
#define LEVEL_MAX           (99)
#define DIMMER_FREQ         (10)
#define DIM_SPEED           (10)
#define SWITCH_ON           (0x01)
#define SWITCH_DIM_UP       (0x02)
#define SWITCH_IS_DIMMING   (0x04)
#define SWITCH_OFF          (0x00)
#define BIN_SWITCH_1        (ENDPOINT_1 - 1)
#define MULTILEVEL_SWITCH_1 (ENDPOINT_2 - 1)

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t cmdClassListNonSecureNotIncluded[] =
{
  COMMAND_CLASS_ZWAVEPLUS_INFO,
  COMMAND_CLASS_SWITCH_BINARY_V2,
  COMMAND_CLASS_ASSOCIATION,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_NOTIFICATION_V3,
  COMMAND_CLASS_TRANSPORT_SERVICE_V2,
  COMMAND_CLASS_VERSION,
  COMMAND_CLASS_MANUFACTURER_SPECIFIC,
  COMMAND_CLASS_DEVICE_RESET_LOCALLY,
  COMMAND_CLASS_INDICATOR,
  COMMAND_CLASS_POWERLEVEL,
  COMMAND_CLASS_SECURITY,
  COMMAND_CLASS_SECURITY_2,
  COMMAND_CLASS_MULTI_CHANNEL_V4,
  COMMAND_CLASS_SUPERVISION,
  COMMAND_CLASS_FIRMWARE_UPDATE_MD_V5
};

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t cmdClassListNonSecureIncludedSecure[] =
{
  COMMAND_CLASS_ZWAVEPLUS_INFO,
  COMMAND_CLASS_TRANSPORT_SERVICE_V2,
  COMMAND_CLASS_SECURITY,
  COMMAND_CLASS_SECURITY_2,
  COMMAND_CLASS_SUPERVISION
};

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t cmdClassListSecure[] =
{
  COMMAND_CLASS_SWITCH_BINARY_V2,
  COMMAND_CLASS_ASSOCIATION,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_NOTIFICATION_V3,
  COMMAND_CLASS_VERSION,
  COMMAND_CLASS_MANUFACTURER_SPECIFIC,
  COMMAND_CLASS_DEVICE_RESET_LOCALLY,
  COMMAND_CLASS_INDICATOR,
  COMMAND_CLASS_POWERLEVEL,
  COMMAND_CLASS_MULTI_CHANNEL_V4,
  COMMAND_CLASS_FIRMWARE_UPDATE_MD_V5
};

/**
 * Structure includes application node information list's and device type.
 */
static app_node_information_t m_AppNIF =
{
  cmdClassListNonSecureNotIncluded, sizeof(cmdClassListNonSecureNotIncluded),
  cmdClassListNonSecureIncludedSecure, sizeof(cmdClassListNonSecureIncludedSecure),
  cmdClassListSecure, sizeof(cmdClassListSecure),
  DEVICE_OPTIONS_MASK, {GENERIC_TYPE, SPECIFIC_TYPE}
};

/**
* Set up security keys to request when joining a network.
* These are taken from the config_app.h header file.
*/
static const uint8_t SecureKeysRequested = REQUESTED_SECURITY_KEYS;

static SAppNodeInfo_t AppNodeInfo =
{
  .DeviceOptionsMask = DEVICE_OPTIONS_MASK,
  .NodeType.generic = GENERIC_TYPE,
  .NodeType.specific = SPECIFIC_TYPE,
  .CommandClasses.UnSecureIncludedCC.iListLength = sizeof_array(cmdClassListNonSecureNotIncluded),
  .CommandClasses.UnSecureIncludedCC.pCommandClasses = cmdClassListNonSecureNotIncluded,
  .CommandClasses.SecureIncludedUnSecureCC.iListLength = sizeof_array(cmdClassListNonSecureIncludedSecure),
  .CommandClasses.SecureIncludedUnSecureCC.pCommandClasses = cmdClassListNonSecureIncludedSecure,
  .CommandClasses.SecureIncludedSecureCC.iListLength = sizeof_array(cmdClassListSecure),
  .CommandClasses.SecureIncludedSecureCC.pCommandClasses = cmdClassListSecure
};

static const SRadioConfig_t RadioConfig =
{
  .iListenBeforeTalkThreshold = ELISTENBEFORETALKTRESHOLD_DEFAULT,
  .iTxPowerLevelMax = APP_MAX_TX_POWER,
  .iTxPowerLevelAdjust = APP_MEASURED_0DBM_TX_POWER,
  .eRegion = APP_FREQ
};

static const SProtocolConfig_t ProtocolConfig = {
  .pVirtualSlaveNodeInfoTable = NULL,
  .pSecureKeysRequested = &SecureKeysRequested,
  .pNodeInfo = &AppNodeInfo,
  .pRadioConfig = &RadioConfig
};

/****************************************************************************/
/* Endpoint 1 command class lists                                           */
/****************************************************************************/

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t ep1_noSec_InclNonSecure[] =
{
  COMMAND_CLASS_ZWAVEPLUS_INFO,
  COMMAND_CLASS_SWITCH_BINARY_V2,
  COMMAND_CLASS_ASSOCIATION,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_NOTIFICATION_V3,
  COMMAND_CLASS_SUPERVISION,
  COMMAND_CLASS_SECURITY,
  COMMAND_CLASS_SECURITY_2
};

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t ep1_noSec_InclSecure[] =
{
  COMMAND_CLASS_ZWAVEPLUS_INFO,
  COMMAND_CLASS_SUPERVISION,
  COMMAND_CLASS_SECURITY,
  COMMAND_CLASS_SECURITY_2
};

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t ep1_sec_InclSecure[] =
{
  COMMAND_CLASS_SWITCH_BINARY_V2,
  COMMAND_CLASS_ASSOCIATION,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_NOTIFICATION_V3
};

/****************************************************************************/
/* Endpoint 2 command class lists                                           */
/****************************************************************************/

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t ep2_noSec_InclNonSecure[] =
{
  COMMAND_CLASS_ZWAVEPLUS_INFO,
  COMMAND_CLASS_SWITCH_MULTILEVEL,
  COMMAND_CLASS_ASSOCIATION,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_NOTIFICATION_V3,
  COMMAND_CLASS_SUPERVISION,
  COMMAND_CLASS_SECURITY,
  COMMAND_CLASS_SECURITY_2
};

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t ep2_noSec_InclSecure[] =
{
  COMMAND_CLASS_ZWAVEPLUS_INFO,
  COMMAND_CLASS_SUPERVISION,
  COMMAND_CLASS_SECURITY,
  COMMAND_CLASS_SECURITY_2
};

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t ep2_sec_InclSecure[] =
{
  COMMAND_CLASS_SWITCH_MULTILEVEL,
  COMMAND_CLASS_ASSOCIATION,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_NOTIFICATION_V3
};

static EP_NIF endpointsNIF[NUMBER_OF_ENDPOINTS] =
{
 /*
  * Endpoint 1
  */
  {
    GENERIC_TYPE_SWITCH_BINARY,
    SPECIFIC_TYPE_NOT_USED,
    {
      {ep1_noSec_InclNonSecure, sizeof(ep1_noSec_InclNonSecure)},
      {{ep1_noSec_InclSecure, sizeof(ep1_noSec_InclSecure)}, {ep1_sec_InclSecure, sizeof(ep1_sec_InclSecure)}}
    }
  },
  /*
   * Endpoint 2
   */
  {
    GENERIC_TYPE_SWITCH_MULTILEVEL,
    SPECIFIC_TYPE_NOT_USED,
    {
      {ep2_noSec_InclNonSecure, sizeof(ep2_noSec_InclNonSecure)},
      {{ep2_noSec_InclSecure, sizeof(ep2_noSec_InclSecure)}, {ep2_sec_InclSecure, sizeof(ep2_sec_InclSecure)}}
    }
  }
};

static EP_FUNCTIONALITY_DATA endPointFunctionality =
{
  {
    NUMBER_OF_INDIVIDUAL_ENDPOINTS,     /**< nbrIndividualEndpoints 7 bit*/
    RES_ZERO,                           /**< resIndZeorBit 1 bit*/
    NUMBER_OF_AGGREGATED_ENDPOINTS,     /**< nbrAggregatedEndpoints 7 bit*/
    RES_ZERO,                           /**< resAggZeorBit 1 bit*/
    RES_ZERO,                           /**< resZero 6 bit*/
    ENDPOINT_IDENTICAL_DEVICE_CLASS_NO,/**< identical 1 bit*/
    ENDPOINT_DYNAMIC_NO                /**< dynamic 1 bit*/
  }
};

/**
 * Setup AGI lifeline table from config_app.h
 */
static const cc_group_t  agiTableLifeLine[] = {AGITABLE_LIFELINE_GROUP};

static const AGI_PROFILE lifelineProfile =
{
 ASSOCIATION_GROUP_INFO_REPORT_PROFILE_GENERAL_V2,
 ASSOCIATION_GROUP_INFO_REPORT_PROFILE_GENERAL_LIFELINE_V2
};

static const cc_group_t agiTableLifeLineEP1[] = {AGITABLE_LIFELINE_GROUP_EP1};
static const cc_group_t agiTableLifeLineEP2[] = {AGITABLE_LIFELINE_GROUP_EP2};

/**
 * Setup AGI root device groups table from config_app.h
 */
static const AGI_GROUP agiTableRootDeviceGroups[] = {AGITABLE_ROOTDEVICE_GROUPS};
static const AGI_GROUP agiTableEndpoint1Groups[] = {AGITABLE_ENDPOINT_1_GROUPS};
static const AGI_GROUP agiTableEndpoint2Groups[] = {AGITABLE_ENDPOINT_2_GROUPS};

static const AGI_PROFILE endpointProfile =
{
  ASSOCIATION_GROUP_INFO_REPORT_PROFILE_NOTIFICATION,
  NOTIFICATION_REPORT_POWER_MANAGEMENT_V4
};

#define ENDPOINT_PROFILE  &endpointProfile

/**************************************************************************************************
 * Configuration for Z-Wave Plus Info CC
 **************************************************************************************************
 */
static SEndpointIcon ZWavePlusEndpointIcons[] = {ENDPOINT_ICONS};

const SEndpointIconList EndpointIconList = {
                                      .pEndpointInfo = ZWavePlusEndpointIcons,
                                      .endpointInfoSize = sizeof_array(ZWavePlusEndpointIcons)
};

static const SCCZWavePlusInfo CCZWavePlusInfo = {
                               .pEndpointIconList = &EndpointIconList,
                               .roleType = APP_ROLE_TYPE,
                               .nodeType = APP_NODE_TYPE,
                               .installerIconType = APP_ICON_TYPE,
                               .userIconType = APP_USER_ICON_TYPE
};

/**
 * Application state-machine state.
 */
static STATE_APP currentState = STATE_APP_IDLE;

/**
 * Parameter is used to save wakeup reason after ApplicationInit(..)
 */
EResetReason_t g_eResetReason;

typedef struct _BIN_SWITCH_{
  uint8_t switchStatus;
}BIN_SWITCH;

typedef struct  _MULTILEVEL_SWITCH_{
  uint8_t switchStatus;
  uint8_t level;
}MULTILEVEL_SWITCH;

typedef struct _POWER_STRIP_{
  BIN_SWITCH binSwitch;
  MULTILEVEL_SWITCH dimmer;
}POWER_STRIP;

POWER_STRIP powerStrip;

static ESwTimerStatus notificationOverLoadTimerStatus = ESWTIMER_STATUS_FAILED;
// Timer
static SSwTimer NotificationTimer;
static SSwTimer SupervisionTimer;

static bool notificationOverLoadActiveState =  false;
static uint8_t notificationOverLoadendpoint = 0;

/**
 * root grp, endpoint, endpoint group
 */
ASSOCIATION_ROOT_GROUP_MAPPING rootGroupMapping[] = { ASSOCIATION_ROOT_GROUP_MAPPING_CONFIG};

uint8_t multiLevelEndpointIDList = ENDPOINT_2;

uint8_t supportedEvents = NOTIFICATION_EVENT_POWER_MANAGEMENT_OVERLOADED_DETECTED;

static TaskHandle_t g_AppTaskHandle;

#ifdef DEBUGPRINT
static uint8_t m_aDebugPrintBuffer[96];
#endif

// Pointer to AppHandles that is passed as input to ApplicationTask(..)
SApplicationHandles* g_pAppHandles;

// Prioritized events that can wakeup protocol thread.
typedef enum EApplicationEvent
{
  EAPPLICATIONEVENT_TIMER = 0,
  EAPPLICATIONEVENT_ZWRX,
  EAPPLICATIONEVENT_ZWCOMMANDSTATUS,
  EAPPLICATIONEVENT_APP
} EApplicationEvent;

static void EventHandlerZwRx(void);
static void EventHandlerZwCommandStatus(void);
static void EventHandlerApp(void);

// Event distributor object
static SEventDistributor g_EventDistributor;

// Event distributor event handler table
static const EventDistributorEventHandler g_aEventHandlerTable[4] =
{
  AppTimerNotificationHandler,  // Event 0
  EventHandlerZwRx,
  EventHandlerZwCommandStatus,
  EventHandlerApp
};

#define APP_EVENT_QUEUE_SIZE 5

/**
 * The following four variables are used for the application event queue.
 */
static SQueueNotifying m_AppEventNotifyingQueue;
static StaticQueue_t m_AppEventQueueObject;
static EVENT_APP eventQueueStorage[APP_EVENT_QUEUE_SIZE];
static QueueHandle_t m_AppEventQueue;

void AppResetNvm(void);

/**
 * Used by the Supervision Get handler. Holds RX options.
 */
static RECEIVE_OPTIONS_TYPE_EX rxOptionSupervision;

/**
 * Used by the Supervision Get handler. Holds Supervision session ID.
 */
static uint8_t sessionID;

/* True Status Engine (TSE) variables */
/* TSE simulated RX option for local change addressed to End Point 1 */
static RECEIVE_OPTIONS_TYPE_EX zaf_tse_local_ep1_actuation = {
    .rxStatus = 0,        /* rxStatus, verified by the TSE for Multicast */
    .securityKey = 0,     /* securityKey, ignored by the TSE */
    .sourceNode = {0,0},  /* sourceNode (nodeId, endpoint), verified against lifeline destinations by the TSE */
    .destNode = {0,1}     /* destNode (nodeId, endpoint), verified by the TSE for local endpoint */
};

/* TSE simulated RX option for local change addressed to End Point 2 */
static RECEIVE_OPTIONS_TYPE_EX zaf_tse_local_ep2_actuation = {
    .rxStatus = 0,        /* rxStatus, verified by the TSE for Multicast */
    .securityKey = 0,     /* securityKey, ignored by the TSE */
    .sourceNode = {0,0},  /* sourceNode (nodeId, endpoint), verified against lifeline destinations by the TSE */
    .destNode = {0,2}     /* destNode (nodeId, endpoint), verified by the TSE for local endpoint */
};

/* Indicate here which End Points (NOT including root device) support each command class */
static const uint8_t endpoints_Supporting_BinarySwitch_CC[] = {1};
static const uint8_t endpoints_Supporting_MultilevelSwitch_CC[] = {2};
s_CC_binarySwitch_data_t ZAF_TSE_BinarySwitchData[sizeof_array(endpoints_Supporting_BinarySwitch_CC)];
s_CC_multilevelSwitch_data_t ZAF_TSE_MultilevelSwitchData[sizeof_array(endpoints_Supporting_MultilevelSwitch_CC)];
static s_CC_indicator_data_t ZAF_TSE_localActuationIdentifyData = {
  .rxOptions = {
    .rxStatus = 0,          /* rxStatus, verified by the TSE for Multicast */
    .securityKey = 0,       /* securityKey, ignored by the TSE */
    .sourceNode = {0,0},    /* sourceNode (nodeId, endpoint), verified against lifeline destinations by the TSE */
    .destNode = {0,0}       /* destNode (nodeId, endpoint), verified by the TSE for local endpoint */
  },
  .indicatorId = 0x50      /* Identify Indicator*/
};

s_CC_notification_data_t ZAF_TSE_NotificationData;

/* CC MultilevelSwitch handler might take some time for dimming, the application needs a timer for this */
static SSwTimer ZAF_TSE_dimming_ActivationTimer;

static nvm3_Handle_t* pFileSystemApplication;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/
void DeviceResetLocallyDone(TRANSMISSION_RESULT * pTransmissionResult);
void DeviceResetLocally(void);
STATE_APP GetAppState(void);
void AppStateManager(EVENT_APP event);
static void ChangeState(STATE_APP newState);

uint8_t CC_BinarySwitch_getEndpointIndex(uint8_t endpoint);
uint8_t CC_MultilevelSwitch_getEndpointIndex(uint8_t endpoint);

static void ApplicationTask(SApplicationHandles* pAppHandles);

bool LoadConfiguration(void);
void SetDefaultConfiguration(void);

static void LearnCompleted(void);

void ZCB_JobStatus(TRANSMISSION_RESULT * pTransmissionResult);
void ZCB_NotificationTimerCallback(SSwTimer *pTimer);
static void ToggleSwitch(uint8_t switchID);
static void UpdateSwitchLevel(uint8_t switchID, uint8_t level );
static void notificationToggle(void);
void ZCB_CommandClassSupervisionGetReceived(SUPERVISION_GET_RECEIVED_HANDLER_ARGS * pArgs);
void ZCB_SupervisionTimerCallback(SSwTimer *pTimer);
void ZAF_TSE_DimmingTimerCallback(SSwTimer *pTimer);


/**
* @brief Called when protocol puts a frame on the ZwRxQueue.
*/
static void EventHandlerZwRx(void)
{
  QueueHandle_t Queue = g_pAppHandles->ZwRxQueue;
  SZwaveReceivePackage RxPackage;

  // Handle incoming replies
  while (xQueueReceive(Queue, (uint8_t*)(&RxPackage), 0) == pdTRUE)
  {
    DPRINT("Incoming Rx msg\r\n");

    switch (RxPackage.eReceiveType)
    {
      case EZWAVERECEIVETYPE_SINGLE:
      {
        ZAF_CP_CommandPublish(ZAF_getCPHandle(), &RxPackage);
        break;
      }

      case EZWAVERECEIVETYPE_NODE_UPDATE:
      {
        /*ApplicationSlaveUpdate() was called from this place in version prior to SDK7*/
        break;
      }

      case EZWAVERECEIVETYPE_SECURITY_EVENT:
      {
        /*ApplicationSecurityEvent() was used to support CSA, not needed in SDK7*/
        break;
      }

    default:
      {
        ASSERT(false);
        break;
      }
    }
  }

}

/**
* @brief Triggered when protocol puts a message on the ZwCommandStatusQueue.
*/
static void EventHandlerZwCommandStatus(void)
{
  QueueHandle_t Queue = g_pAppHandles->ZwCommandStatusQueue;
  SZwaveCommandStatusPackage Status;

  // Handle incoming replies
  while (xQueueReceive(Queue, (uint8_t*)(&Status), 0) == pdTRUE)
  {
    DPRINT("Incoming Status msg\r\n");

    switch (Status.eStatusType)
    {
      case EZWAVECOMMANDSTATUS_TX:
      {
        SZWaveTransmitStatus* pTxStatus = &Status.Content.TxStatus;
        if (!pTxStatus->bIsTxFrameLegal)
        {
          DPRINT("Auch - not sure what to do\r\n");
        }
        else
        {
          DPRINTF("Tx Status received: %u\r\n", pTxStatus->TxStatus);
          if (ZAF_TSE_TXCallback == pTxStatus->Handle)
          {
            // TSE do not use the TX result to anything. Hence, we pass NULL.
            ZAF_TSE_TXCallback(NULL);
          }
          else if (pTxStatus->Handle)
          {
            void(*pCallback)(uint8_t txStatus, TX_STATUS_TYPE* extendedTxStatus) = pTxStatus->Handle;
            pCallback(pTxStatus->TxStatus, &pTxStatus->ExtendedTxStatus);

          }
        }

        break;
      }

      case EZWAVECOMMANDSTATUS_GENERATE_RANDOM:
      {
        DPRINT("Generate Random status\r\n");
        break;
      }

      case EZWAVECOMMANDSTATUS_LEARN_MODE_STATUS:
      {
        DPRINTF("Learn status %d\r\n", Status.Content.LearnModeStatus.Status);
        if (ELEARNSTATUS_ASSIGN_COMPLETE == Status.Content.LearnModeStatus.Status)
        {
          LearnCompleted();
        }
        else if(ELEARNSTATUS_SMART_START_IN_PROGRESS == Status.Content.LearnModeStatus.Status)
        {
          ZAF_EventHelperEventEnqueue(EVENT_APP_SMARTSTART_IN_PROGRESS);
        }
        else if(ELEARNSTATUS_LEARN_IN_PROGRESS == Status.Content.LearnModeStatus.Status)
        {
          ZAF_EventHelperEventEnqueue(EVENT_APP_LEARN_IN_PROGRESS);
        }
        else if(ELEARNSTATUS_LEARN_MODE_COMPLETED_TIMEOUT == Status.Content.LearnModeStatus.Status)
        {
          ZAF_EventHelperEventEnqueue((EVENT_APP) EVENT_SYSTEM_LEARNMODE_FINISHED);
        }
        else if(ELEARNSTATUS_LEARN_MODE_COMPLETED_FAILED == Status.Content.LearnModeStatus.Status)
        {
          //Reformats protocol and application NVM. Then soft reset.
          ZAF_EventHelperEventEnqueue((EVENT_APP) EVENT_SYSTEM_RESET);
        }
        break;
      }

      case EZWAVECOMMANDSTATUS_NETWORK_LEARN_MODE_START:
      {
        break;
      }

      case EZWAVECOMMANDSTATUS_SET_DEFAULT:
      { // Received when protocol is started (not implemented yet), and when SetDefault command is completed
        DPRINTF("Protocol Ready\r\n");
        ZAF_EventHelperEventEnqueue(EVENT_APP_FLUSHMEM_READY);
        break;
      }

      case EZWAVECOMMANDSTATUS_INVALID_TX_REQUEST:
      {
        DPRINTF("ERROR: Invalid TX Request to protocol - %d", Status.Content.InvalidTxRequestStatus.InvalidTxRequest);
        break;
      }

      case EZWAVECOMMANDSTATUS_INVALID_COMMAND:
      {
        DPRINTF("ERROR: Invalid command to protocol - %d", Status.Content.InvalidCommandStatus.InvalidCommand);
        break;
      }

      case EZWAVECOMMANDSTATUS_ZW_SET_MAX_INCL_REQ_INTERVALS:
      {
        // Status response from calling the ZAF_SetMaxInclusionRequestIntervals function
        DPRINTF("SetMaxInclusionRequestIntervals status: %s\r\n",
                 Status.Content.NetworkManagementStatus.statusInfo[0] == true ? "SUCCESS" : "FAIL");

        // Add your application code here...
        break;
      }

      default:
        ASSERT(false);
        break;
    }
  }
}

static void EventHandlerApp(void)
{
  DPRINT("Got event!\r\n");

  uint8_t event;

  while (xQueueReceive(m_AppEventQueue, (uint8_t*)(&event), 0) == pdTRUE)
  {
    DPRINTF("Event: %d\r\n", event);
    AppStateManager((EVENT_APP)event);
  }
}

/*
 * See description for function prototype in ZW_basis_api.h.
 */
ZW_APPLICATION_STATUS
ApplicationInit(EResetReason_t eResetReason)
{
  // NULL - We dont have the Application Task handle yet
  AppTimerInit(EAPPLICATIONEVENT_TIMER, NULL);

  g_eResetReason = eResetReason;

  /* hardware initialization */
  Board_Init();
  BRD420xBoardInit(RadioConfig.eRegion);

#ifdef DEBUGPRINT
  ZAF_UART0_enable(115200, true, false);
  DebugPrintConfig(m_aDebugPrintBuffer, sizeof(m_aDebugPrintBuffer), ZAF_UART0_tx_send);
#endif

  /* Init state machine*/
  currentState = STATE_APP_STARTUP;

  DPRINT("\n\n------------------------------\n");
  DPRINT("Z-Wave Sample App: Power Strip\n");
  DPRINTF("SDK: %d.%d.%d ZAF: %d.%d.%d.%d [Freq: %d]\n",
          SDK_VERSION_MAJOR,
          SDK_VERSION_MINOR,
          SDK_VERSION_PATCH,
          ZAF_GetAppVersionMajor(),
          ZAF_GetAppVersionMinor(),
          ZAF_GetAppVersionPatchLevel(),
          ZAF_BUILD_NO,
          RadioConfig.eRegion);
  DPRINT("------------------------------\n");
  DPRINTF("%s: Outlet 1 toggle on/off\n", Board_GetButtonLabel(OUTLET1_TOGGLE_BTN));
  DPRINTF("%s: Outlet 2 dim/on/off\n", Board_GetButtonLabel(OUTLET2_DIMMER_BTN));
  DPRINT("      Hold: Dim up/down\n");
  DPRINTF("%s: Toggle notification every 30 sec\n", Board_GetButtonLabel(NOTIFICATION_TOGGLE_BTN));
  DPRINTF("%s: Toggle learn mode\n", Board_GetButtonLabel(APP_BUTTON_LEARN_RESET));
  DPRINT("      Hold 5 sec: Reset\n");
  DPRINTF("%s: Learn mode + identify\n", Board_GetLedLabel(APP_LED_INDICATOR));
  DPRINTF("%s: Outlet 1 status on/off\n", Board_GetLedLabel(OUTLET1_STATUS_LED));
  DPRINTF("%s: Outlet 2 level/intensity\n", Board_GetLedLabel(OUTLET2_LEVEL_LED_R));
  DPRINT("------------------------------\n\n");

  DPRINTF("ApplicationInit eResetReason = %d\n", g_eResetReason);

  CC_ZWavePlusInfo_Init(&CCZWavePlusInfo);

  MultilevelSwitchInit(1, &multiLevelEndpointIDList);
  SetSwitchHwLevel(0x00, ENDPOINT_2);
  powerStrip.dimmer.level = 0;
  powerStrip.dimmer.switchStatus = SWITCH_OFF | SWITCH_DIM_UP;
  powerStrip.binSwitch.switchStatus = SWITCH_OFF;
  
  CC_Version_SetApplicationVersionInfo(ZAF_GetAppVersionMajor(),
                                       ZAF_GetAppVersionMinor(),
                                       ZAF_GetAppVersionPatchLevel(),
                                       ZAF_BUILD_NO);

  Transport_AddEndpointSupport( &endPointFunctionality, endpointsNIF, NUMBER_OF_ENDPOINTS);

  /* Register task function */
  bool bWasTaskCreated = ZW_ApplicationRegisterTask(
                                                    ApplicationTask,
                                                    EAPPLICATIONEVENT_ZWRX,
                                                    EAPPLICATIONEVENT_ZWCOMMANDSTATUS,
                                                    &ProtocolConfig
                                                    );
  ASSERT(bWasTaskCreated);

  return(APPLICATION_RUNNING);
}

/**
 * A pointer to this function is passed to ZW_ApplicationRegisterTask() making it the FreeRTOS
 * application task.
 */
static void
ApplicationTask(SApplicationHandles* pAppHandles)
{
  // Init
  DPRINT("Enabling watchdog\n");
  WDOGn_Enable(DEFAULT_WDOG, true);

  g_AppTaskHandle = xTaskGetCurrentTaskHandle();
  g_pAppHandles = pAppHandles;
  AppTimerSetReceiverTask(g_AppTaskHandle);
  AppTimerRegister(&NotificationTimer, true, ZCB_NotificationTimerCallback);
  AppTimerRegister(&ZAF_TSE_dimming_ActivationTimer, false, ZAF_TSE_DimmingTimerCallback);
  AppTimerRegister(&SupervisionTimer, false, ZCB_SupervisionTimerCallback);

  ZAF_Init(g_AppTaskHandle, pAppHandles, &ProtocolConfig, NULL);

  // Initialize Queue Notifier for events in the application.
  m_AppEventQueue = xQueueCreateStatic(
    sizeof_array(eventQueueStorage),
    sizeof(eventQueueStorage[0]),
    (uint8_t*)eventQueueStorage,
    &m_AppEventQueueObject
  );

  QueueNotifyingInit(
      &m_AppEventNotifyingQueue,
      m_AppEventQueue,
      g_AppTaskHandle,
      EAPPLICATIONEVENT_APP);

  ZAF_EventHelperInit(&m_AppEventNotifyingQueue);

  ZAF_JobHelperInit();
  ZAF_EventHelperEventEnqueue(EVENT_APP_INIT);

  Board_RgbLedInitPwmTimer(LEVEL_MAX);

  Board_RgbLedEnablePwm(OUTLET2_LEVEL_LED_R);
  Board_RgbLedEnablePwm(OUTLET2_LEVEL_LED_G);
  Board_RgbLedEnablePwm(OUTLET2_LEVEL_LED_B);

  Board_EnableButton(APP_BUTTON_LEARN_RESET);
  Board_EnableButton(OUTLET1_TOGGLE_BTN);
  Board_EnableButton(OUTLET2_DIMMER_BTN);
  Board_EnableButton(NOTIFICATION_TOGGLE_BTN);

  Board_IndicatorInit(APP_LED_INDICATOR);
  Board_IndicateStatus(BOARD_STATUS_IDLE);

  CommandClassSupervisionInit(
      CC_SUPERVISION_STATUS_UPDATES_NOT_SUPPORTED,
      ZCB_CommandClassSupervisionGetReceived,
      NULL);

  EventDistributorConfig(
    &g_EventDistributor,
    sizeof_array(g_aEventHandlerTable),
    g_aEventHandlerTable,
    NULL
    );

  // Wait for and process events
  DPRINT("PowerStrip Event processor Started\r\n");
  uint32_t iMaxTaskSleep = 5;
  for (;;)
  {
    EventDistributorDistribute(&g_EventDistributor, iMaxTaskSleep, 0);
  }
}

/*
 * See description for function prototype in ZW_TransportEndpoint.h.
 */
received_frame_status_t
Transport_ApplicationCommandHandlerEx(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength)
{
  received_frame_status_t frame_status = RECEIVED_FRAME_STATUS_NO_SUPPORT;
  /* Call command class handlers */
  switch (pCmd->ZW_Common.cmdClass)
  {
    case COMMAND_CLASS_VERSION:
      frame_status = handleCommandClassVersion(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_ASSOCIATION_GRP_INFO:
      frame_status = CC_AGI_handler( rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_ASSOCIATION:
			frame_status = handleCommandClassAssociation(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_INDICATOR:
      frame_status = handleCommandClassIndicator(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_POWERLEVEL:
      frame_status = handleCommandClassPowerLevel(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_MANUFACTURER_SPECIFIC:
      frame_status = handleCommandClassManufacturerSpecific(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_ZWAVEPLUS_INFO:
      frame_status = handleCommandClassZWavePlusInfo(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_BASIC:
      frame_status = CC_Basic_handler(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_SWITCH_BINARY_V2:
      frame_status = handleCommandClassBinarySwitch(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_NOTIFICATION_V3:
      frame_status = handleCommandClassNotification(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_SWITCH_MULTILEVEL:
      frame_status = handleCommandClassMultilevelSwitch(rxOpt,pCmd, cmdLength);
      break;

    case COMMAND_CLASS_SUPERVISION:
      frame_status = handleCommandClassSupervision(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2:
      frame_status = handleCommandClassMultiChannelAssociation(rxOpt, pCmd, cmdLength);
      break;
    case COMMAND_CLASS_SECURITY:
    case COMMAND_CLASS_SECURITY_2:
      frame_status = handleCommandClassSecurity(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_FIRMWARE_UPDATE_MD_V5:
      frame_status = handleCommandClassFWUpdate(rxOpt, pCmd, cmdLength);
    break;

    case COMMAND_CLASS_MULTI_CHANNEL_V4:
      frame_status = MultiChanCommandHandler(rxOpt, pCmd, cmdLength);
      break;
  }
  return frame_status;
}

/*

 * See description for function prototype in misc.h.
 */
uint8_t
GetMyNodeID(void)
{
  return g_pAppHandles->pNetworkInfo->NodeId;
}

static void
LearnCompleted(void)
{
  uint8_t bNodeID = GetMyNodeID();

  /*If bNodeID= 0xff.. learn mode failed*/
  if(NODE_BROADCAST != bNodeID)
  {
    /* Success */
    if (0 == bNodeID)
    {
      SetDefaultConfiguration();
      if(ESWTIMER_STATUS_FAILED != notificationOverLoadTimerStatus)
      {
        TimerStop(&NotificationTimer);
        notificationOverLoadTimerStatus = ESWTIMER_STATUS_FAILED;
      }
    }
  }
  ZAF_EventHelperEventEnqueue((EVENT_APP) EVENT_SYSTEM_LEARNMODE_FINISHED);
  Transport_OnLearnCompleted(bNodeID);
}

/**
 * @brief Returns the current state of the application state machine.
 * @return Current state
 */
STATE_APP
GetAppState(void)
{
  return currentState;
}

/**
 * @brief The core state machine of this sample application.
 * @param event The event that triggered the call of AppStateManager.
 */
void
AppStateManager(EVENT_APP event)
{
  DPRINTF("AppStateManager St: %d, Ev: %d\r\n", currentState, event);

  if ((BTN_EVENT_LONG_PRESS(APP_BUTTON_LEARN_RESET) == (BUTTON_EVENT)event) ||
      (EVENT_SYSTEM_RESET == (EVENT_SYSTEM)event))
  {
    /*Force state change to activate system-reset without taking care of current state.*/
    ChangeState(STATE_APP_RESET);
    /* Send reset notification*/
    DeviceResetLocally();
  }

  switch(currentState)
  {
    case STATE_APP_STARTUP:

      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
      }

      if(EVENT_APP_INIT == event)
      {
        /* Load the application settings from NVM3 file system */
        bool filesExist = LoadConfiguration();

        /* Initialize association module */
        AssociationInitEndpointSupport(false,
                                       rootGroupMapping,
                                       sizeof(rootGroupMapping)/sizeof(ASSOCIATION_ROOT_GROUP_MAPPING),
                                       pFileSystemApplication);

        // Initialize AGI and set up groups.
        AGI_Init();
        // Root device
        CC_AGI_LifeLineGroupSetup(agiTableLifeLine,
                                  sizeof_array(agiTableLifeLine),
                                  ENDPOINT_ROOT);
        // Endpoint 1
        CC_AGI_LifeLineGroupSetup(agiTableLifeLineEP1,
                                  sizeof_array(agiTableLifeLineEP1),
                                  ENDPOINT_1);
        // Endpoint 2
        CC_AGI_LifeLineGroupSetup(agiTableLifeLineEP2,
                                  sizeof_array(agiTableLifeLineEP2),
                                  ENDPOINT_2);

        AGI_ResourceGroupSetup(agiTableRootDeviceGroups,
                               sizeof_array(agiTableRootDeviceGroups),
                               ENDPOINT_ROOT);
        // Endpoint 1
        AGI_ResourceGroupSetup(agiTableEndpoint1Groups,
                               sizeof_array(agiTableEndpoint1Groups),
                               ENDPOINT_1);

        // Endpoint 2
        AGI_ResourceGroupSetup(agiTableEndpoint2Groups,
                               sizeof_array(agiTableEndpoint2Groups),
                               ENDPOINT_2);

        InitNotification(pFileSystemApplication);
        {
          AddNotification(ENDPOINT_PROFILE,
                          NOTIFICATION_TYPE_POWER_MANAGEMENT,
                          &supportedEvents,
                          1,
                          false,
                          ENDPOINT_1,
                          NOTIFICATION_STATUS_UNSOLICIT_ACTIVATED,
                          filesExist);

          AddNotification(ENDPOINT_PROFILE,
                          NOTIFICATION_TYPE_POWER_MANAGEMENT,
                          &supportedEvents,
                          1,
                          false,
                          ENDPOINT_2,
                          NOTIFICATION_STATUS_UNSOLICIT_ACTIVATED,
                          filesExist);
        }

        /*
         * Initialize Event Scheduler.
         */
        Transport_OnApplicationInitSW( &m_AppNIF, NULL);

        /* Enter SmartStart*/
        /* Protocol will commence SmartStart only if the node is NOT already included in the network */
        ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART, g_eResetReason);

        /* Init state machine*/
        ZAF_EventHelperEventEnqueue(EVENT_EMPTY);

        break;
      }
      CC_FirmwareUpdate_Init(NULL, NULL, NULL, true);
      ChangeState(STATE_APP_IDLE);
      break;

    case STATE_APP_IDLE:

      if (EVENT_APP_REFRESH_MMI == event)
      {
        Board_IndicateStatus(BOARD_STATUS_IDLE);
      }

      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        LoadConfiguration();
      }

      if(EVENT_APP_SMARTSTART_IN_PROGRESS == event)
      {
        ChangeState(STATE_APP_LEARN_MODE);
      }

      if ((BTN_EVENT_SHORT_PRESS(APP_BUTTON_LEARN_RESET) == (BUTTON_EVENT)event) ||
          (EVENT_SYSTEM_LEARNMODE_START == (EVENT_SYSTEM)event))
      {
        DPRINT("APP_BUTTON_LEARN_RESET SHORT_PRESS\n");

        if (EINCLUSIONSTATE_EXCLUDED != g_pAppHandles->pNetworkInfo->eInclusionState)
        {
          DPRINT("LEARN_MODE_EXCLUSION\n");
          ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_EXCLUSION_NWE, g_eResetReason);
        }
        else
        {
          DPRINT("LEARN_MODE_INCLUSION\n");
          ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION, g_eResetReason);
        }
        ChangeState(STATE_APP_LEARN_MODE);
      }

      if ((BTN_EVENT_SHORT_PRESS(OUTLET1_TOGGLE_BTN) == (BUTTON_EVENT)event) ||
          (BTN_EVENT_UP(OUTLET1_TOGGLE_BTN) == (BUTTON_EVENT)event))
      {
        DPRINT("+Toggle SW 1");
        ToggleSwitch(BIN_SWITCH_1);
      }

      if (BTN_EVENT_SHORT_PRESS(OUTLET2_DIMMER_BTN) == (BUTTON_EVENT)event)
      {
        DPRINT("+Stop DM 1");
        StopSwitchDimming(ENDPOINT_2);
        powerStrip.dimmer.switchStatus &= ~SWITCH_IS_DIMMING; // Clear dim bit

        DPRINT("+Toggle DM 1");
        ToggleSwitch(MULTILEVEL_SWITCH_1);
      }

      if (BTN_EVENT_UP(OUTLET2_DIMMER_BTN) == (BUTTON_EVENT)event)
      {
   	    DPRINT("+Stop DM 1");
        StopSwitchDimming(ENDPOINT_2);

        if (powerStrip.dimmer.switchStatus & SWITCH_IS_DIMMING)
        {
          DPRINT("+Stop DM 1");
          powerStrip.dimmer.switchStatus &= ~SWITCH_IS_DIMMING; // Clear dim bit
          powerStrip.dimmer.switchStatus ^= SWITCH_DIM_UP;      // Toggle dim up/down
        }

        /* Stop timer if any*/
        TimerStop(&ZAF_TSE_dimming_ActivationTimer);
        /* Tell the lifeline destination that EP2 state has been modified */
        void* pDataEp2 = CC_MultilevelSwitch_prepare_zaf_tse_data(&zaf_tse_local_ep2_actuation);
        ZAF_TSE_Trigger((void *)CC_MultilevelSwitch_report_stx, pDataEp2, true);
      }

      if (BTN_EVENT_HOLD(OUTLET2_DIMMER_BTN) == (BUTTON_EVENT)event)
      {
        DPRINT("+DM 1 hold ");
        powerStrip.dimmer.switchStatus |= SWITCH_IS_DIMMING;
        if (powerStrip.dimmer.switchStatus & SWITCH_DIM_UP)
        {
          CC_MultilevelSwitch_SetValue(LEVEL_MAX, 2, ENDPOINT_2);
        }
        else
        {
          CC_MultilevelSwitch_SetValue(0, 3, ENDPOINT_2);
        }
      }

      if ((BTN_EVENT_SHORT_PRESS(NOTIFICATION_TOGGLE_BTN) == (BUTTON_EVENT)event) ||
          (BTN_EVENT_UP(NOTIFICATION_TOGGLE_BTN) == (BUTTON_EVENT)event))
      {
        /*
         * Pressing the NOTIFICATION_TOGGLE_BTN key will toggle the overload timer.
         * This timer will transmit a notification every 30th second.
         */
        notificationToggle();
      }
      break;

    case STATE_APP_LEARN_MODE:
      if(EVENT_APP_REFRESH_MMI == event)
      {
        Board_IndicateStatus(BOARD_STATUS_LEARNMODE_ACTIVE);
      }

      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        LoadConfiguration();
      }

      if ((BTN_EVENT_SHORT_PRESS(APP_BUTTON_LEARN_RESET) == (BUTTON_EVENT)event) ||
          (EVENT_SYSTEM_LEARNMODE_STOP == (EVENT_SYSTEM)event))
      {
        ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_DISABLE, g_eResetReason);

        //Go back to smart start if the node was never included.
        //Protocol will not commence SmartStart if the node is already included in the network.
        ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART, g_eResetReason);

        Board_IndicateStatus(BOARD_STATUS_IDLE);
        ChangeState(STATE_APP_IDLE);

        /* If we are in a network and the Identify LED state was changed to idle due to learn mode, report it to lifeline */
        CC_Indicator_RefreshIndicatorProperties();
        ZAF_TSE_Trigger((void *)CC_Indicator_report_stx, &ZAF_TSE_localActuationIdentifyData, true);
      }

      if(EVENT_SYSTEM_LEARNMODE_FINISHED == (EVENT_SYSTEM)event)
      {
        //Go back to smart start if the node was excluded.
        //Protocol will not commence SmartStart if the node is already included in the network.
        ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART, g_eResetReason);

        ChangeState(STATE_APP_IDLE);

        /* If we are in a network and the Identify LED state was changed to idle due to learn mode, report it to lifeline */
        CC_Indicator_RefreshIndicatorProperties();
        ZAF_TSE_Trigger((void *)CC_Indicator_report_stx, &ZAF_TSE_localActuationIdentifyData, true);
      }
      break;

    case STATE_APP_RESET:
      if(EVENT_APP_REFRESH_MMI == event){}

	  if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        /* Soft reset */
        Board_ResetHandler();
      }
      break;

    case STATE_APP_TRANSMIT_DATA:

      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        LoadConfiguration();
      }

      if (EVENT_APP_FINISH_EVENT_JOB == event)
      {
        ChangeState(STATE_APP_IDLE);
      }

      if (BTN_EVENT_SHORT_PRESS(NOTIFICATION_TOGGLE_BTN) == (BUTTON_EVENT)event)
      {
        /*
         * Short press on notification key will toggle the overload timer.
         * This timer will transmit a notification every 30th second.
         */
        notificationToggle();
      }
      break;
  }
}

/**
 * @brief Sets the current state to a new, given state.
 * @param newState New state.
 */
static void
ChangeState(STATE_APP newState)
{
  currentState = newState;
  /**< Pre-action on new state is to refresh MMI */
  ZAF_EventHelperEventEnqueue(EVENT_APP_REFRESH_MMI);
}

/**
 * @brief Transmission callback for Device Reset Locally call.
 * @param pTransmissionResult Result of each transmission.
 */
void
DeviceResetLocallyDone(TRANSMISSION_RESULT * pTransmissionResult)
{
  if (TRANSMISSION_RESULT_FINISHED == pTransmissionResult->isFinished)
  {
    /* Reset protocol */
    // Set default command to protocol
    SZwaveCommandPackage CommandPackage;
    CommandPackage.eCommandType = EZWAVECOMMANDTYPE_SET_DEFAULT;

    DPRINT("\nDisabling watchdog during reset\n");
    WDOGn_Enable(DEFAULT_WDOG, false);

    EQueueNotifyingStatus Status = QueueNotifyingSendToBack(g_pAppHandles->pZwCommandQueue, (uint8_t*)&CommandPackage, 500);
    ASSERT(EQUEUENOTIFYING_STATUS_SUCCESS == Status);
  }
}

/**
 * @brief Send reset notification.
 */
void
DeviceResetLocally(void)
{
  DPRINT("Call locally reset\r\n");

  CC_DeviceResetLocally_notification_tx(&lifelineProfile, DeviceResetLocallyDone);
}

/*
 * See description for function prototype in CC_Version.h.
 */
uint8_t handleNbrFirmwareVersions()
{
  return 1; /*CHANGE THIS - firmware 0 version*/
}

/*
 * See description for function prototype in CC_Version.h.
 */
void
handleGetFirmwareVersion(
  uint8_t bFirmwareNumber,
  VG_VERSION_REPORT_V2_VG *pVariantgroup)
{
  /*firmware 0 version and sub version*/
  if(bFirmwareNumber == 0)
  {
    pVariantgroup->firmwareVersion = ZAF_GetAppVersionMajor();
    pVariantgroup->firmwareSubVersion = ZAF_GetAppVersionMinor();
  }
  else
  {
    /*Just set it to 0 if firmware n is not present*/
    pVariantgroup->firmwareVersion = 0;
    pVariantgroup->firmwareSubVersion = 0;
  }
}

/**
 * @brief Function resets configuration to default values.
 */
void
SetDefaultConfiguration(void)
{
  AssociationInit(true, pFileSystemApplication);

  DefaultNotificationStatus(NOTIFICATION_STATUS_UNSOLICIT_ACTIVATED);

  uint32_t appVersion = ZAF_GetAppVersion();
  Ecode_t errCode = nvm3_writeData(pFileSystemApplication, ZAF_FILE_ID_APP_VERSION, &appVersion, ZAF_FILE_SIZE_APP_VERSION);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code if this error can only be caused by some internal flash HW failure
}

/**
 * @brief This function loads the application settings from non-volatile memory.
 * If no settings are found, default values are used and saved.
 */
bool
LoadConfiguration(void)
{
  // Init file system
  ApplicationFileSystemInit(&pFileSystemApplication);

  uint32_t appVersion;
  Ecode_t versionFileStatus = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_APP_VERSION, &appVersion, ZAF_FILE_SIZE_APP_VERSION);

  if (ECODE_NVM3_OK == versionFileStatus)
  {
    if (ZAF_GetAppVersion() != appVersion)
    {
      // Add code for migration of file system to higher version here.
    }

    /* Initialize association module */
    AssociationInit(false, pFileSystemApplication);

    /* Initialize PowerLevel functionality*/
    loadStatusPowerLevel();
    return true;
  }
  else
  {
    DPRINT("Application FileSystem Verify failed\r\n");
    loadInitStatusPowerLevel();

    // Reset the file system if ZAF_FILE_ID_APP_VERSION is missing since this indicates
    // corrupt or missing file system.
    AppResetNvm();
    return false;
  }

}

void AppResetNvm(void)
{
  DPRINT("Resetting application FileSystem to default\r\n");

  ASSERT(0 != pFileSystemApplication); //Assert has been kept for debugging , can be removed from production code. This error can only be caused by some internal flash HW failure

  Ecode_t errCode = nvm3_eraseAll(pFileSystemApplication);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code. This error can only be caused by some internal flash HW failure

  /* Initialize transport layer */
  Transport_SetDefault();

  /* Apparently there is no valid configuration in the NVM3 file system, so load */
  /* default values and save them. */
  SetDefaultConfiguration();
}

/*
 * This function will be invoked when a Basic Set command is received.
 *
 * See prototype for more information.
 */
e_cmd_handler_return_code_t CC_Basic_Set_handler(uint8_t val, uint8_t endpoint)
{
  if (ENDPOINT_2 == endpoint)
  {
    return CC_MultilevelSwitch_SetValue(val, 1, endpoint);
  }
  else
  {
    return CommandClassBinarySwitchSupportSet(val, 0, endpoint);
  }
}

/*
 * This function will be invoked when a Basic Get command is received.
 *
 * See prototype for more information.
 */
uint8_t CC_Basic_GetCurrentValue_handler(uint8_t endpoint)
{
  if (ENDPOINT_2 == endpoint)
  {
    return CC_MultilevelSwitch_GetCurrentValue_handler(endpoint);
  }
  return appBinarySwitchGetCurrentValue(endpoint);
}

/*
 * This function will be invoked when a Basic Get command is received.
 *
 * See prototype for more information.
 */
uint8_t CC_Basic_GetTargetValue_handler(uint8_t endpoint)
{
  if (ENDPOINT_2 == endpoint)
  {
    return GetTargetLevel(endpoint);
  }
  return appBinarySwitchGetTargetValue(endpoint);
}

/*
 * This function will be invoked when a Basic Get command is received.
 *
 * See prototype for more information.
 */
uint8_t CC_Basic_GetDuration_handler(uint8_t endpoint)
{
  if (ENDPOINT_2 == endpoint)
  {
    return GetCurrentDuration(endpoint);
  }
  return appBinarySwitchGetDuration(endpoint);
}

/*
 * This function will be invoked when a Binary Switch Get command is received by the root
 * device or by the endpoint that supports the Binary Switch CC.
 *
 * See prototype for more information.
 */
CMD_CLASS_BIN_SW_VAL appBinarySwitchGetCurrentValue(uint8_t endpoint)
{
  UNUSED(endpoint);

  if (powerStrip.binSwitch.switchStatus & SWITCH_ON)
  {
    return CMD_CLASS_BIN_ON;
  }
  else
  {
    return CMD_CLASS_BIN_OFF;
  }
}

/*
 * This function will be invoked when a Binary Switch Get command is received by the root
 * device or by the endpoint that supports the Binary Switch CC.
 *
 * See prototype for more information.
 */
CMD_CLASS_BIN_SW_VAL appBinarySwitchGetTargetValue(uint8_t endpoint)
{
  /* Always report target value equal to current value  */
  return appBinarySwitchGetCurrentValue(endpoint);
}

/*
 * This function will be invoked when a Binary Switch Get command is received by the root
 * device or by the endpoint that supports the Binary Switch CC.
 *
 * See prototype for more information.
 */
uint8_t appBinarySwitchGetDuration(uint8_t endpoint)
{
  UNUSED(endpoint);

  /* Always report instant transition; the only reasonable transition for a binary switch. */
  return 0;
}

/*
 * This function will be invoked when a Binary Switch Set command is received by the root
 * device or by the endpoint that supports the Binary Switch CC.
 *
 * See prototype for more information.
 */
e_cmd_handler_return_code_t appBinarySwitchSet(
  CMD_CLASS_BIN_SW_VAL val,
  uint8_t duration,
  uint8_t endpoint
)
{
  UNUSED(duration); /* Ignore duration - for a binary switch only instant transitions make sense */
  UNUSED(endpoint);

  DPRINTF("\r\nApp: handleApplBinarySwitchSet %u %u\r\n", endpoint, val);

  if(CMD_CLASS_BIN_OFF == val)
  {
    powerStrip.binSwitch.switchStatus &= ~SWITCH_ON;
    Board_SetLed(OUTLET1_STATUS_LED, LED_OFF);
  }
  else if(CMD_CLASS_BIN_ON == val)
  {
    powerStrip.binSwitch.switchStatus |= SWITCH_ON;
    Board_SetLed(OUTLET1_STATUS_LED, LED_ON);
  }

  return E_CMD_HANDLER_RETURN_CODE_HANDLED;
}

/*
 * See prototype.
 */
uint8_t appBinarySwitchGetFactoryDefaultDuration(uint8_t endpoint)
{
  UNUSED(endpoint);
  return 0;
}

/*
 * This function will be invoked when a Multilevel Switch Supported Get command is received by the
 * root device or by the endpoint that supports the Multilevel CC.
 *
 * See prototype for more information.
 */
MULTILEVEL_SWITCH_TYPE CommandClassMultilevelSwitchPrimaryTypeGet(uint8_t endpoint)
{
  UNUSED(endpoint);
  return MULTILEVEL_SWITCH_DOWN_UP;
}

/*
 * This function will be invoked when a Multilevel Switch Get command is received by the root
 * device or by the endpoint that supports the Multilevel CC.
 *
 * See prototype for more information.
 */
uint8_t CC_MultilevelSwitch_GetCurrentValue_handler(uint8_t endpoint)
{
  /*we have only one mutlilevel switch so we map root device endpoint to the multilevel switch*/
  if( ENDPOINT_2 == endpoint || 0 == endpoint)
  {
    if (powerStrip.dimmer.switchStatus & SWITCH_ON)
    {
      return powerStrip.dimmer.level;
    }
  }
  return 0;
}

/*
 * This function will be invoked when a Multilevel Switch Set command is received by the root
 * device or by the endpoint that supports the Multilevel CC.
 *
 * See prototype for more information.
 */
void CommandClassMultilevelSwitchSet(uint8_t bLevel, uint8_t endpoint)
{
  powerStrip.dimmer.level = bLevel;
  UpdateSwitchLevel(MULTILEVEL_SWITCH_1, powerStrip.dimmer.level);
}

/*
 * See prototype for info
 */
uint8_t GetFactoryDefaultDimmingDuration(bool boIsSetCmd, uint8_t endpoint )
{
  UNUSED(endpoint);
  /*we have only one mutlilevel switch so we ignore the endpoint parmeter*/
  return (boIsSetCmd ? 1 : 10);
}

void
ZCB_JobStatus(TRANSMISSION_RESULT * pTransmissionResult)
{
  DPRINTF("\r\nTX CB for N %u", pTransmissionResult->nodeId);

  if (TRANSMISSION_RESULT_FINISHED == pTransmissionResult->isFinished)
  {
    ZAF_EventHelperEventEnqueue(EVENT_APP_FINISH_EVENT_JOB);
  }
}

/**
 * Prepare the data input for the TSE for Notification events.
*/
void* CC_Notification_prepare_zaf_tse_data(notification_type_t type, uint8_t event, uint8_t endpoint)
{
  static RECEIVE_OPTIONS_TYPE_EX pRxOpt = {
      .rxStatus = 0,        /* rxStatus, verified by the TSE for Multicast */
      .securityKey = 0,     /* securityKey, ignored by the TSE */
      .sourceNode = {0,0},  /* sourceNode (nodeId, endpoint), verified against lifeline destinations by the TSE */
      .destNode = {0,0}     /* destNode (nodeId, endpoint), verified by the TSE for local endpoint */
  };

  /* Sanity check the endpoint value is valid */
  if (endpoint > NUMBER_OF_ENDPOINTS)
  {
    return NULL;
  }

  /* Specify which endpoint triggered this state change  */
  pRxOpt.destNode.endpoint = endpoint;

  /* And store the data and return the pointer */
  ZAF_TSE_NotificationData.rxOptions = pRxOpt;
  ZAF_TSE_NotificationData.notificationType = type;
  ZAF_TSE_NotificationData.notificationEvent = event;
  ZAF_TSE_NotificationData.sourceEndpoint = endpoint;

  return &ZAF_TSE_NotificationData;
}

/**
 * @brief Called every time the notification timer triggers.
 */
void
ZCB_NotificationTimerCallback(SSwTimer *pTimer)
{
  UNUSED(pTimer);

  JOB_STATUS jobStatus;
  void       *pData;

  DPRINT("\r\nNtfctn timer");

  if (false == notificationOverLoadActiveState)
  {
    /*Find new endpoint to send notification*/
    notificationOverLoadendpoint++;
    if (notificationOverLoadendpoint > NUMBER_OF_ENDPOINTS)
    {
      notificationOverLoadendpoint = 1;
    }

    DPRINTF("\r\nNtfctn enable EP %u", notificationOverLoadendpoint);

    notificationOverLoadActiveState = true;
    NotificationEventTrigger(
        ENDPOINT_PROFILE,
        NOTIFICATION_TYPE_POWER_MANAGEMENT,
        NOTIFICATION_EVENT_POWER_MANAGEMENT_OVERLOADED_DETECTED,
        NULL,
        0,
        notificationOverLoadendpoint);

    /* Tell the lifeline destinations that an Endpoint state has been modified */
    pData = CC_Notification_prepare_zaf_tse_data(NOTIFICATION_TYPE_POWER_MANAGEMENT, NOTIFICATION_EVENT_POWER_MANAGEMENT_OVERLOADED_DETECTED, notificationOverLoadendpoint);
  }
  else
  {
    DPRINTF("\r\nNtfctn disable EP %u", notificationOverLoadendpoint);

    notificationOverLoadActiveState = false;
    NotificationEventTrigger(
        ENDPOINT_PROFILE,
        NOTIFICATION_TYPE_POWER_MANAGEMENT,
        NOTIFICATION_EVENT_POWER_MANAGEMENT_NO_EVENT,
        &supportedEvents,
        1,
        notificationOverLoadendpoint);

    /* Tell the lifeline destinations that an Endpoint state has been modified */
    pData = CC_Notification_prepare_zaf_tse_data(NOTIFICATION_TYPE_POWER_MANAGEMENT, NOTIFICATION_EVENT_POWER_MANAGEMENT_NO_EVENT, notificationOverLoadendpoint);
  }

  //@ [NOTIFICATION_TRANSMIT]
  jobStatus = UnsolicitedNotificationAction(
      ENDPOINT_PROFILE,
      notificationOverLoadendpoint,
      ZCB_JobStatus);
  //@ [NOTIFICATION_TRANSMIT]

  if (JOB_STATUS_SUCCESS != jobStatus)
  {
    TRANSMISSION_RESULT transmissionResult;

    DPRINTF("\r\nX%u", jobStatus);

    transmissionResult.status = false;
    transmissionResult.nodeId = 0;
    transmissionResult.isFinished = TRANSMISSION_RESULT_FINISHED;

    ZCB_JobStatus(&transmissionResult);
  }

  /* Trigger TSE */
  if (pData)
  {
    ZAF_TSE_Trigger((void *)CC_Notification_report_stx, pData, true);
  }

  ChangeState(STATE_APP_TRANSMIT_DATA);
}

static void UpdateSwitchLevel(uint8_t switchID, uint8_t level)
{
  UNUSED(switchID);
  Board_RgbLedSetPwm(OUTLET2_LEVEL_LED_R, level);
  Board_RgbLedSetPwm(OUTLET2_LEVEL_LED_G, level);
  Board_RgbLedSetPwm(OUTLET2_LEVEL_LED_B, level);

  if (0 == level)
  {
    powerStrip.dimmer.switchStatus &= ~SWITCH_ON;
    DPRINT("Dimmer OFF\r\n");
  }
  else
  {
    powerStrip.dimmer.switchStatus |= SWITCH_ON;
    DPRINTF("Dimmer level: %u\r\n", level);
  }
}

static void ToggleSwitch(uint8_t switchID)
{
  switch (switchID)
  {
    case BIN_SWITCH_1:
      if (powerStrip.binSwitch.switchStatus & SWITCH_ON)
      {
        DPRINT(" OFF\n"); /* Continuation of DPRINT in AppStateManager() */
        powerStrip.binSwitch.switchStatus &= ~SWITCH_ON;
        Board_SetLed(OUTLET1_STATUS_LED, LED_OFF);
      }
      else
      {
        DPRINT(" ON\n"); /* Continuation of DPRINT in AppStateManager() */
        powerStrip.binSwitch.switchStatus |= SWITCH_ON;
        Board_SetLed(OUTLET1_STATUS_LED, LED_ON);
      }
      /* Tell the lifeline destination that EP1 state has been modified */
      void* pDataEp1 = CC_BinarySwitch_prepare_zaf_tse_data(&zaf_tse_local_ep1_actuation);
      ZAF_TSE_Trigger((void *)CC_BinarySwitch_report_stx, pDataEp1, true);

      break;
    case MULTILEVEL_SWITCH_1:
      DPRINTF(" %s %u\n", (powerStrip.dimmer.switchStatus & SWITCH_ON) ? "ON" : "OFF", powerStrip.dimmer.level);
      /*stop the dimmer first if it is running*/
      StopSwitchDimming(ENDPOINT_2);
      if (powerStrip.dimmer.switchStatus & SWITCH_ON)
      {
        /*turn off*/
        UpdateSwitchLevel(switchID, 0);
        powerStrip.dimmer.switchStatus |= SWITCH_DIM_UP;
      }
      else
      {
        /*turn on*/
        /*go back to the last level or turn it full on*/
        if (powerStrip.dimmer.level == 0)
        {
          powerStrip.dimmer.level = GetOnStateLevel(ENDPOINT_2);
          if(powerStrip.dimmer.level == 0)
          {
            powerStrip.dimmer.level = LEVEL_MAX;
          }
        }

        if (powerStrip.dimmer.level == LEVEL_MAX)
        {
        	powerStrip.dimmer.switchStatus &= ~SWITCH_DIM_UP;
        }

        UpdateSwitchLevel(switchID, powerStrip.dimmer.level);
      }
      /* Stop timer if any*/
      TimerStop(&ZAF_TSE_dimming_ActivationTimer);
      /* Tell the lifeline destination that EP2 state has been modified */
      void* pDataEp2 = CC_MultilevelSwitch_prepare_zaf_tse_data(&zaf_tse_local_ep2_actuation);
      ZAF_TSE_Trigger((void *)CC_MultilevelSwitch_report_stx, pDataEp2, true);

      break;
    default:
      /*this is expection*/
      break;
  }
}

/**
 * @brief Toggles the notification timer.
 */
static void notificationToggle(void)
{
  DPRINT("\r\nNtfctn toggle");


  if (ESWTIMER_STATUS_FAILED == notificationOverLoadTimerStatus)
  {
    DPRINT("\r\nNtfctn start");

    notificationOverLoadActiveState = false;

    notificationOverLoadTimerStatus = TimerStart(&NotificationTimer, 30*1000);

    /*
     * The timer will transmit the first notification in 30 seconds. We
     * call the callback function directly to transmit the first
     * notification right now.
     */
    ZCB_NotificationTimerCallback(NULL);
  }
  else
  {
    DPRINT("\r\nNtfctn stop");
    /* Deactivate overload timer */
    TimerStop(&NotificationTimer);
    notificationOverLoadTimerStatus = ESWTIMER_STATUS_FAILED;
  }
}

/**
 * Handles a received Supervision Get command.
 *
 * The purpose of Supervision is to inform the source node (controller) when the dimming
 * operation is finished.
 * The first Supervision report will be transmitted automatically by the Framework, but transmission
 * of the next report(s) need(s) to be handled by the application. In this scenario there's only
 * one extra Supervision report.
 */
void ZCB_CommandClassSupervisionGetReceived(SUPERVISION_GET_RECEIVED_HANDLER_ARGS * pArgs)
{
  if (SWITCH_MULTILEVEL_SET == pArgs->cmd && COMMAND_CLASS_SWITCH_MULTILEVEL_V4 == pArgs->cmdClass)
  {
    uint32_t duration = GetCurrentDuration(pArgs->rxOpt->destNode.endpoint);
    pArgs->duration = duration;
    if (0 < duration)
    {
      pArgs->status = CC_SUPERVISION_STATUS_WORKING;

      if (CC_SUPERVISION_STATUS_UPDATES_SUPPORTED == CC_SUPERVISION_EXTRACT_STATUS_UPDATE(pArgs->properties1))
      {
        if (TimerIsActive(&SupervisionTimer))
        {
          // The timer is active. It means we have a WORKING session pending from previous Supervision Get command.
          // Complete that session first. By stopping the timer and manually calling the timer callback function.
          TimerStop(&SupervisionTimer);
          ZCB_SupervisionTimerCallback(NULL);
        }

        // Save the data
        rxOptionSupervision = *(pArgs->rxOpt);
        sessionID = CC_SUPERVISION_EXTRACT_SESSION_ID(pArgs->properties1);

        pArgs->properties1 = CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_REPORTS_TO_FOLLOW) | CC_SUPERVISION_ADD_SESSION_ID(sessionID);

        /*
         * Start timer that will send another Supervision report when triggered.
         *
         * Checking whether duration is higher than 127 is done because these values are
         * interpreted as minutes. Please see table 8, SDS13781-4.
         */
        if (127 < duration)
        {
          duration = (duration - 127) * 60; /* minutes */
        }
        TimerStart(&SupervisionTimer, duration * 1000);
      }
    }
    else {
      // Duration == 0. I.e. the command has already been handled and completed. Clear the "More status updates coming" flag
      pArgs->properties1 &= (CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_THIS_IS_LAST) | 0x7F);
    }
  }
  else
  {
    // Status for all other commands
    pArgs->properties1 &= (CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_THIS_IS_LAST) | 0x7F);
    pArgs->duration = 0;
  }
}

/**
 * Transmits a Supervision report.
 *
 * This function is triggered by the timer set in the Supervision Get handler.
 */
void ZCB_SupervisionTimerCallback(SSwTimer *pTimer)
{
  TRANSMIT_OPTIONS_TYPE_SINGLE_EX * pTxOptions;
  RxToTxOptions(&rxOptionSupervision, &pTxOptions);
  CmdClassSupervisionReportSend(
      pTxOptions,
      CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_THIS_IS_LAST) | CC_SUPERVISION_ADD_SESSION_ID(sessionID),
      CC_SUPERVISION_STATUS_SUCCESS,
      0);
}

/**
 * @brief This function triggers the TSE after CC Multilevel Switch Set handler
 * @param pTimer SSwTimer pointer that triggered the callback
 */
void ZAF_TSE_DimmingTimerCallback(SSwTimer *pTimer)
{
  if (&ZAF_TSE_dimming_ActivationTimer != pTimer)
  {
    DPRINTF("%s(): Call with wrong SSwTimer pointer. Ignoring\n\r", __func__);
    return;
  }

  uint8_t endpoint_index = CC_MultilevelSwitch_getEndpointIndex(2);
  if (false == ZAF_TSE_Trigger((void *)CC_MultilevelSwitch_report_stx, &ZAF_TSE_MultilevelSwitchData[endpoint_index], true))
  {
    DPRINTF("%s(): ZAF_TSE_Trigger failed\n", __func__);
  }
  /* Stop timer if any*/
  TimerStop(pTimer);
}

/**
 * This function is used to notify the Application that the CC Multilevel Switch Set is in changing state
 * The application can subsequently make the TSE Trigger using the information passed on from the rxOptions.
 * @param rxOpt pointer used to when triggering the "working state"
 * @param dimmingDuration Dimming timeout. During this period the app is in "working state"
 */
void CC_MultilevelSwitch_report_notifyWorking(RECEIVE_OPTIONS_TYPE_EX *rxOpt, uint8_t dimmingDuration)
{
  // In case when CC Basic is mapped to CC Multilevel Switch, default action is SET, with duration = Default
  CC_MultilevelSwitch_prepare_zaf_tse_data(rxOpt);
  TimerStart(&ZAF_TSE_dimming_ActivationTimer, dimmingDuration ? dimmingDuration * 1000 : 10);
}

/**
 * This function is used to return a pointer for input data for the TSE trigger.
 * @param rxOption pointer that will be included in the ZAF_TSE input data.
*/
void* CC_Basic_prepare_zaf_tse_data(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  /* Basic is mapped to Binary Switch for Root and EP1
  Basic is mapped to multilevel Switch for EP3.
  Call here different functions
  for each endpoint (rxOpt->destNode.endpoint) if they do not map to the same
  Command Class & Command */
  if (0 == pRxOpt->destNode.endpoint || 1 == pRxOpt->destNode.endpoint)
  {
    return CC_BinarySwitch_prepare_zaf_tse_data(pRxOpt);
  }
  else if (2 == pRxOpt->destNode.endpoint)
  {
    return CC_MultilevelSwitch_prepare_zaf_tse_data(pRxOpt);
  }
  else
  {
    /* Basic is not supported, return NULL so the TSE trigger will just be rejected */
    return NULL;
  }
}

/**
 * This function is used to return a pointer for input data for the TSE trigger.
 * @param txOptions pointer that will be included in the ZAF_TSE input data.
 * @param pData App specific data
*/
void CC_Basic_report_stx(TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions, void* pData)
{
  /* Basic Report is mapped to different commands for all end points.
  Call here different functions for each endpoint (pData->rxOpt->destNode.endpoint)
  if they do not map to the same Command Class & Command */
  s_zaf_tse_data_input_template_t* pDataInput = (s_zaf_tse_data_input_template_t*)(pData);
  RECEIVE_OPTIONS_TYPE_EX RxOptions = pDataInput->rxOptions;

  if (0 == RxOptions.destNode.endpoint || 1 == RxOptions.destNode.endpoint)
  {
    CC_BinarySwitch_report_stx(txOptions,pData);
  }
  else if (2 == RxOptions.destNode.endpoint)
  {
    CC_MultilevelSwitch_report_stx(txOptions,pData);
  }
}

/**
 * This function is used to notify the Application that the CC Basic Set
 * status is in a WORKING state. The application can subsequently make the TSE Trigger
 * using the information passed on from the rxOptions.
 * @param rxOption pointer used to when triggering the "working state"
 */
void CC_Basic_report_notifyWorking(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  /* "working state" can happen for endpoint 2 only, when dimming takes some time to finish */
  CC_MultilevelSwitch_report_notifyWorking(pRxOpt, GetFactoryDefaultDimmingDuration(true, 2));
}

/**
 * Considering several endpoints supporting the same Command Class,
 * this function finds the index of the end point if they were stored in an array.
 * e.g. if Endpoints 0, 3, 5 and 12 support the Command Class, their index would
 * be respectively 0, 1, 2 and 3.
 * @param endpoint Indicating the endpoint identifier to search for.
*/
uint8_t CC_BinarySwitch_getEndpointIndex(uint8_t endpoint)
{
  for (uint8_t i=0; i<sizeof(endpoints_Supporting_BinarySwitch_CC); i++)
  {
    if (endpoint == endpoints_Supporting_BinarySwitch_CC[i]){
      return i;
    }
  }
  /*End point was not found, this should not happen but in this case, just return the first End Point */
  return 0;
}

/**
 * Prepare the data input for the TSE for any Binary Switch CC command based on the pRxOption pointer.
 * @param pRxOpt pointer used to indicate how the frame was received (if any) and what endpoints are affected
*/
void* CC_BinarySwitch_prepare_zaf_tse_data(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  uint8_t endpoint_index = CC_BinarySwitch_getEndpointIndex(pRxOpt->destNode.endpoint);
  memset(&ZAF_TSE_BinarySwitchData[endpoint_index], 0, sizeof(s_CC_binarySwitch_data_t));
  ZAF_TSE_BinarySwitchData[endpoint_index].rxOptions = *pRxOpt;

  /* Apply here a endpoint mapping, endpoint 0 state maps to endpoint 1.
  Note: in order to map the root device to more than one end point,
  it is recommended to modify the Command handler to be called for each mapped End point
  e.g. call CommandClassBinarySwitchSupportSet() for every mapped
  end point in handleCommandClassBinarySwitch() */
  if (0 == ZAF_TSE_BinarySwitchData[endpoint_index].rxOptions.destNode.endpoint)
  {
    ZAF_TSE_BinarySwitchData[endpoint_index].rxOptions.destNode.endpoint = 1;
  }

  return &ZAF_TSE_BinarySwitchData[endpoint_index];
}


/**
 * Considering several endpoints supporting the same Command Class,
 * this function finds the index of the end point if they were stored in an array.
 * e.g. if Endpoints 0, 3, 5 and 12 support the Command Class, their index would
 * be respectively 0, 1, 2 and 3.
 * @param endpoint Indicating the endpoint identifier to search for.
*/
uint8_t CC_MultilevelSwitch_getEndpointIndex(uint8_t endpoint)
{
  for (uint8_t i=0; i<sizeof(endpoints_Supporting_MultilevelSwitch_CC); i++)
  {
    if (endpoint == endpoints_Supporting_MultilevelSwitch_CC[i]){
      return i;
    }
  }
  /*End point was not found, this should not happen but in this case, just return the first End Point */
  return 0;
}


/**
 * Prepare the data input for the TSE for any Multilevel Switch CC command based on the pRxOption pointer.
 * @param pRxOpt pointer used to indicate how the frame was received (if any) and what endpoints are affected
*/
void* CC_MultilevelSwitch_prepare_zaf_tse_data(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  uint8_t endpoint_index = CC_MultilevelSwitch_getEndpointIndex(pRxOpt->destNode.endpoint);
  memset(&ZAF_TSE_MultilevelSwitchData[endpoint_index], 0, sizeof(s_CC_multilevelSwitch_data_t));
  ZAF_TSE_MultilevelSwitchData[endpoint_index].rxOptions = *pRxOpt;

  /* Apply here a endpoint mapping, endpoint 0 state maps to endpoint 2
  Note: in order to map the root device to more than one end point,
  it is recommended to modify the Command handler to be called for each mapped End point
  e.g. call CC_MultilevelSwitch_SetValue() for every mapped
  end point in handleCommandClassMultilevelSwitch() */
  if (0 == ZAF_TSE_MultilevelSwitchData[endpoint_index].rxOptions.destNode.endpoint)
  {
    ZAF_TSE_MultilevelSwitchData[endpoint_index].rxOptions.destNode.endpoint = 2;
  }

  return &ZAF_TSE_MultilevelSwitchData[endpoint_index];
}

uint16_t handleFirmWareIdGet(uint8_t n)
{
  if (n == 0)
  {
    // Firmware 0
    return APP_FIRMWARE_ID;
  }
  // Invalid Firmware number
  return 0;
}

uint8_t CC_Version_GetHardwareVersion_handler(void)
{
  return 1;
}

void CC_ManufacturerSpecific_ManufacturerSpecificGet_handler(uint16_t * pManufacturerID,
                                                             uint16_t * pProductID)
{
  *pManufacturerID = APP_MANUFACTURER_ID;
  *pProductID = APP_PRODUCT_ID;
}

/*
 * This function will report a serial number in a binary format according to the specification.
 * The serial number is the chip serial number and can be verified using the Simplicity Commander.
 * The choice of reporting can be changed in accordance with the Manufacturer Specific
 * Command Class specification.
 */
void CC_ManufacturerSpecific_DeviceSpecificGet_handler(device_id_type_t * pDeviceIDType,
                                                       device_id_format_t * pDeviceIDDataFormat,
                                                       uint8_t * pDeviceIDDataLength,
                                                       uint8_t * pDeviceIDData)
{
  *pDeviceIDType = DEVICE_ID_TYPE_SERIAL_NUMBER;
  *pDeviceIDDataFormat = DEVICE_ID_FORMAT_BINARY;
  *pDeviceIDDataLength = 8;
  uint64_t uuID = SYSTEM_GetUnique();
  DPRINTF("\r\nuuID: %x", (uint32_t)uuID);
  *(pDeviceIDData + 0) = (uint8_t)(uuID >> 56);
  *(pDeviceIDData + 1) = (uint8_t)(uuID >> 48);
  *(pDeviceIDData + 2) = (uint8_t)(uuID >> 40);
  *(pDeviceIDData + 3) = (uint8_t)(uuID >> 32);
  *(pDeviceIDData + 4) = (uint8_t)(uuID >> 24);
  *(pDeviceIDData + 5) = (uint8_t)(uuID >> 16);
  *(pDeviceIDData + 6) = (uint8_t)(uuID >>  8);
  *(pDeviceIDData + 7) = (uint8_t)(uuID >>  0);
}
