/**
 * Z-Wave Certified Application Door Lock Key Pad
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

//To enable DEBUGPRINT you must first undefine DISABLE_USART0
//In Simplicity Studio open Project->Properties->C/C++ General->Paths and Symbols
//Remove DISABLE_USART0 from list found under # Symbols->GNU C
#if !defined(DISABLE_USART1) || !defined(DISABLE_USART0)
//#define DEBUGPRINT
#endif

#include "DebugPrint.h"
#include "DebugPrintConfig.h"
#include "config_app.h"
#include "ZAF_app_version.h"
#include <ZAF_file_ids.h>
#include "nvm3.h"
#include "ZAF_nvm3_app.h"
#include <em_system.h>
#include <ZW_slave_api.h>
#include <ZW_typedefs.h>
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

#include <CC_Basic.h>
#include <CC_Association.h>
#include <CC_AssociationGroupInfo.h>
#include <CC_Version.h>
#include <CC_ZWavePlusInfo.h>
#include <CC_PowerLevel.h>
#include <CC_DeviceResetLocally.h>
#include <CC_DoorLock.h>
#include <CC_Indicator.h>
#include <CC_UserCode.h>
#include <CC_Battery.h>
#include <CC_MultiChanAssociation.h>
#include <CC_Supervision.h>
#include <CC_FirmwareUpdate.h>
#include <CC_ManufacturerSpecific.h>

#include <string.h>

#include "zaf_event_helper.h"
#include "zaf_job_helper.h"

#include <ZAF_Common_helper.h>
#include <ZAF_PM_Wrapper.h>
#include <ZAF_network_learn.h>
#include <ota_util.h>
#include "ZAF_TSE.h"
#include "ZAF_adc.h"
#include <ZAF_CmdPublisher.h>
#include <em_wdog.h>
#include "events.h"

/****************************************************************************/
/* Application specific button and LED definitions                          */
/****************************************************************************/

#define DOORHANDLE_BTN       APP_BUTTON_A
#define BATTERY_REPORT_BTN   APP_BUTTON_B
#define ENTER_USER_CODE      APP_BUTTON_C
#define LATCH_STATUS_LED     APP_LED_A
#define BOLT_STATUS_LED      APP_LED_B

/* Ensure we did not allocate the same physical button or led to more than one function */
STATIC_ASSERT((APP_BUTTON_LEARN_RESET != DOORHANDLE_BTN) &&
              (APP_BUTTON_LEARN_RESET != BATTERY_REPORT_BTN) &&
              (APP_BUTTON_LEARN_RESET != ENTER_USER_CODE) &&
              (DOORHANDLE_BTN != BATTERY_REPORT_BTN) &&
              (DOORHANDLE_BTN != ENTER_USER_CODE) &&
              (BATTERY_REPORT_BTN != ENTER_USER_CODE),
              STATIC_ASSERT_FAILED_button_overlap);
STATIC_ASSERT((APP_LED_INDICATOR != LATCH_STATUS_LED) &&
              (APP_LED_INDICATOR != BOLT_STATUS_LED) &&
              (LATCH_STATUS_LED != BOLT_STATUS_LED),
              STATIC_ASSERT_FAILED_led_overlap);

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

/**
 * Application states. Function AppStateManager(..) includes the state
 * event machine.
 */
typedef enum _STATE_APP_
{
  STATE_APP_IDLE,
  STATE_APP_LEARN_MODE,
  STATE_APP_RESET,
  STATE_APP_TRANSMIT_DATA,
  STATE_UNSOLICITED_EVENT,
  STATE_BATT_DEAD,
  STATE_APP_POWERDOWN
 }
STATE_APP;

#define DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED 0xFE

#define APP_SUPPORTED_OUTSIDE_HANDLES DOOR_HANDLE_1
#define APP_SUPPORTED_INSIDE_HANDLES  0

/** 8-bit mask. Bit 0: Twist assist, bit 1: Block to block, others reserved */
#define APP_SUPPORTED_OPTIONS_FLAGS 0x00

/** 16-bit values */
#define APP_MAX_AUTORELOCKTIME  0
#define APP_MAX_HOLDANDRELEASETIME  0

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

/**
 * Please see the description of app_node_information_t.
 */
static uint8_t cmdClassListNonSecureNotIncluded[] =
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
  COMMAND_CLASS_VERSION,
  COMMAND_CLASS_MANUFACTURER_SPECIFIC,
  COMMAND_CLASS_DEVICE_RESET_LOCALLY,
  COMMAND_CLASS_INDICATOR,
  COMMAND_CLASS_POWERLEVEL,
  COMMAND_CLASS_BATTERY,
  COMMAND_CLASS_DOOR_LOCK,
  COMMAND_CLASS_USER_CODE,
  COMMAND_CLASS_ASSOCIATION_V2,
  COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2,
  COMMAND_CLASS_ASSOCIATION_GRP_INFO,
  COMMAND_CLASS_FIRMWARE_UPDATE_MD_V5
};


/**
 * Structure includes application node information list's and device type.
 */
app_node_information_t m_AppNIF =
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

static const SAppNodeInfo_t AppNodeInfo =
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


/**
 * Setup AGI lifeline table from config_app.h
 */
CMD_CLASS_GRP  agiTableLifeLine[] = {AGITABLE_LIFELINE_GROUP};

/**
 * Setup AGI lifeline profile
 */
AGI_PROFILE lifelineProfile = {ASSOCIATION_GROUP_INFO_REPORT_PROFILE_GENERAL,
                               ASSOCIATION_GROUP_INFO_REPORT_PROFILE_GENERAL_LIFELINE};

/**************************************************************************************************
 * Configuration for Z-Wave Plus Info CC
 **************************************************************************************************
 */
static const SCCZWavePlusInfo CCZWavePlusInfo = {
                               .pEndpointIconList = NULL,
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

/**
 * Command Class Door lock
 * Following parameters are not supported in application:
 * insideDoorHandleMode: Inside Door Handles Mode (4 bits)
 * outsideDoorHandleMode: Outside Door Handles Mode
 * condition: Door condition
 */
static cc_door_lock_data_t myDoorLock;


/**
 * Used by the Supervision Get handler. Holds RX options.
 */
static RECEIVE_OPTIONS_TYPE_EX rxOptionSupervision;

/**
 * Used by the Supervision Get handler. Holds Supervision session ID.
 */
static uint8_t sessionID;

// Timer
static SSwTimer SupervisionTimer;
#ifdef DEBUGPRINT
static uint8_t m_aDebugPrintBuffer[96];
#endif
static TaskHandle_t g_AppTaskHandle;

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

#define FILE_SIZE_APPLICATIONDATA     (sizeof(cc_door_lock_data_t))

SBatteryData BatteryData;

#define BATTERY_DATA_UNASSIGNED_VALUE (CMD_CLASS_BATTERY_LEVEL_FULL + 1)  // Just some value not defined in cc_battery_level_t

#define APP_EVENT_QUEUE_SIZE 5

/**
 * The following four variables are used for the application event queue.
 */
static SQueueNotifying m_AppEventNotifyingQueue;
static StaticQueue_t m_AppEventQueueObject;
static EVENT_APP eventQueueStorage[APP_EVENT_QUEUE_SIZE];
static QueueHandle_t m_AppEventQueue;


/* TSE variables */
/* Door Lock CC has a command returning WORKING, the application needs a timer for this */
static SSwTimer ZAF_TSE_operation_report_ActivationTimer;
s_CC_doorLock_data_t ZAF_TSE_doorLockData;
/* Keep an array of "working" stage endpoints */
static bool doorlock_in_working_state;
/* True Status Engine (TSE) variables */
static s_CC_indicator_data_t ZAF_TSE_localActuationIdentifyData = {
  .rxOptions = {
    .rxStatus = 0,          /* rxStatus, verified by the TSE for Multicast */
    .securityKey = 0,       /* securityKey, ignored by the TSE */
    .sourceNode = {0,0},    /* sourceNode (nodeId, endpoint), verified against lifeline destinations by the TSE */
    .destNode = {0,0}       /* destNode (nodeId, endpoint), verified by the TSE for local endpoint */
  },
  .indicatorId = 0x50      /* Identify Indicator*/
};

/**
* TSE simulated RX option for local change addressed to the Root Device
* All applications can use this variable when triggering the TSE after
* a local / non Z-Wave initiated change
*/
static RECEIVE_OPTIONS_TYPE_EX zaf_tse_local_actuation = {
    .rxStatus = 0,        /* rxStatus, verified by the TSE for Multicast */
    .securityKey = 0,     /* securityKey, ignored by the TSE */
    .sourceNode = {0,0},  /* sourceNode (nodeId, endpoint), verified against lifeline destinations by the TSE */
    .destNode = {0,0}     /* destNode (nodeId, endpoint), verified by the TSE for local endpoint */
};

/* Interval for checking and reporting battery status (in minutes) */
#define PERIODIC_BATTERY_CHECKING_INTERVAL_MINUTES 5

static SSwTimer BatteryCheckTimer;

static nvm3_Handle_t* pFileSystemApplication;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

void AppResetNvm(void);
void ZCB_BattReportSentDone(TRANSMISSION_RESULT * pTransmissionResult);
void DeviceResetLocallyDone(TRANSMISSION_RESULT * pTransmissionResult);
void DeviceResetLocally(void);
STATE_APP GetAppState();
void AppStateManager( EVENT_APP event);
static void ChangeState( STATE_APP newState);
void SetDefaultConfiguration(void);
bool LoadConfiguration(void);

static void LearnCompleted(void);
static void ApplicationTask(SApplicationHandles* pAppHandles);
static void SaveStatus(void);
void UpdateDoorLockCondition_RefreshMMI( void );

SBatteryData readBatteryData(void);
void writeBatteryData(const SBatteryData* pBatteryData);

void ZCB_CommandClassSupervisionGetReceived(SUPERVISION_GET_RECEIVED_HANDLER_ARGS * pArgs);
void ZCB_SupervisionTimerCallback(SSwTimer *pTimer);
void ZAF_TSE_operation_report_ActivationTimerCallback(SSwTimer *pTimer);
void ZCB_BatteryCheckTimerCallback(SSwTimer *pTimer);
void DefaultApplicationsSettings(void);


static inline door_lock_mode_t getCurrentMode();
static bool ValidateUserCode( uint8_t identifier, uint8_t const * const pCode, uint8_t len);

bool CheckAndReportBatteryLevel(void);

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
    DPRINTF("Incoming Rx msg  type:%u class:%u \r\n", RxPackage.eReceiveType, RxPackage.uReceiveParams.Rx.Payload.rxBuffer.ZW_Common.cmdClass);

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
          DPRINT("Tx Status received\r\n");
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
        DPRINTF("Learn Mode status: 0x%x\r\n", Status.Content.LearnModeStatus.Status);
        if (ELEARNSTATUS_ASSIGN_COMPLETE == Status.Content.LearnModeStatus.Status)
        {
          LearnCompleted();
        }
        else if(ELEARNSTATUS_LEARN_MODE_COMPLETED_TIMEOUT == Status.Content.LearnModeStatus.Status)
        {
          ZAF_EventHelperEventEnqueue((EVENT_APP) EVENT_SYSTEM_LEARNMODE_FINISHED);
        }
        else if(ELEARNSTATUS_SMART_START_IN_PROGRESS == Status.Content.LearnModeStatus.Status)
        {
          ZAF_EventHelperEventEnqueue(EVENT_APP_SMARTSTART_IN_PROGRESS);
        }
        else if(ELEARNSTATUS_LEARN_IN_PROGRESS == Status.Content.LearnModeStatus.Status)
        {
          ZAF_EventHelperEventEnqueue(EVENT_APP_LEARN_IN_PROGRESS);
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

/**
 *
 */
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

/**
 * @brief See description for function prototype in ZW_basis_api.h.
 */
ZW_APPLICATION_STATUS ApplicationInit(EResetReason_t eResetReason)
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
  currentState = STATE_APP_IDLE;

  DPRINT("\n\n-----------------------------------\n");
  DPRINT("Z-Wave Sample App: Door Lock Keypad\n");
  DPRINTF("SDK: %d.%d.%d ZAF: %d.%d.%d.%d [Freq: %d]\n",
          SDK_VERSION_MAJOR,
          SDK_VERSION_MINOR,
          SDK_VERSION_PATCH,
          ZAF_GetAppVersionMajor(),
          ZAF_GetAppVersionMinor(),
          ZAF_GetAppVersionPatchLevel(),
          ZAF_BUILD_NO,
          RadioConfig.eRegion);
  DPRINT("-----------------------------------\n");
  DPRINTF("%s: Hold/release: Activate/deactivate outside door handle #1\n", Board_GetButtonLabel(DOORHANDLE_BTN));
  DPRINTF("%s: Send battery report\n", Board_GetButtonLabel(BATTERY_REPORT_BTN));
  DPRINTF("%s: Toggle learn mode\n", Board_GetButtonLabel(APP_BUTTON_LEARN_RESET));
  DPRINT("      Hold 5 sec: Reset\n");
  DPRINTF("%s: Enter user code\n", Board_GetButtonLabel(ENTER_USER_CODE));
  DPRINTF("%s: Learn mode + identify\n", Board_GetLedLabel(APP_LED_INDICATOR));
  DPRINTF("%s: Latch closed(off)/open(on)\n", Board_GetLedLabel(LATCH_STATUS_LED));
  DPRINTF("%s: Bolt locked(on)/unlocked(off)\n", Board_GetLedLabel(BOLT_STATUS_LED));
  DPRINT("-----------------------------------\n\n");

  DPRINTF("ApplicationInit eResetReason = %d\n", g_eResetReason);

  CC_ZWavePlusInfo_Init(&CCZWavePlusInfo);

  memset((uint8_t *)&myDoorLock, 0x00, sizeof(myDoorLock));
  myDoorLock.type = DOOR_OPERATION_CONST;

  CC_Version_SetApplicationVersionInfo(ZAF_GetAppVersionMajor(),
                                       ZAF_GetAppVersionMinor(),
                                       ZAF_GetAppVersionPatchLevel(),
                                       ZAF_BUILD_NO);

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
  AppTimerRegister(&SupervisionTimer, false, ZCB_SupervisionTimerCallback);
  /* TSE initializations */
  AppTimerRegister(&ZAF_TSE_operation_report_ActivationTimer,false,ZAF_TSE_operation_report_ActivationTimerCallback);
  /* Timer for periodic battery level checking */
  AppTimerRegister(&BatteryCheckTimer, true, ZCB_BatteryCheckTimerCallback);
  TimerStart(&BatteryCheckTimer, PERIODIC_BATTERY_CHECKING_INTERVAL_MINUTES * 60 * 1000);

  ZAF_Init(g_AppTaskHandle, pAppHandles, &ProtocolConfig, ZAF_FLiRS_StayAwake);

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

  // Enables button events on test board
  Board_EnableButton(APP_BUTTON_LEARN_RESET);
  Board_EnableButton(DOORHANDLE_BTN);
  Board_EnableButton(BATTERY_REPORT_BTN);
  Board_EnableButton(ENTER_USER_CODE);

  Board_IndicatorInit(APP_LED_INDICATOR);
  Board_IndicateStatus(BOARD_STATUS_IDLE);

  /* Load the application settings from NVM3 file system */
  LoadConfiguration();

  /*
   * The following group of function calls initializes the Association Group Information CC.
   */
  AGI_Init();
  CC_AGI_LifeLineGroupSetup(agiTableLifeLine, sizeof_array(agiTableLifeLine), ENDPOINT_ROOT);
  Transport_OnApplicationInitSW(&m_AppNIF, ZAF_FLiRS_StayAwake);

  CommandClassSupervisionInit(
      CC_SUPERVISION_STATUS_UPDATES_NOT_SUPPORTED,
      ZCB_CommandClassSupervisionGetReceived,
      NULL);

  /* Enter SmartStart*/
  /* Protocol will commence SmartStart only if the node is NOT already included in the network */
  ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART, g_eResetReason);

  CC_FirmwareUpdate_Init(NULL,NULL,NULL, true);

  EventDistributorConfig(
    &g_EventDistributor,
    sizeof_array(g_aEventHandlerTable),
    g_aEventHandlerTable,
    NULL
    );

  // Wait for and process events
  DPRINT("DoorLockKeyPad Event processor Started\r\n");
  uint32_t iMaxTaskSleep = 0xFFFFFFFF;
  for (;;)
  {
    if (0xFFFFFFFF == EventDistributorDistribute(&g_EventDistributor, iMaxTaskSleep, 0))
    {
      return;
    }
  }
}

/**
 * @brief See description for function prototype in ZW_TransportEndpoint.h.
 */
received_frame_status_t
Transport_ApplicationCommandHandlerEx(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength)
{
  received_frame_status_t frame_status = RECEIVED_FRAME_STATUS_NO_SUPPORT;
  DPRINTF("\r\nCmd handler: %#02x", pCmd->ZW_Common.cmdClass);

  /* Call command class handlers */
  switch (pCmd->ZW_Common.cmdClass)
  {
    case COMMAND_CLASS_VERSION:
      frame_status = CC_Version_handler(rxOpt, pCmd, cmdLength);
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
      frame_status = CC_Powerlevel_handler(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_MANUFACTURER_SPECIFIC:
      frame_status = handleCommandClassManufacturerSpecific(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_ZWAVEPLUS_INFO:
      frame_status = handleCommandClassZWavePlusInfo(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_BATTERY:
      frame_status = CC_Battery_handler(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_USER_CODE:
      frame_status = CC_UserCode_handler(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_DOOR_LOCK_V2:
      frame_status = CC_DoorLock_handler(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_SUPERVISION:
      frame_status = handleCommandClassSupervision(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_MULTI_CHANNEL_ASSOCIATION_V2:
      frame_status = handleCommandClassMultiChannelAssociation(rxOpt, pCmd, cmdLength);
      break;

    case COMMAND_CLASS_BASIC:
      frame_status = CC_Basic_handler(rxOpt, pCmd, cmdLength);
      break;

	case COMMAND_CLASS_FIRMWARE_UPDATE_MD_V5:
      frame_status = handleCommandClassFWUpdate(rxOpt, pCmd, cmdLength);
      break;
  }
  return frame_status;
}


static void
LearnCompleted(void)
{
  uint8_t bNodeID = g_pAppHandles->pNetworkInfo->NodeId;

  /*If bNodeID= 0xff.. learn mode failed*/
  if(NODE_BROADCAST != bNodeID )
  {
    /*Success*/
    if (bNodeID == 0)
    {
      SetDefaultConfiguration();
    }
  }
  ZAF_EventHelperEventEnqueue((EVENT_APP) EVENT_SYSTEM_LEARNMODE_FINISHED);
  Transport_OnLearnCompleted(bNodeID);
}

/**
 * @brief See description for function prototype in misc.h.
 */
uint8_t
GetMyNodeID(void)
{
  return g_pAppHandles->pNetworkInfo->NodeId;
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
    /*Force state change to activate system-reset without taking care of current
      state.*/
    ChangeState(STATE_APP_RESET);
    /* Send reset notification*/
    DeviceResetLocally();
  }

  if(EVENT_APP_IS_POWERING_DOWN == event)
  {
    ChangeState(STATE_APP_POWERDOWN);
  }

  switch(currentState)
  {
    case STATE_APP_IDLE:
      if(EVENT_APP_REFRESH_MMI == event)
      {
        Board_IndicateStatus(BOARD_STATUS_IDLE);
        UpdateDoorLockCondition_RefreshMMI();
        break;
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

      if (BTN_EVENT_SHORT_PRESS(BATTERY_REPORT_BTN) == (BUTTON_EVENT)event)
      {
        DPRINT("\r\nBattery Level report transmit (keypress trig)\r\n");
        ChangeState(STATE_APP_TRANSMIT_DATA);

        if (false == ZAF_EventHelperEventEnqueue(EVENT_APP_NEXT_EVENT_JOB))
        {
          DPRINT("\r\n** EVENT_APP_NEXT_EVENT_JOB fail\r\n");
        }

        /*Add event's on job-queue*/
        ZAF_JobHelperJobEnqueue(EVENT_APP_SEND_BATTERY_LEVEL_REPORT);
      }

      if (EVENT_APP_PERIODIC_BATTERY_CHECK_TRIGGER == event)
      {
        /* Check the battery level and send a report to lifeline if required */
        if (true == CheckAndReportBatteryLevel())
        {
          /* Battery level report TX initiated */
          ChangeState(STATE_APP_TRANSMIT_DATA);
          /*
           * Add an empty event to remain in STATE_APP_TRANSMIT_DATA until
           * the TX complete callback event occurs
           */
          ZAF_JobHelperJobEnqueue(EVENT_EMPTY);
        }

        if (false == ZAF_EventHelperEventEnqueue(EVENT_APP_NEXT_EVENT_JOB))
        {
          DPRINT("\r\n** EVENT_APP_NEXT_EVENT_JOB fail\r\n");
        }
      }

      /*
       * Pressing the Enter User Code button simulates entering a user code
       * on a key pad.
       * The entered user code is hardcoded with the value of the default user
       * code of the application. Hence, the lock can be secured/unsecured by
       * default.
       *
       * If the user code for user ID 1 is changed to something else than the
       * default user code the lock can no longer be secured/unsecured by
       * pressing the button.
       */
      if (BTN_EVENT_SHORT_PRESS(ENTER_USER_CODE) == (BUTTON_EVENT)event)
      {
        DPRINT("\r\nUser code entered!\r\n");

        uint8_t userCode[4] = DEFAULT_USERCODE;
        if (true == ValidateUserCode(1, userCode, sizeof(userCode)))
        {
          DPRINT("\r\nValid user code!\r\n");
          door_lock_mode_t door_lock_mode = (DOOR_MODE_SECURED == getCurrentMode()) ? DOOR_MODE_UNSEC : DOOR_MODE_SECURED;

          CC_DoorLock_OperationSet_handler(door_lock_mode);

          /* Update the lifeline destinations when the door status has changed */
          void* pData = CC_Doorlock_prepare_zaf_tse_data(&zaf_tse_local_actuation);
          ZAF_TSE_Trigger((void*)CC_DoorLock_operation_report_stx, pData, true);
        }
      }

      /* Outside door handle #1 activated? */
      if (BTN_EVENT_HOLD(DOORHANDLE_BTN) == (BUTTON_EVENT)event)
      {
        myDoorLock.outsideDoorHandleState |= 0x01;
        UpdateDoorLockCondition_RefreshMMI();
        DPRINTF("outsideDoorHandleState %d\r\n", (0x01 & myDoorLock.outsideDoorHandleState));
        SaveStatus();
        /* Update the lifeline destinations when the door status has changed */
        void* pData = CC_Doorlock_prepare_zaf_tse_data(&zaf_tse_local_actuation);
        ZAF_TSE_Trigger((void*)CC_DoorLock_operation_report_stx, pData, true);
      }

      /* Outside door handle #1 deactivated?
       * NB: If DOORHANDLE_BTN is held for more than 5 seconds then BTN_EVENT_LONG_PRESS
       *     will be received on DOORHANDLE_BTN release instead of BTN_EVENT_UP
       */
      if ((BTN_EVENT_UP(DOORHANDLE_BTN) == (BUTTON_EVENT)event) ||
          (BTN_EVENT_LONG_PRESS(DOORHANDLE_BTN) == (BUTTON_EVENT)event))
      {
        myDoorLock.outsideDoorHandleState &= 0xFE;
        UpdateDoorLockCondition_RefreshMMI();
        DPRINTF("outsideDoorHandleState %d\r\n", (0x01 & myDoorLock.outsideDoorHandleState));
        SaveStatus();
        /* Update the lifeline destinations when the door status has changed */
        void* pData = CC_Doorlock_prepare_zaf_tse_data(&zaf_tse_local_actuation);
        ZAF_TSE_Trigger((void*)CC_DoorLock_operation_report_stx, pData, true);
      }
      break;

    case STATE_APP_LEARN_MODE:
      if(EVENT_APP_REFRESH_MMI == event)
      {
        Board_IndicateStatus(BOARD_STATUS_LEARNMODE_ACTIVE);
        UpdateDoorLockCondition_RefreshMMI();
      }

      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        LoadConfiguration();
      }

      if ((BTN_EVENT_SHORT_PRESS(APP_BUTTON_LEARN_RESET) == (BUTTON_EVENT)event) ||
          (EVENT_SYSTEM_LEARNMODE_STOP == (EVENT_SYSTEM)event))
      {
        DPRINT("\r\n STATE_APP_LEARN_MODE disable\r\n");
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
      if (EVENT_SYSTEM_LEARNMODE_FINISHED == (EVENT_SYSTEM)event)
      {
        //Go back to smart start if the node was excluded.
        //Protocol will not commence SmartStart if the node is already included in the network.
        ZAF_setNetworkLearnMode(E_NETWORK_LEARN_MODE_INCLUSION_SMARTSTART, g_eResetReason);

        DPRINT("\r\n STATE_APP_LEARN_MODE finish\r\n");
        ChangeState(STATE_APP_IDLE);

        /* If we are in a network and the Identify LED state was changed to idle due to learn mode, report it to lifeline */
        CC_Indicator_RefreshIndicatorProperties();
        ZAF_TSE_Trigger((void *)CC_Indicator_report_stx, &ZAF_TSE_localActuationIdentifyData, true);
      }
      break;

    case STATE_APP_RESET:
      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        /* Soft reset */
        Board_ResetHandler();
      }
      break;

    case STATE_UNSOLICITED_EVENT:
      /* ??? */
      break;

    case STATE_BATT_DEAD:
      /* ??? */
      break;

    case STATE_APP_POWERDOWN:
      /* Device is powering down.. wait!*/
      break;

    case STATE_APP_TRANSMIT_DATA:
      if(EVENT_APP_REFRESH_MMI == event)
      {
        UpdateDoorLockCondition_RefreshMMI();
        break;
      }

      if(EVENT_APP_FLUSHMEM_READY == event)
      {
        AppResetNvm();
        LoadConfiguration();
      }

      if(EVENT_APP_NEXT_EVENT_JOB == event)
      {
        uint8_t event;
        /*check job-queue*/
        if(true == ZAF_JobHelperJobDequeue(&event))
        {
          /*Let the event scheduler fire the event on state event machine*/
          ZAF_EventHelperEventEnqueue(event);
        }
        else{
          ZAF_EventHelperEventEnqueue(EVENT_APP_FINISH_EVENT_JOB);
        }
      }

      if(EVENT_APP_SEND_BATTERY_LEVEL_REPORT == event)
      {
        uint8_t battLevel = CC_Battery_BatteryGet_handler(ENDPOINT_ROOT);
        if (JOB_STATUS_SUCCESS != CC_Battery_LevelReport_tx(&lifelineProfile, ENDPOINT_ROOT, battLevel, ZCB_BattReportSentDone))
        {
          DPRINTF("\r\n** CC_Battery TX FAILED ** \r\n");
          ZAF_EventHelperEventEnqueue(EVENT_APP_NEXT_EVENT_JOB);
        }
      }

      if(EVENT_APP_FINISH_EVENT_JOB == event)
      {
        ChangeState(STATE_APP_IDLE);
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
  DPRINTF("State changed from %d to %d\r\n", currentState, newState);

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
 * @brief Reset delay callback.
 */
void
DeviceResetLocally(void)
{
  AGI_PROFILE lifelineProfile = {
      ASSOCIATION_GROUP_INFO_REPORT_PROFILE_GENERAL,
      ASSOCIATION_GROUP_INFO_REPORT_PROFILE_GENERAL_LIFELINE
  };

  DPRINT("\r\nCall locally reset\r\n");

  CC_DeviceResetLocally_notification_tx(&lifelineProfile, DeviceResetLocallyDone);
}

/**
 * @brief See description for function prototype in CC_Version.h.
 */
uint8_t
CC_Version_getNumberOfFirmwareTargets_handler(void)
{
  return 1; /*CHANGE THIS - firmware 0 version*/
}


/**
 * @brief See description for function prototype in CC_Version.h.
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

#define MY_BATTERY_SPEC_LEVEL_FULL         3000  // My battery's 100% level (millivolts)
#define MY_BATTERY_SPEC_LEVEL_EMPTY        2400  // My battery's 0% level (millivolts)
#define BATTERY_LEVEL_REPORTING_DECREMENTS   10  // Round off and report the level in 10% decrements (100%, 90%, 80%, etc)
/**
 * Report current battery level to battery command class handler
 *
 * @brief See description for function prototype in CC_Battery.h
 */
uint8_t
CC_Battery_BatteryGet_handler(uint8_t endpoint)
{
  uint32_t VBattery;
  uint8_t  accurateLevel;
  uint8_t  roundedLevel;

  UNUSED(endpoint);

  /*
   * Simple example how to use the ADC to measure the battery voltage
   * and convert to a percentage battery level on a linear scale.
   */
  ZAF_ADC_Enable();
  VBattery = ZAF_ADC_Measure_VSupply();
  DPRINTF("\r\nBattery voltage: %dmV", VBattery);
  ZAF_ADC_Disable();

  if (MY_BATTERY_SPEC_LEVEL_FULL <= VBattery)
  {
    // Level is full
    return (uint8_t)CMD_CLASS_BATTERY_LEVEL_FULL;
  }
  else if (MY_BATTERY_SPEC_LEVEL_EMPTY > VBattery)
  {
    // Level is empty (<0%)
    return (uint8_t)CMD_CLASS_BATTERY_LEVEL_WARNING;
  }
  else
  {
    // Calculate the percentage level from 0 to 100
    accurateLevel = (100 * (VBattery - MY_BATTERY_SPEC_LEVEL_EMPTY)) / (MY_BATTERY_SPEC_LEVEL_FULL - MY_BATTERY_SPEC_LEVEL_EMPTY);

    // And round off to the nearest "BATTERY_LEVEL_REPORTING_DECREMENTS" level
    roundedLevel =  (accurateLevel / BATTERY_LEVEL_REPORTING_DECREMENTS) * BATTERY_LEVEL_REPORTING_DECREMENTS; // Rounded down
    if ((accurateLevel % BATTERY_LEVEL_REPORTING_DECREMENTS) >= (BATTERY_LEVEL_REPORTING_DECREMENTS / 2))
    {
      roundedLevel += BATTERY_LEVEL_REPORTING_DECREMENTS; // Round up
    }
  }
  return roundedLevel;
}

/**
 * Function for periodically checking the battery level and sending a report to the lifeline if
 * the level differs from what was last reported.
 *
 * @return true if a report was sent / false if the battery level hasn't changed since last reported
 */
bool CheckAndReportBatteryLevel(void)
{
  uint8_t currentBatteryLevel;

  if (0 == GetMyNodeID())
  {
    // We are not network included. Nothing to do.
    DPRINTF("\r\n%s: Not included\r\n", __FUNCTION__);
    return false;
  }

  currentBatteryLevel = CC_Battery_BatteryGet_handler(ENDPOINT_ROOT);
  DPRINTF("\r\n%s: Current Level=%d, Last reported level=%d\r\n", __FUNCTION__, currentBatteryLevel, BatteryData.lastReportedBatteryLevel);

  if ((currentBatteryLevel == BatteryData.lastReportedBatteryLevel) ||
      (currentBatteryLevel == BatteryData.lastReportedBatteryLevel + BATTERY_LEVEL_REPORTING_DECREMENTS)) // Hysteresis
  {
    // Battery level hasn't changed (significantly) since last reported. Do nothing
    return false;
  }

  // Battery level has changed. Send a new update to the lifeline
  if (JOB_STATUS_SUCCESS != CC_Battery_LevelReport_tx(&lifelineProfile, ENDPOINT_ROOT, currentBatteryLevel, ZCB_BattReportSentDone))
  {
    DPRINTF("\r\n%s: TX FAILED ** \r\n", __FUNCTION__);
    return false;
  }

  // Report sucessfully sent. Update the last reported value and store in flash
  BatteryData.lastReportedBatteryLevel = currentBatteryLevel;
  writeBatteryData(&BatteryData);

  return true;
}

/**
 * @brief Function resets configuration to default values.
 */
void
SetDefaultConfiguration(void)
{
  AssociationInit(true, pFileSystemApplication);

  DefaultApplicationsSettings();

  Ecode_t errCode = nvm3_writeData(pFileSystemApplication, FILE_ID_APPLICATIONDATA, &myDoorLock, FILE_SIZE_APPLICATIONDATA);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code if this error can only be caused by some internal flash HW failure

  BatteryData.lastReportedBatteryLevel = BATTERY_DATA_UNASSIGNED_VALUE;
  writeBatteryData(&BatteryData);

  uint32_t appVersion = ZAF_GetAppVersion();
  errCode =  nvm3_writeData(pFileSystemApplication, ZAF_FILE_ID_APP_VERSION, &appVersion, ZAF_FILE_SIZE_APP_VERSION);

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

  myDoorLock.condition = 0; /* read HW-condition for the door: [door] Open/close,[bolt] Locked/unlocked,[Latch] Open/Closed */
  myDoorLock.insideDoorHandleMode |= APP_SUPPORTED_INSIDE_HANDLES;    /* enable all supported inside handles */
  myDoorLock.outsideDoorHandleMode |= APP_SUPPORTED_OUTSIDE_HANDLES;  /* enable all supported outside handles */

  uint32_t appVersion;
  Ecode_t versionFileStatus = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_APP_VERSION, &appVersion, ZAF_FILE_SIZE_APP_VERSION);

  if (ECODE_NVM3_OK == versionFileStatus)
  {
    if (ZAF_GetAppVersion() != appVersion)
    {
      // Add code for migration of file system to higher version here.
    }

    /* get stored values */
    CMD_CLASS_DOOR_LOCK_DATA savedDoorLock;
    Ecode_t errCode = nvm3_readData(pFileSystemApplication, FILE_ID_APPLICATIONDATA, &savedDoorLock, FILE_SIZE_APPLICATIONDATA);
    ASSERT(ECODE_NVM3_OK == errCode);    //Assert has been kept for debugging , can be removed from production code if this error can only be caused by some internal flash failure

    myDoorLock.condition = savedDoorLock.condition;
    myDoorLock.type = savedDoorLock.type;
    myDoorLock.insideDoorHandleMode = savedDoorLock.insideDoorHandleMode;
    myDoorLock.outsideDoorHandleMode = savedDoorLock.outsideDoorHandleMode;
    /* NB: Door handle states (insideDoorHandleState and outsideDoorHandleState)
     * are not really part of the configuration. For that reason we do not
     * set them here even though they are part of the saved config data.
     */
    myDoorLock.lockTimeoutMin = savedDoorLock.lockTimeoutMin;
    myDoorLock.lockTimeoutSec = savedDoorLock.lockTimeoutSec;

    BatteryData = readBatteryData();

    /* Initialize association module */
    AssociationInit(false, pFileSystemApplication);

    loadStatusPowerLevel();

//    /* There is a configuration stored, so load it */
//    LoadBatteryConfiguration();

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

  ASSERT(0 != pFileSystemApplication);     //we are not able to successfully opened the file system ,Assert has been kept for debugging,this can be removed from the production code as it is hard to fail , it is flash driver/ hardware failure

  Ecode_t errCode = nvm3_eraseAll(pFileSystemApplication);
  ASSERT(ECODE_NVM3_OK == errCode);          //Assert has been kept for debugging, we are not able to successfully erase the file system , removing this from production code as it can only cause by some flash hardware failure

  /* Initialize transport layer */
  Transport_SetDefault();

  /* Apparently there is no valid configuration in NVM3, so load */
  /* default values and save them. */
  SetDefaultConfiguration();
}


e_cmd_handler_return_code_t CC_Basic_Set_handler(uint8_t val, uint8_t endpoint)
{
  door_lock_mode_t mode;
  mode = (val == 0) ? DOOR_MODE_UNSEC : DOOR_MODE_SECURED;

  /* Return the mapped command handler return code */
  return CC_DoorLock_OperationSet_handler(mode);
}

uint8_t CC_Basic_GetCurrentValue_handler(uint8_t endpoint)
{
  cc_door_lock_operation_report_t operation_report;

  CC_DoorLock_OperationGet_handler(&operation_report);
  if ((uint8_t)operation_report.mode == 0)
  {
    /* For Doorlock mode = 0 return 0x00 */
    return 0x00;
  }
  else
  {
    /* For Doorlock mode > 0 return 0xFF */
    return 0xFF;
  }
}

uint8_t CC_Basic_GetTargetValue_handler(uint8_t endpoint)
{
  /* Report the same value as current value (mode) */
  return CC_Basic_GetCurrentValue_handler(endpoint);
}

uint8_t CC_Basic_GetDuration_handler(uint8_t endpoint)
{
  /* Duration always 0 for Doorlock */
  return 0x00;
}

/**
 * @brief validate a code agains idenfier usercode
 * @param[in] identifier user ID
 * @param[in] pCode pointer to pin code for validation agains pin code in NVM
 * @param[in] len length of the pin code
 */
static bool ValidateUserCode( uint8_t identifier, uint8_t const * const pCode, uint8_t len)
{
  SUserCode userCodeData[USER_ID_MAX];

  Ecode_t errCode = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_USERCODE, userCodeData, ZAF_FILE_SIZE_USERCODE);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging,this can be removed from the production code as it is hard to fail in read when a corressponding write is successfull

  if( (len == userCodeData[identifier - 1].userCodeLen) && ((USER_ID_OCCUPIED == userCodeData[identifier - 1].user_id_status) || (USER_ID_RESERVED == userCodeData[identifier - 1].user_id_status)))
  {
    if(0 == memcmp(pCode, userCodeData[identifier - 1].userCode, len))
    {
      return true;
    }
  }
  return false;
}

/**
 * @brief See description for function prototype in CC_UserCode.h
 */
e_cmd_handler_return_code_t
CC_UserCode_Set_handler(
  uint8_t identifier,
  USER_ID_STATUS id,
  uint8_t* pUserCode,
  uint8_t len,
  uint8_t endpoint )
{
  uint8_t i;
  UNUSED(endpoint);
  SUserCode userCodeData[USER_ID_MAX];

  Ecode_t errCode = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_USERCODE, userCodeData, ZAF_FILE_SIZE_USERCODE);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging,this can be removed from the production code as it is hard to fail in read when a corressponding write is successfull

  if(0 == identifier)
  {
    if(USER_ID_AVAILBLE == id) // it is possible to remove all user codes at once when 0 == identifier
    {
      for(i = 0; i < USER_ID_MAX; i++)
      {
        userCodeData[i].user_id_status = id;
        memset(userCodeData[i].userCode, 0xFF, len);
        userCodeData[i].userCodeLen = len;
      }
    }
  }
  else
  {
    userCodeData[identifier - 1].user_id_status = id;
    memcpy(userCodeData[identifier - 1].userCode, pUserCode, len);
    userCodeData[identifier - 1].userCodeLen = len;
  }

  errCode = nvm3_writeData(pFileSystemApplication, ZAF_FILE_ID_USERCODE, userCodeData, ZAF_FILE_SIZE_USERCODE);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging if we are able to write then certainly some failure in flash hardware/driver, can be removed from production build

  for(i = 0; i < len; i++)
  {
    DPRINTF("%d",*(pUserCode+i));
  }
  DPRINT("\r\n");
  return E_CMD_HANDLER_RETURN_CODE_HANDLED;
}

/**
 * @brief See description for function prototype in CC_UserCode.h
 */
bool
CC_UserCode_getId_handler(
  uint8_t identifier,
  USER_ID_STATUS* pId,
  uint8_t endpoint)
{
  UNUSED(endpoint);
  SUserCode userCodeData[USER_ID_MAX];

  Ecode_t errCode = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_USERCODE, userCodeData, ZAF_FILE_SIZE_USERCODE);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code if this error can only be caused by some internal flash failure

  *pId = userCodeData[identifier - 1].user_id_status;
  return true;
}

/**
 * @brief See description for function prototype in CC_UserCode.h
 */
bool
CC_UserCode_Report_handler(
  uint8_t identifier,
  uint8_t* pUserCode,
  uint8_t* pLen,
  uint8_t endpoint)
{
  uint8_t i;
  SUserCode userCodeData[USER_ID_MAX];
  UNUSED(endpoint);

  Ecode_t errCode = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_USERCODE, userCodeData, ZAF_FILE_SIZE_USERCODE);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , Read is difficult to fail when a write has a failure can be removed from production code if this error can only be caused by some internal flash failure

  *pLen = userCodeData[identifier - 1].userCodeLen;
  if(USERCODE_MAX_LEN >= *pLen)
  {
    memcpy(pUserCode, userCodeData[identifier - 1].userCode, *pLen);

    DPRINT("hCmdUC_Report = ");
    for(i = 0; i < *pLen; i++){
      DPRINTF("%d", *(pUserCode+i));
    }
    DPRINT("\r\n");
    return true;
  }
  return false;
}

/**
 * @brief See description for function prototype in CC_UserCode.h
 */
uint8_t
CC_UserCode_UsersNumberReport_handler(uint8_t endpoint)
{
  UNUSED(endpoint);
  return USER_ID_MAX;
}


static inline void BoltLock(cc_door_lock_data_t * pDoorLock)
{
  pDoorLock->condition &= 0xFD; /* Clear bolt condition bit (0 -> bold locked) */
}

static inline void BoltUnlock(cc_door_lock_data_t * pDoorLock)
{
  pDoorLock->condition |= 0x02; /* Set bolt condition bit (1 -> bold unlocked) */
}

/**
 * Returns current mode of the node
 * @return DOOR_MODE_SECURED if bolt is locked, DOOR_MODE_UNSEC otherwize
 */
static inline door_lock_mode_t getCurrentMode()
{
  if(0 == (0x02 & myDoorLock.condition))
  {
    /* if bolt lock -> mode is DOOR_MODE_SECURED*/
    return DOOR_MODE_SECURED;
  }

  return DOOR_MODE_UNSEC;
}


/**
 * See description for function prototype in CC_DoorLock.h
 *
 * This application supports only a subset of the door lock modes in the "Door Lock Operation Set"
 * command. The following modes are supported:
 *
 * - 0x00: Door Unsecured
 * - 0xFF: Door Secured
 */
e_cmd_handler_return_code_t CC_DoorLock_OperationSet_handler(door_lock_mode_t mode)
{
  DPRINTF("\r\nCC Door Lock Operation Set - mode: %#02x\r\n", (uint8_t)mode);

  if (getCurrentMode() == mode)
  {
    //Nothing to do, return success
    DPRINTF("%s(): Already in mode %#02x\r\n", __func__, mode);
    return E_CMD_HANDLER_RETURN_CODE_HANDLED;
  }

  if (DOOR_MODE_SECURED == mode)
  {
    DPRINTF("%s(): Switch to Secured mode %#02x\r\n", __func__, mode);
    BoltLock(&myDoorLock);

    UpdateDoorLockCondition_RefreshMMI();
    SaveStatus();
  }
  else if (DOOR_MODE_UNSEC == mode)
  {
    DPRINTF("%s(): Switch to Unsecured mode %#02x\r\n", __func__, mode);
    BoltUnlock(&myDoorLock);

    UpdateDoorLockCondition_RefreshMMI();
    SaveStatus();
  }
  else
  {
    DPRINTF("%s(): Mode %#02x not supported, failing \r\n", __func__, mode);
    //Receive mode not supported or it's a reserved value
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }
  return E_CMD_HANDLER_RETURN_CODE_WORKING;
}

/**
 * See description for function prototype in CC_DoorLock.h.
 */
void CC_DoorLock_OperationGet_handler(cc_door_lock_operation_report_t* pData)
{
  DPRINTF("\r\nCC Door Lock Operation Get");

  pData->mode = getCurrentMode();
  // Inside- and Outside Door Handle Mode must always report 0 if the current lock mode == SECURED
  pData->insideDoorHandleMode = pData->mode == DOOR_MODE_SECURED ? 0 : myDoorLock.insideDoorHandleMode;
  pData->outsideDoorHandleMode = pData->mode == DOOR_MODE_SECURED ? 0 : myDoorLock.outsideDoorHandleMode;
  pData->condition = myDoorLock.condition;
  pData->lockTimeoutMin = DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED;
  pData->lockTimeoutSec = DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED;
  pData->targetMode = pData->mode; // Assuming instantaneous lock state change, target and current mode always equal
  pData->duration = 0; // Duration is always 0 with instantaneous lock state change
}


/**
 * See description for function prototype in CC_DoorLock.h
 *
 * This application supports only a subset of the "Door Lock Configuration Set" command.
 *
 * The following stuff can be configured:
 * - Operation type: Constant operation
 * - Outside door handles mode: Handle 1 can be enabled or disabled
 */
e_cmd_handler_return_code_t CC_DoorLock_ConfigurationSet_handler(cc_door_lock_configuration_t * pData)
{
  DPRINT("Door Lock Configuration Set\n");

  if ((DOOR_OPERATION_CONST == pData->type) &&
      !(~APP_SUPPORTED_INSIDE_HANDLES & pData->insideDoorHandleMode) && // Fail if non-supported inside handles are set
      !(~APP_SUPPORTED_OUTSIDE_HANDLES & pData->outsideDoorHandleMode) &&  // Fail if non-supported outside handles are set
      !(APP_MAX_AUTORELOCKTIME < ((pData->autoRelockTime1 << 8) + pData->autoRelockTime2)) && // Fail if non-supported auto-relock time
      !(APP_MAX_HOLDANDRELEASETIME < ((pData->holdAndReleaseTime1 << 8) + pData->holdAndReleaseTime2)) && // Fail if non-supported hold and release time
      !(~APP_SUPPORTED_OPTIONS_FLAGS & pData->reservedOptionsFlags )  // Fail if non-supported options flags
     )
  {
    myDoorLock.insideDoorHandleMode  = pData->insideDoorHandleMode;
    myDoorLock.outsideDoorHandleMode = pData->outsideDoorHandleMode;

    UpdateDoorLockCondition_RefreshMMI();

    SaveStatus();
    return E_CMD_HANDLER_RETURN_CODE_HANDLED;
  }
  return E_CMD_HANDLER_RETURN_CODE_FAIL;
}


/**
 * @brief See description for function prototype in CC_DoorLock.h
 */
void
CC_DoorLock_ConfigurationGet_handler(cc_door_lock_configuration_t* pData)
{
  DPRINT("Door Lock Configuration Get\n");

  memset(pData, 0, sizeof(*pData));

  pData->type = myDoorLock.type;
  pData->insideDoorHandleMode = myDoorLock.insideDoorHandleMode;
  pData->outsideDoorHandleMode = myDoorLock.outsideDoorHandleMode;
  pData->lockTimeoutMin = DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED;
  pData->lockTimeoutSec = DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED;
  pData->autoRelockTime1 = 0;
  pData->autoRelockTime2 = 0;
  pData->holdAndReleaseTime1 = 0;
  pData->holdAndReleaseTime2 = 0;
  pData->reservedOptionsFlags = 0;
}

/**
 * @brief See description for function prototype in CC_DoorLock.h
 */
void CC_DoorLock_CapabilitiesGet_handler(cc_door_lock_capabilities_report_t* pData)
{

  pData->reserved = 0; // Reserved fields must be set to zero.
  pData->lengthSupportedOperationType = 0x01;
  pData->supportedOperationTypeBitmask = 1 << DOOR_OPERATION_CONST;
  pData->lengthSupportedDoorLockModeList = 2;
  pData->supportedDoorLockModeList[0] = DOOR_MODE_UNSEC;
  pData->supportedDoorLockModeList[1] =  DOOR_MODE_SECURED;
  pData->supportedOutsideHandleModes = APP_SUPPORTED_OUTSIDE_HANDLES;
  pData->supportedInsideHandleModes = APP_SUPPORTED_INSIDE_HANDLES;
  pData->supportedDoorComponents = DOOR_COMPONENT_LATCH | DOOR_COMPONENT_BOLT;
  pData->autoRelockSupport = 0;
  pData->holdAndReleaseSupport = 0;
  pData->twistAssistSupport = 0;
  pData->blockToBlockSupport = 0;

}

/**
 * @brief Set door conditions (LED) from door handle mode and state
 *
 * Door condition encodes the following:
 * - bit 0: Door open(0) or closed(1)
 * - bit 1: Bolt locked(0) or unlocked(1)
 * - bit 2: Latch open(0) or closed(1)
 */
void
UpdateDoorLockCondition_RefreshMMI( void )
{
  /*
   * Mode bit 0 value == 1  --> handle #1 can open the door
   * State bit 0 value == 1 --> handle #1 is activated
   */
  if((myDoorLock.outsideDoorHandleMode & 0x01) && (myDoorLock.outsideDoorHandleState & 0x01))
  {
    myDoorLock.condition &=  0xFB;   /* Latch open --> Clear latch bit (condition bit #2) */
  }
  else
  {
    myDoorLock.condition |=  0x04;   /* Latch closed --> Set latch bit (condition bit #2) */
  }

  DPRINTF("SetCondition con = %d\r\n", myDoorLock.condition );

  if(0x04 & myDoorLock.condition)
  {
    Board_SetLed(LATCH_STATUS_LED, LED_OFF);
    DPRINT("Latch closed\r\n");
  }
  else
  {
    Board_SetLed(LATCH_STATUS_LED, LED_ON);
    DPRINT("Latch open\r\n");
  }

  if(0x02 & myDoorLock.condition)
  {
    Board_SetLed(BOLT_STATUS_LED, LED_OFF);
    DPRINT("Bolt unlocked\r\n");
  }
  else
  {
    Board_SetLed(BOLT_STATUS_LED, LED_ON);
    DPRINT("Bolt locked\r\n");
  }
}


/**
 * @brief Callback function used when sending battery report.
 */
void
ZCB_BattReportSentDone(TRANSMISSION_RESULT * pTransmissionResult)
{
  if (TRANSMISSION_RESULT_FINISHED == pTransmissionResult->isFinished)
  {
    DPRINTF("\r\nBattery level report transmit finished\r\n");
    ZAF_EventHelperEventEnqueue(EVENT_APP_NEXT_EVENT_JOB);
  }
}



/**
 * @brief Stores the current status of the lock on/off
 * in the application NVM3 file system.
 */
static void
SaveStatus(void)
{
  Ecode_t errCode = nvm3_writeData(pFileSystemApplication, FILE_ID_APPLICATIONDATA, &myDoorLock, FILE_SIZE_APPLICATIONDATA);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code if this error can only be caused by some internal flash hw failure
}

/**
 * Handles Supervision Get commands sent to this node.
 *
 * The purpose of Supervision is to inform the source node (controller) when the door lock
 * operation is finished. Since this application does not run on in a real door lock and therefore
 * has no door lock hardware, a timer is implemented to show how Supervision can be used. This timer
 * represents the physical bolt of a door lock.
 * The first Supervision report will be transmitted automatically by the ZAF, but transmission
 * of the next report(s) must be handled by the application.
 *
 * When receiving a Supervision Get, this handler will be invoked. For this specific application
 * only a Door Lock Operation Set is a special case. The function will change the Supervision
 * status to be 'working'. A timer is then started and when it triggers, it will send another
 * Supervision Report with the status of 'done'.
 *
 * In the case of other commands than Door Lock Operation Set, the application must make sure that
 * the 'more status updates' field in properties1 are set to zero indicating that no more status
 * updates will be sent.
 */
void
ZCB_CommandClassSupervisionGetReceived(SUPERVISION_GET_RECEIVED_HANDLER_ARGS * pArgs)
{
  if ((DOOR_LOCK_OPERATION_SET_V2 == pArgs->cmd && COMMAND_CLASS_DOOR_LOCK_V2 == pArgs->cmdClass) ||
      (BASIC_SET_V2 == pArgs->cmd && COMMAND_CLASS_BASIC_V2 == pArgs->cmdClass)) // Basic Set mimics Door Lock Operation Set
  {
    /* Status for DOOR_LOCK_OPERATION_SET_V2 or BASIC_SET_V2 */
    pArgs->status = CC_SUPERVISION_STATUS_WORKING;
    pArgs->duration = 2;

    if(CC_SUPERVISION_STATUS_UPDATES_SUPPORTED == CC_SUPERVISION_EXTRACT_STATUS_UPDATE(pArgs->properties1))
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

      pArgs->properties1 = CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_REPORTS_TO_FOLLOW);
      pArgs->properties1 |= CC_SUPERVISION_ADD_SESSION_ID(sessionID);
      // Start timer which will send another Supervision report when triggered.
      DPRINT("\r\n*** CC_SUPERVISION_STATUS_UPDATES_SUPPORTED\r\n");

      TimerStart(&SupervisionTimer, 2 * 1000);
    }
  }
  else
  {
    /* Status for all other commands */
    pArgs->properties1 &= (CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_THIS_IS_LAST) | 0x7F);
    pArgs->duration = 0;
  }
}

/**
 * @brief Transmits a Supervision report.
 * @details This function is triggered by the timer set in the Supervision Get handler.
 * @param pTimer Timer object assigned to this function
 */
void
ZCB_SupervisionTimerCallback(SSwTimer *pTimer)
{
  TRANSMIT_OPTIONS_TYPE_SINGLE_EX * pTxOptions;
  RxToTxOptions(&rxOptionSupervision, &pTxOptions);
  CmdClassSupervisionReportSend(
      pTxOptions,
      CC_SUPERVISION_ADD_MORE_STATUS_UPDATE(CC_SUPERVISION_MORE_STATUS_UPDATES_THIS_IS_LAST) | CC_SUPERVISION_ADD_SESSION_ID(sessionID),
      CC_SUPERVISION_STATUS_SUCCESS,
      0);
  UNUSED(pTimer);
}

void DefaultApplicationsSettings(void)
{
  uint8_t i;
  uint8_t defaultUserCode[] = DEFAULT_USERCODE;
  SUserCode userCodeDefaultData[USER_ID_MAX];

  DPRINT("\r\nDefaultApplicationsSettings\r\n");

  /* Its alive */
  memset(&myDoorLock, 0x00, sizeof(myDoorLock));
  myDoorLock.type = DOOR_OPERATION_CONST;
  myDoorLock.insideDoorHandleMode  = APP_SUPPORTED_INSIDE_HANDLES;  /* all supported inside handles enabled */
  myDoorLock.outsideDoorHandleMode = APP_SUPPORTED_OUTSIDE_HANDLES;  /* all supported outside handles enabled */
  myDoorLock.lockTimeoutMin = DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED;
  myDoorLock.lockTimeoutSec = DOOR_LOCK_OPERATION_SET_TIMEOUT_NOT_SUPPORTED;
  myDoorLock.outsideDoorHandleState = 0; /* Handles not being pressed */
  myDoorLock.insideDoorHandleState = 0; /* Handles not being pressed */
  myDoorLock.condition = 0x02; /*Set bolt unsecured by default. (must clear reserved bits 7..3) */


  userCodeDefaultData[0].user_id_status = USER_ID_OCCUPIED;
  userCodeDefaultData[0].userCodeLen = sizeof(defaultUserCode);
  memcpy(userCodeDefaultData[0].userCode, defaultUserCode, userCodeDefaultData[0].userCodeLen);

  if(USER_ID_MAX > 1)
  {
    for (i = 1; i < USER_ID_MAX; i++)
    {
      userCodeDefaultData[i].user_id_status = USER_ID_AVAILBLE;
      userCodeDefaultData[i].userCodeLen = sizeof(defaultUserCode);
      memset(userCodeDefaultData[i].userCode, 0xFF, userCodeDefaultData[i].userCodeLen);
    }
  }

  Ecode_t errCode = nvm3_writeData(pFileSystemApplication, ZAF_FILE_ID_USERCODE, userCodeDefaultData, ZAF_FILE_SIZE_USERCODE);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code if this error can only be caused by some internal flash failure

}

/**
 * @brief This function triggers the TSE after a WORKING status for
 * the Door Lock Operation Set handler
 * @param pTimer SSwTimer pointer that triggered the callback
 */
void
ZAF_TSE_operation_report_ActivationTimerCallback(SSwTimer *pTimer)
{
  if (&ZAF_TSE_operation_report_ActivationTimer != pTimer)
  {
    DPRINTF("%s(): Call with wrong SSwTimer pointer. Ignoring\n\r", __func__);
    return ;
  }
  /* Find out if we have been in the working state and need a TSE trigger */
  if (true == doorlock_in_working_state)
  {
    /* Start a TSE trigger for the Door Lock Operation Report */
    if (true == ZAF_TSE_Trigger((void*)CC_DoorLock_operation_report_stx,(void*)&ZAF_TSE_doorLockData, true))
    {
      /* If successful, mark the Endpoint as handled. Else just wait for the next callback and retry */
      doorlock_in_working_state = false;
    }
  }
}

/**
 * @brief Timer function for periodic battery level checking
 * @param pTimer Timer object assigned to this function
 */
void
ZCB_BatteryCheckTimerCallback(SSwTimer *pTimer)
{
  UNUSED(pTimer);

  /* Send a battery level report to the lifeline  */
  if (false == ZAF_EventHelperEventEnqueue(EVENT_APP_PERIODIC_BATTERY_CHECK_TRIGGER))
  {
    DPRINT("\r\n** Periodic battery checking trigger FAILED\r\n");
  }
}

/**
 * This function is used to notify the Application that the CC Door Lock Operation Set
 * status is in a WORKING state. The application can subsequently make the TSE Trigger
 * using the information passed on from the rxOptions.
 * @param rxOption pointer used to when triggering the "working state"
*/
void CC_DoorLock_operation_report_notifyWorking(RECEIVE_OPTIONS_TYPE_EX *pRxOpt)
{
  /* Pick up the end point and reset previous RxOptions data.
  Here treat each endpoint differently if they do not map to the same command */
  CC_Doorlock_prepare_zaf_tse_data(pRxOpt);

  /* Set a flag to remember we have been in the Working state */
  doorlock_in_working_state= true;

  /* If the status is working, the application is in charge of making the TSE trigger
  at the proper time. Start a timer to callback the TSE Trigger at a later stage */
  TimerStart(&ZAF_TSE_operation_report_ActivationTimer,5*1000);
}

/**
 * This function is used to return a pointer for input data for the TSE trigger.
 * @param rxOption pointer that will be included in the ZAF_TSE input data.
*/
void* CC_Basic_prepare_zaf_tse_data(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  /* Basic is mapped to Door Lock for all end points. Call here different functions
  for each endpoint (rxOpt->destNode.endpoint) if they do not map to the same
  Command Class & Command */
  return CC_Doorlock_prepare_zaf_tse_data(pRxOpt);
}

/**
 * This function is used to return a pointer for input data for the TSE trigger.
 * @param rxOption pointer that will be included in the ZAF_TSE input data.
*/
void CC_Basic_report_stx(TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions,void* pData)
{
  /* Basic Report is mapped to Door Lock Operation Report for all end points.
  Call here different functions for each endpoint (pData->rxOpt->destNode.endpoint)
  if they do not map to the same Command Class & Command */
  CC_DoorLock_operation_report_stx(txOptions,pData);
}

/**
 * This function is used to notify the Application that the CC Basic Set
 * status is in a WORKING state. The application can subsequently make the TSE Trigger
 * using the information passed on from the rxOptions.
 * @param rxOption pointer used to when triggering the "working state"
*/
void CC_Basic_report_notifyWorking(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  /* Basic Report is mapped to Door Lock Operation Report for all end points.
  Call here different functions for each endpoint (pData->rxOpt->destNode.endpoint)
  if they do not map to the same Command Class & Command */
  CC_DoorLock_operation_report_notifyWorking(pRxOpt);
}

/**
 * Prepare the data input for the TSE for any Door Lock CC command based on the pRxOption pointer.
 * @param pRxOpt pointer used to indicate how the frame was received (if any) and what endpoints are affected
*/
void* CC_Doorlock_prepare_zaf_tse_data(RECEIVE_OPTIONS_TYPE_EX* pRxOpt)
{
  /* Here we ignore the pRxOpt->destNode.endpoint since we have only the root device */
  memset(&ZAF_TSE_doorLockData, 0, sizeof(s_CC_doorLock_data_t));
  ZAF_TSE_doorLockData.rxOptions = *pRxOpt;

  return &ZAF_TSE_doorLockData;
}

/**
 * @brief Reads battery data from file system.
 */
SBatteryData
readBatteryData(void)
{
  SBatteryData StoredBatteryData;

  Ecode_t errCode = nvm3_readData(pFileSystemApplication, ZAF_FILE_ID_BATTERYDATA, &StoredBatteryData, ZAF_FILE_SIZE_BATTERYDATA);
  if(ECODE_NVM3_OK != errCode)
  {
    StoredBatteryData.lastReportedBatteryLevel = BATTERY_DATA_UNASSIGNED_VALUE;
    writeBatteryData(&BatteryData);
  }

  return StoredBatteryData;
}

/**
 * @brief Writes battery data to file system.
 */
void writeBatteryData(const SBatteryData* pBatteryData)
{
  Ecode_t errCode = nvm3_writeData(pFileSystemApplication, ZAF_FILE_ID_BATTERYDATA, pBatteryData, ZAF_FILE_SIZE_BATTERYDATA);
  ASSERT(ECODE_NVM3_OK == errCode); //Assert has been kept for debugging , can be removed from production code. This error can only be caused by some internal flash HW failure
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
