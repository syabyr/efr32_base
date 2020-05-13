/**
 * @file
 * This module implements functions used in combination with command class firmware update.
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <ota_util.h>
#include <ZW_TransportLayer.h>

#include <CRC.h>

#include <misc.h>

#include <CC_FirmwareUpdate.h>
#include <CC_ManufacturerSpecific.h>

#include <AppTimer.h>
#include <SwTimer.h>

#include <string.h>
#include <ZAF_Common_interface.h>

#include<btl_interface.h>
#include<btl_interface_storage.h>
#include<BootLoader/OTA/bootloader-slot-configuration.h>
#include <em_wdog.h>

#include "stdlib.h"
#include "em_msc.h"
#include <CC_Version.h>
#include <btl_reset_cause_util.h>
#include <nvm3.h>
#include <ZAF_file_ids.h>
#include <ZAF_nvm3_app.h>

//#define DEBUGPRINT
#include "DebugPrint.h"


/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

typedef enum _FW_STATE_
{
  FW_STATE_DISABLE,//!< FW_STATE_DISABLE
  FW_STATE_READY,  //!< FW_STATE_READY
  FW_STATE_ACTIVE, //!< FW_STATE_ACTIVE
} FW_STATE;

typedef enum _FW_EV_
{
  FW_EV_FORCE_DISABLE,
  FW_EV_VALID_COMBINATION,
  FW_EV_MD_REQUEST_REPORT_SUCCESS,
  FW_EV_MD_REQUEST_REPORT_FAILED,
  FW_EV_GET_NEXT_FW_FRAME,
  FW_EV_RETRY_NEXT_FW_FRAME,
  FW_EV_VERIFY_IMAGE,
  FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE,
  FW_EV_UPDATE_STATUS_CRC_ERROR

} FW_EV;

#define FIRMWARE_UPDATE_REQUEST_PACKET_TIMEOUT  10000   /* unit: 1 ms ticks */

#define FIRMWARE_UPDATE_REQEUST_TIMEOUT_UNIT    2000    /* unit: 1 ms ticks per sub-timeout */

#define HOST_REPONSE_TIMEOUT                    2000    /* unit: 1 ms ticks */

/* number of sub-timeouts to achieve total of FIRMWARE_UPDATE_REQUEST_PACKET_TIMEOUT */
#define FIRMWARE_UPDATE_REQUEST_TIMEOUTS        (FIRMWARE_UPDATE_REQUEST_PACKET_TIMEOUT / FIRMWARE_UPDATE_REQEUST_TIMEOUT_UNIT)

#define FIRMWARE_UPDATE_MAX_RETRY 10


#define BOOTLOADER_NO_OF_FLASHPAGES_IN_SECTOR  116U // This is the number of the flash pages in bootloader section

typedef enum _ST_DATA_WRITE_TO_HOST_
{
  ST_DATA_WRITE_PROGRESS,
  ST_DATA_WRITE_LAST,
  ST_DATA_WRITE_WAIT_HOST_STATUS
} ST_DATA_WRITE_TO_HOST;

#define APPLICATION_IMAGE_STORAGE_SLOT 0x0

#define ACTIVATION_SUPPORT_MASK_APP        0x80
#define ACTIVATION_SUPPORT_MASK_INITIATOR  0x01
#define ACTIVATION_SUPPORT_ENABLED_MASK    0x81

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

static uint8_t firmware_update_packetsize;

ST_DATA_WRITE_TO_HOST st_data_write_to_host;
typedef struct _OTA_UTIL_
{
  CC_FirmwareUpdate_start_callback_t pOtaStart;
  CC_FirmwareUpdate_host_write_callback_t pOtaExtWrite;
  CC_FirmwareUpdate_finish_callback_t pOtaFinish;
  FW_STATE fwState;
  OTA_STATUS finishStatus;
  uint16_t firmwareCrc;
  uint8_t fw_numOfRetries;
  SSwTimer * pTimerFwUpdateFrameGet;
  uint16_t firmwareUpdateReportNumberPrevious;
  uint16_t fw_crcrunning;
  uint8_t timerFWUpdateCount;
  uint8_t fwExtern;
  SSwTimer * pTimerHostResponse;
  uint8_t userReboot;
  RECEIVE_OPTIONS_TYPE_EX rxOpt;
  bool NVM_valid;
  uint8_t activation_enabled;
} OTA_UTIL;

OTA_UTIL myOta = {
    NULL, // pOtaStart
    NULL, // pOtaExtWrite
    NULL, // pOtaFinish
    FW_STATE_DISABLE,
    OTA_STATUS_DONE,
    0, // firmwareCrc
    0, // fw_numOfRetries
    NULL, // pTimerFwUpdateFrameGet
    0, // firmwareUpdateReportNumberPrevious
    0, // fw_crcrunning
    0, // timerFWUpdateCount
    0, // fwExtern
    NULL, // pTimerHostResponse
    0, // userReboot
    {0}, // rxOpt
    false,  // NVM_valid
    0, // activation_enabled
};

static uint8_t m_requestGetStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_VALID_COMBINATION_V5;

static SSwTimer TimerFwUpdateFrameGet;
static SSwTimer TimerHostResponse;
static int pageCnt =0;
static SSwTimer Timerotasuccess;

static int32_t pageno=1, prevpage=0;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/
void InitEvState();
static void SetEvState( FW_EV ev);
void TimerCancelFwUpdateFrameGet();
void TimerStartFwUpdateFrameGet();
void ZCB_TimerOutFwUpdateFrameGet(SSwTimer* pTimer);
void ZCB_FinishFwUpdate(TRANSMISSION_RESULT * pTransmissionResult);
void Reboot();
void OTA_WriteData(uint32_t offset, uint8_t* pData, uint16_t legth);
void TimerCancelHostResponse();
void TimerStartHostResponse();
void ZCB_TimerTimeoutHostResponse(SSwTimer* pTimer);
static void ZCB_VerifyImage(SSwTimer* pTimer);
static void UpdateStatusSuccess();
void SendFirmwareUpdateStatusReport(uint8_t status);

static inline bool ActivationIsEnabled(void)
{
  return (ACTIVATION_SUPPORT_ENABLED_MASK == myOta.activation_enabled);
}

/**
 * Do cleanup after end flash storage write/erase operation.
 *
 * All the MSC_Init() function does is to unlock the flash controller
 * to allow for other entities (such as the NVM3 module) to access the
 * internal flash device after we are done using it.
 *
 * This is required because the bootloader leaves the flash controller
 * LOCKED upon return from all OTA write/erase operations.
 */
static void OtaFlashWriteEraseDone(void)
{
  MSC_Init();
}

/*============================ Reboot ===============================
** Function description
** Reboot the node by enabling watchdog.
**
** Side effects:
****------------------------------------------------------------------*/
void
Reboot()
{
#ifdef EFR32ZG
  DPRINT("before calling reboot...\n");
  // Trigger a software system reset
  RMU->CTRL = (RMU->CTRL & ~_RMU_CTRL_SYSRMODE_MASK) | RMU_CTRL_SYSRMODE_FULL;
  NVIC_SystemReset();
#endif
}

bool CC_FirmwareUpdate_Init(
    CC_FirmwareUpdate_start_callback_t pOtaStart,
    CC_FirmwareUpdate_host_write_callback_t pOtaExtWrite,
    CC_FirmwareUpdate_finish_callback_t pOtaFinish,
    bool support_activation)
{
  int32_t retvalue;

  BootloaderStorageSlot_t slot;
  BootloaderInformation_t bloaderInfo;

  myOta.pOtaStart = pOtaStart;
  myOta.pOtaExtWrite = pOtaExtWrite;
  myOta.pOtaFinish = pOtaFinish;
  myOta.pTimerHostResponse = NULL;
  myOta.NVM_valid = true;

  pageCnt = 0;
  pageno = 1;
  prevpage = 0;

  if (true == support_activation) myOta.activation_enabled |= ACTIVATION_SUPPORT_MASK_APP;
  else myOta.activation_enabled &= ~ACTIVATION_SUPPORT_MASK_APP;

  retvalue = bootloader_init();
  if(retvalue != BOOTLOADER_OK)
  {
    DPRINTF("\r\nBootloader NOT OK! %x", retvalue);
    myOta.NVM_valid = false;
  }
  /* Checking the bootloader validity before proceed, if it is non silabs bootloader then make it non upgradable */
  bootloader_getInfo(&bloaderInfo);
  if(bloaderInfo.type != SL_BOOTLOADER)
  {
     DPRINTF("\r\nNo bootloader is present or non silabs bootloader hence it's not upgradable type =%x",bloaderInfo.type);
     myOta.NVM_valid = false;
  }
  /*Checking this bootloader has storage capablity or not, just in case a wrong bootloader been loaded into the device*/
  if(!(bloaderInfo.capabilities & BOOTLOADER_CAPABILITY_STORAGE))
  {
     DPRINT("\r\nThis bootloader do not have storage capablity hence it can't be used for OTA");
     myOta.NVM_valid = false;
  }

  //Get the bootloader storage slot information
  slot.address = 0;
  slot.length  = 0;
  bootloader_getStorageSlotInfo(APPLICATION_IMAGE_STORAGE_SLOT,&slot);

  DPRINTF("\r\nslot address %d, length %d",slot.address,slot.length);

  Ecode_t errCode = 0;
  uint32_t objectType;
  size_t dataLen = 0;
  nvm3_Handle_t * pFileSystem = ZAF_GetFileSystemHandle();
  errCode = nvm3_getObjectInfo(pFileSystem,
                               ZAF_FILE_ID_CC_FIRMWARE_UPDATE,
                               &objectType,
                               &dataLen);

  if ((ECODE_NVM3_OK != errCode) || (ZAF_FILE_SIZE_CC_FIRMWARE_UPDATE != dataLen))
  {
    DPRINT("\nFile default!");
    SFirmwareUpdateFile file = {0, 0};
    errCode = nvm3_writeData(pFileSystem,
                             ZAF_FILE_ID_CC_FIRMWARE_UPDATE,
                             &file,
                             ZAF_FILE_SIZE_CC_FIRMWARE_UPDATE);
    ASSERT(ECODE_NVM3_OK == errCode);
  }

  uint16_t bootloader_reset_reason;
  if (CC_FirmwareUpdate_IsFirstBoot(&bootloader_reset_reason))
  {
    DPRINT("\nFIRMWARE UPDATE DONE NOW!");

    SFirmwareUpdateFile file;
    errCode = nvm3_readData(pFileSystem,
                            ZAF_FILE_ID_CC_FIRMWARE_UPDATE,
                            &file,
                            ZAF_FILE_SIZE_CC_FIRMWARE_UPDATE);
    ASSERT(ECODE_NVM3_OK == errCode);

    DPRINTF("\nF INIT: %x", file.activation_was_applied);

    RECEIVE_OPTIONS_TYPE_EX rxOpt;
    rxOpt.sourceNode.nodeId   = file.srcNodeID;
    rxOpt.sourceNode.endpoint = file.srcEndpoint;
    rxOpt.rxStatus            = file.rxStatus;
    rxOpt.securityKey         = file.securityKey;
    rxOpt.destNode.endpoint   = 0; // Firmware update is part of the root device.

    if (ACTIVATION_SUPPORT_ENABLED_MASK == file.activation_was_applied)
    {
      // TX Activation Status Report including checksum
      DPRINT("\nTX Activation Status Report!");

      uint8_t status;
      if (BOOTLOADER_RESET_REASON_GO == bootloader_reset_reason)
      {
        status = FIRMWARE_UPDATE_ACTIVATION_STATUS_REPORT_FIRMWARE_UPDATE_COMPLETED_SUCCESSFULLY_V5;
      }
      else
      {
        status = FIRMWARE_UPDATE_ACTIVATION_STATUS_REPORT_ERROR_ACTIVATING_THE_FIRMWARE_V5;
      }
      CC_FirmwareUpdate_ActivationStatusReport_tx(&rxOpt, file.checksum, status);
    }
    else
    {
      uint8_t status;
      if (BOOTLOADER_RESET_REASON_GO == bootloader_reset_reason)
      {
        status = FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_V5;
      }
      else
      {
        status = FIRMWARE_UPDATE_MD_STATUS_REPORT_INVALID_FILE_HEADER_INFORMATION_V5;
      }
      // Tx Status Report
      DPRINT("\nTX Status Report!");
      CmdClassFirmwareUpdateMdStatusReport(&rxOpt,
                                           status,
                                           0,
                                           NULL);
    }
  }

  if(AppTimerRegister(&Timerotasuccess, false, ZCB_VerifyImage))
  {
    DPRINT("\r\n**Registering timer OK for last report...**");
  }
  else
  {
    DPRINT("\r\n**Registering timer Failed for  the last report**");
  }
  DPRINTF("\r\nInit including bootloader init finished--bootloader init status%x",retvalue);
  return myOta.NVM_valid;
}


/*============================ OTA_WriteData ===============================
** Function description
** Write data to the OTA NVM
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void
OTA_WriteData(uint32_t offset, uint8_t *pData, uint16_t length)
{
  int32_t retvalue = BOOTLOADER_OK;

  if (pageCnt < BOOTLOADER_NO_OF_FLASHPAGES_IN_SECTOR)
  {
    pageno = ((offset + length) / FLASH_PAGE_SIZE) + 1;
    if (pageno != prevpage) //address on the same page so no write
    {
      retvalue = bootloader_eraseRawStorage(
          BTL_STORAGE_SLOT_START_ADDRESS + (pageCnt * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
      prevpage = pageno;
      if (retvalue != BOOTLOADER_OK)
      {
        DPRINTF("OTA_ERROR_ERASING_FLASH ERROR =0x%x\n", retvalue);
      }
      else
      {
        //DPRINTF("OTA_SUCESS_ERASING_FLASH page=%d\n",pageCnt);
        pageCnt++;
      }
    }
  }
  /*writing gbl image to the flash*/
  if (0 == (length % 4))
  {
    retvalue = bootloader_writeRawStorage(BTL_STORAGE_SLOT_START_ADDRESS + offset, pData, length); //for EFR32ZG Chip
  }
  else
  {
    //last packet make the writing as 4 bytes alligned
    DPRINTF("Writing the last packet length is %d...\n", length);
    retvalue = bootloader_writeRawStorage(BTL_STORAGE_SLOT_START_ADDRESS + offset, pData,
                                          length + (4 - (length % 4)));
  }
  if (retvalue != BOOTLOADER_OK)
  {
    DPRINTF("OTA_ERROR_WRITING_FLASH ERROR =0x%x,length=%d,offset=%d", retvalue, length, offset);
  }
  OtaFlashWriteEraseDone(); // Required after end flash write/erase operation
}

/*======================== UpdateStatusSuccess ==========================
** Function to set ev state after successful verification of image.
**
** Side effects:
**
**-------------------------------------------------------------------------*/
static void UpdateStatusSuccess()
{

  DPRINT("OTA_SUCCESS_CB");
  if (ActivationIsEnabled())
  {
    SendFirmwareUpdateStatusReport(FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_WAITING_FOR_ACTIVATION_V5);
  }
  else if ( false == myOta.userReboot)
  {
    /* send FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_V5 to controller.
       Device reboot itself*/
    SendFirmwareUpdateStatusReport(FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_V5);
  }
  else
  {
    /* send FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_STORED_V5 to controller.
       User need to reboot device*/
    SendFirmwareUpdateStatusReport(FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_STORED_V5);
  }
  myOta.userReboot = false;
  myOta.finishStatus = OTA_STATUS_DONE;
  myOta.fwState = FW_STATE_DISABLE;
}

/*======================== ZCB_VerifyImage ==========================
** Timer callback to start image verification *after* we have ack/routed-ack'ed
** the last fw update frame.
**
** Side effects:
**
**-------------------------------------------------------------------------*/
static void ZCB_VerifyImage(SSwTimer* pTimer)
{
  SetEvState(FW_EV_VERIFY_IMAGE);
}

void
handleCmdClassFirmwareUpdateMdReport( uint16_t crc16Result,
                                      uint16_t firmwareUpdateReportNumber,
                                      uint8_t properties,
                                      uint8_t* pData,
                                      uint8_t fw_actualFrameSize)
{
  /*Check correct state*/
  if( FW_STATE_ACTIVE != myOta.fwState)
  {
    /*Not correct state.. just stop*/
    return;
  }

  /*Check checksum*/
  DPRINTF(" (CRC----: 0x%04X)", crc16Result);
  if (0 == crc16Result)
  {
    myOta.fw_numOfRetries = 0;
    /* Check report number */
    if (firmwareUpdateReportNumber == myOta.firmwareUpdateReportNumberPrevious + 1)
    {
      /* Right number*/
      uint32_t firstAddr = 0;
      if (0 == myOta.firmwareUpdateReportNumberPrevious)
      {
        /* First packet sets the packetsize for the whole firmware update transaction */
      }
      else
      {
        if ((firmware_update_packetsize != fw_actualFrameSize) && (!(properties & FIRMWARE_UPDATE_MD_REPORT_PROPERTIES1_LAST_BIT_MASK)))
        {
          DPRINTF("%c", '%');
          DPRINTF(" 0x%x", firmware_update_packetsize);
          DPRINTF("%c", '-');
          DPRINTF(" 0x%x", fw_actualFrameSize);
          /* (firmware_update_packetsize != fw_actualFrameSize) and not last packet - do not match.. do nothing. */
          /* Let the timer handle retries */
          return;
        }
      }
      myOta.firmwareUpdateReportNumberPrevious = firmwareUpdateReportNumber;

      firstAddr = ((uint32_t)(firmwareUpdateReportNumber - 1) * firmware_update_packetsize);
      myOta.fw_crcrunning = CRC_CheckCrc16(myOta.fw_crcrunning, pData, fw_actualFrameSize);

      /** Check: intern or extern Firmware update **/
      if( false == myOta.fwExtern)
      {
        /** Intern firmware to update **/
        OTA_WriteData(firstAddr, pData, fw_actualFrameSize);

        /* Is this the last report ? */
        if (properties & FIRMWARE_UPDATE_MD_REPORT_PROPERTIES1_LAST_BIT_MASK)
        {
          /*check CRC for received dataBuffer*/
          if (myOta.fw_crcrunning == myOta.firmwareCrc)
          {
            DPRINT("**OTA_SUCCESS_CRC**");
            /* Delay starting the CRC calculation so we can transmit
             * the ack or routed ack first */

              if(ESWTIMER_STATUS_FAILED == TimerStart(&Timerotasuccess, 100))
              {
                DPRINT("OTA_SUCCESS_NOTIMER");
                SetEvState(FW_EV_VERIFY_IMAGE);
              }

          }
          else
          {
            DPRINT("**OTA_FAIL CRC!!**");
            DPRINTF(" 0x%x", myOta.fw_crcrunning);
            DPRINTF("%c", '-');
            DPRINTF(" 0x%x", myOta.firmwareCrc);
            DPRINT("\r\n");
            SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
          }
        }
        else
        {
          SetEvState(FW_EV_GET_NEXT_FW_FRAME);
        }
      }
      else
      {
        /** Extern firmware to update **/
        if(NON_NULL( myOta.pOtaExtWrite ))
        {
          /* Is this the last report ? */
          if(properties & FIRMWARE_UPDATE_MD_REPORT_PROPERTIES1_LAST_BIT_MASK)
          {
            /*check CRC for received dataBuffer*/
            if (myOta.fw_crcrunning == myOta.firmwareCrc)
            {
              DPRINT("**OTA_SUCCESS_CRC**");
              st_data_write_to_host = ST_DATA_WRITE_LAST;
              /* Start timeout on host to response on OtaWriteFinish() */
              TimerStartHostResponse();
            }
            else
            {
              DPRINT("**OTA_FAIL CRC!!**");
              DPRINTF(" 0x%x", myOta.fw_crcrunning);
              DPRINTF("%c", '-');
              DPRINTF(" 0x%x", myOta.firmwareCrc);
              DPRINT("\r\n");
              SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
            }
          }
          else
          {
            st_data_write_to_host = ST_DATA_WRITE_PROGRESS;
            /* Start timeout on host to response on OtaWriteFinish() */
            TimerStartHostResponse();
          }


          myOta.pOtaExtWrite( pData, fw_actualFrameSize);

        }
        else{
          /* fail to send data to host*/
          SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
        }
      }
    }
    else{
      DPRINT("n");
      /* (firmwareUpdateReportNumber == myOta.firmwareUpdateReportNumberPrevious + 1) do noth match.. do nothing.
         Let the timer hande retries*/
    }

  }
  else
  {
    DPRINT("c");
    if (FIRMWARE_UPDATE_REQUEST_TIMEOUTS < myOta.timerFWUpdateCount)
    {
      if (FIRMWARE_UPDATE_MAX_RETRY < myOta.fw_numOfRetries)
      {
        DPRINT("FAILED!");
        /* Send invalid status*/
        SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
      }
    }
  }
}

void handleCmdClassFirmwareUpdateMdReqGet(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_FIRMWARE_UPDATE_MD_REQUEST_GET_V5_FRAME * pFrame,
  uint8_t cmdLength,
  uint8_t* pStatus)
{
  uint8_t fwTarget = 0;
  uint32_t fragmentSize = 0xFFFFFFFF;

  /* Verify if the FragmentSize and FirmwareTarget fields are part of the command (V3 and onwards) */
  if (sizeof(ZW_FIRMWARE_UPDATE_MD_REQUEST_GET_V3_FRAME) <= cmdLength)
  {
    fwTarget = pFrame->firmwareTarget;
    fragmentSize = 0;
    fragmentSize +=  (((uint32_t)pFrame->fragmentSize1) << 8);
    fragmentSize +=  (((uint32_t)pFrame->fragmentSize2) & 0xff);
  }

  if (sizeof(ZW_FIRMWARE_UPDATE_MD_REQUEST_GET_V4_FRAME) <= cmdLength)
  {
    // Activation bit
    if (pFrame->properties1 & FIRMWARE_UPDATE_MD_REQUEST_GET_PROPERTIES1_ACTIVATION_BIT_MASK_V5)
    {
      myOta.activation_enabled |= ACTIVATION_SUPPORT_MASK_INITIATOR;
    }
    else myOta.activation_enabled &= ~ACTIVATION_SUPPORT_MASK_INITIATOR;
  }

  /* Validate the hardwareVersion field (V5 and onwards) */
  uint8_t hardwareVersion = CC_Version_GetHardwareVersion_handler();
  if ((sizeof(ZW_FIRMWARE_UPDATE_MD_REQUEST_GET_V5_FRAME) <= cmdLength) &&
      (hardwareVersion != pFrame->hardwareVersion))
  {
    /* Invalid hardware version */
    *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_INVALID_HARDWARE_VERSION_V5;
    m_requestGetStatus = *pStatus;
    return;
  }

  if (pFrame->firmwareTarget >= CC_Version_getNumberOfFirmwareTargets_handler())
  {
    /*wrong target!!*/
    *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_NOT_UPGRADABLE_V5;
    m_requestGetStatus = *pStatus;
    return;
  }

  uint16_t manufacturerID = 0;
  uint16_t productID      = 0;
  CC_ManufacturerSpecific_ManufacturerSpecificGet_handler(&manufacturerID, &productID);

  uint32_t maxFragmentSize = (uint32_t)handleCommandClassFirmwareUpdateMaxFragmentSize() & 0x0000FFFF;

  if (0xFFFFFFFF == fragmentSize)
  {
    // The Request Get command did not contain a fragment size => Set it to max fragment size.
    fragmentSize = maxFragmentSize;
  }
  else if ((fragmentSize > maxFragmentSize) || (0 == fragmentSize))
  {
    /*
     * The fragment size given in Request Get was too high or zero.
     * Report status of invalid fragment size.
     */
    *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_INVALID_FRAGMENT_SIZE_V5;
    m_requestGetStatus = *pStatus;
    return;
  }

  uint16_t manufacturerIdIncoming = (((uint16_t)pFrame->manufacturerId1) << 8)
                                    | (uint16_t)pFrame->manufacturerId2;
  uint16_t firmwareIdIncoming = (((uint16_t)pFrame->firmwareId1) << 8)
                                | (uint16_t)pFrame->firmwareId2;
  uint16_t firmwareId = handleFirmWareIdGet(fwTarget);
  if ((manufacturerIdIncoming != manufacturerID) ||
      (firmwareIdIncoming != firmwareId))
  {
    *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_INVALID_COMBINATION_V5;
    m_requestGetStatus = *pStatus;
    return;
  }

  if (false == myOta.NVM_valid)
  {
    *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_NOT_UPGRADABLE_V5;
    m_requestGetStatus = *pStatus;
    return;
  }

  uint16_t checksumIncoming = (((uint16_t)pFrame->checksum1) << 8)
                              | (uint16_t)pFrame->checksum2;

  /*Firmware valid.. ask OtaStart to start update*/
  if(NON_NULL( myOta.pOtaStart ))
  {

    if(false == myOta.pOtaStart(handleFirmWareIdGet(fwTarget), checksumIncoming))
    {
      DPRINT("&");
      //SetEvState(FW_EV_FORCE_DISABLE);
      *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_REQUIRES_AUTHENTICATION_V5;
      m_requestGetStatus = *pStatus;
      return;
    }
  }
  InitEvState();
  memcpy( (uint8_t*) &myOta.rxOpt, (uint8_t*)rxOpt, sizeof(RECEIVE_OPTIONS_TYPE_EX));

  // Save activationstatus, checksum and RX options
  Ecode_t errCode = 0;
  SFirmwareUpdateFile file = {
                              .activation_was_applied = myOta.activation_enabled,
                              .checksum = checksumIncoming,
                              .srcNodeID = rxOpt->sourceNode.nodeId,
                              .srcEndpoint = rxOpt->sourceNode.endpoint,
                              .rxStatus = rxOpt->rxStatus,
                              .securityKey = (uint32_t)rxOpt->securityKey
  };
  nvm3_Handle_t * pFileSystem = ZAF_GetFileSystemHandle();
  errCode = nvm3_writeData(pFileSystem, ZAF_FILE_ID_CC_FIRMWARE_UPDATE, &file, ZAF_FILE_SIZE_CC_FIRMWARE_UPDATE);
  ASSERT(ECODE_NVM3_OK == errCode);

  DPRINTF("\nF: %x", file.activation_was_applied);

  SetEvState(FW_EV_VALID_COMBINATION);
  myOta.firmwareCrc = checksumIncoming;

  if(0 != fwTarget)
  {
	 DPRINT("**fwTarget external**");
     myOta.fwExtern = true;
  }
  else
  {
	DPRINT("**fwTarget Internal**");
    myOta.fwExtern = false;
  }

  firmware_update_packetsize = (uint8_t)fragmentSize;
  *pStatus = FIRMWARE_UPDATE_MD_REQUEST_REPORT_VALID_COMBINATION_V5;
  m_requestGetStatus = *pStatus;
}

void ZCB_CmdClassFwUpdateMdReqReport(uint8_t txStatus)
{
  if (FIRMWARE_UPDATE_MD_REQUEST_REPORT_VALID_COMBINATION_V5 != m_requestGetStatus)
  {
    // If the request get command failed, nothing was initiated. Hence, don't restart.
    return;
  }
  if(txStatus == TRANSMIT_COMPLETE_OK)
  {
    SetEvState(FW_EV_MD_REQUEST_REPORT_SUCCESS);
  }
  else{
    SetEvState(FW_EV_MD_REQUEST_REPORT_FAILED);
  }
}


/*============================ InitEvState ===============================
** Function description
** This function...
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void
InitEvState()
{
  myOta.fwState = FW_STATE_DISABLE;
  myOta.fw_crcrunning = 0x1D0F;
  myOta.firmwareUpdateReportNumberPrevious = 0;
  myOta.fw_numOfRetries = 0;
  myOta.firmwareCrc = 0;
  myOta.timerFWUpdateCount = 0;
}


/*============================ SetState ===============================
** Function description
** This function...
**
** Side effects:
**
**-------------------------------------------------------------------------*/
static void
SetEvState(FW_EV ev)
{
  DPRINTF("SetEvState ev=%d, st=%d\r\n", ev, myOta.fwState);

  switch(myOta.fwState)
  {
    case FW_STATE_DISABLE:
      if(ev == FW_EV_VALID_COMBINATION)
      {
        myOta.fwState = FW_STATE_READY;
        SetEvState(ev);
      }
      else
      {
        if( NON_NULL( myOta.pOtaFinish ) )
        {
          myOta.pOtaFinish(OTA_STATUS_ABORT);
        }
        else
        {
		  DPRINT("Reboot from 1\n");
          Reboot();
        }
      }
      break;


    case FW_STATE_READY:
      if(ev == FW_EV_FORCE_DISABLE)
      {
        /*Tell application it is aborted*/
        if( NON_NULL( myOta.pOtaFinish ) )
        {
          myOta.pOtaFinish(OTA_STATUS_ABORT);
        }
        else
        {
		  DPRINT("Reboot from 2\n");
          Reboot();
        }
        myOta.fwState = FW_STATE_DISABLE;
      }
      else  if (ev == FW_EV_VALID_COMBINATION)
      {
        CC_FirmwareUpdate_InvalidateImage();
        myOta.fw_crcrunning = 0x1D0F;
        myOta.firmwareUpdateReportNumberPrevious = 0;
        TimerCancelFwUpdateFrameGet();
        /*Stop timer*/
      }
      else if(ev == FW_EV_MD_REQUEST_REPORT_SUCCESS)
      {
        myOta.fwState = FW_STATE_ACTIVE;
        SetEvState(FW_EV_GET_NEXT_FW_FRAME);
      }
      else if((ev == FW_EV_MD_REQUEST_REPORT_FAILED)||
              (ev == FW_EV_GET_NEXT_FW_FRAME)||
              (ev == FW_EV_RETRY_NEXT_FW_FRAME)||
              (ev == FW_EV_VERIFY_IMAGE)||
              (ev == FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE))
      {
        myOta.fwState = FW_STATE_DISABLE;
        if( NON_NULL( myOta.pOtaFinish ) )
        {
          myOta.pOtaFinish(OTA_STATUS_ABORT);
        }
        else
        {
		  DPRINT("Reboot from 3\n");
          Reboot();
        }

        /*Stop timer*/
      }
      break;


    case FW_STATE_ACTIVE:
      switch(ev)
      {
        case FW_EV_FORCE_DISABLE:
        case FW_EV_VALID_COMBINATION:
          TimerCancelFwUpdateFrameGet();
          /*Tell application it is aborted*/
          if( NON_NULL( myOta.pOtaFinish ) )
          {
            myOta.pOtaFinish(OTA_STATUS_ABORT);
          }
          else
          {
			      DPRINT("Reboot from 4\n");
            Reboot();
          }

          myOta.fwState = FW_STATE_DISABLE;
          break;

        case FW_EV_MD_REQUEST_REPORT_SUCCESS:
        case FW_EV_MD_REQUEST_REPORT_FAILED:
          /* Ignore - this happens when someone sends us Request Gets
           * while we are busy updating */
          break;

        case FW_EV_GET_NEXT_FW_FRAME:
          TimerStartFwUpdateFrameGet();
          CmdClassFirmwareUpdateMdGet( &myOta.rxOpt, myOta.firmwareUpdateReportNumberPrevious + 1);
          /*Start/restart timer*/
          break;
        case FW_EV_RETRY_NEXT_FW_FRAME:
          DPRINTF(" FWUpdateCount %d retry nbr %d", myOta.timerFWUpdateCount, myOta.fw_numOfRetries);
          if( FIRMWARE_UPDATE_REQUEST_TIMEOUTS < ++(myOta.timerFWUpdateCount))
          {
            myOta.timerFWUpdateCount = 0;
            if (FIRMWARE_UPDATE_MAX_RETRY > ++(myOta.fw_numOfRetries))
            {
              CmdClassFirmwareUpdateMdGet( &myOta.rxOpt, myOta.firmwareUpdateReportNumberPrevious + 1);
              /*Start/restart timer*/
            }
            else
            {
              DPRINT("+");
              SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
            }
          }
          break;
        case FW_EV_VERIFY_IMAGE:
          if(OtaVerifyImage())
          {
            UpdateStatusSuccess();
          }
          else
          {
            SendFirmwareUpdateStatusReport(FIRMWARE_UPDATE_MD_STATUS_REPORT_DOES_NOT_MATCH_THE_FIRMWARE_TARGET_V5);
            DPRINT("FIRMWARE_UPDATE_MD_STATUS_REPORT_DOES_NOT_MATCH_THE_FIRMWARE_TARGET_V5");
            myOta.finishStatus = OTA_STATUS_ABORT;
            myOta.fwState = FW_STATE_DISABLE;
          }
          break;

        case FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE:
          SendFirmwareUpdateStatusReport(FIRMWARE_UPDATE_MD_STATUS_REPORT_UNABLE_TO_RECEIVE_V5);
          DPRINT("FIRMWARE_UPDATE_MD_STATUS_REPORT_UNABLE_TO_RECEIVE_V5");
          myOta.finishStatus = OTA_STATUS_ABORT;
          myOta.fwState = FW_STATE_DISABLE;
          break;

        case FW_EV_UPDATE_STATUS_CRC_ERROR:
          SendFirmwareUpdateStatusReport(FIRMWARE_UPDATE_MD_STATUS_REPORT_UNABLE_TO_RECEIVE_WITHOUT_CHECKSUM_ERROR_V5);
          DPRINT("FIRMWARE_UPDATE_MD_STATUS_REPORT_UNABLE_TO_RECEIVE_WITHOUT_CHECKSUM_ERROR_V5");
          myOta.finishStatus = OTA_STATUS_ABORT;
          myOta.fwState = FW_STATE_DISABLE;
          break;

      }
      break;
  }
}

/*============================ SendFirmwareUpdateStatus ==========================
**-------------------------------------------------------------------------*/
void
SendFirmwareUpdateStatusReport(uint8_t status)
{
  uint8_t waitTime = WAITTIME_FWU_FAIL;
  TimerCancelFwUpdateFrameGet();

  switch (status)
  {
    case FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_STORED_V5:
      // The image is stored. Report it and wait for user reboot.
      waitTime = 0;
      break;
    case FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_V5:
      // Reboot right away and report afterwards.
      bootloader_rebootAndInstall();
      return;
      break;
    case FIRMWARE_UPDATE_MD_STATUS_REPORT_SUCCESSFULLY_WAITING_FOR_ACTIVATION_V5:
      // Image is stored. Wait for activation.
      waitTime = 0;
      break;
    default:
      // Do nothing
      break;
  }

  if(JOB_STATUS_SUCCESS != CmdClassFirmwareUpdateMdStatusReport( &myOta.rxOpt,
                                                              status,
                                                              waitTime,
                                                              ZCB_FinishFwUpdate))
  {
    /*Failed to send frame and we do not get a CB. Inform app we are finish*/
    ZCB_FinishFwUpdate(NULL);
  }
}


/*============================ ZCB_FinishFwUpdate ==========================
** Function description
** Callback Finish Fw update status to application.
** a new Get frame.
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void ZCB_FinishFwUpdate(TRANSMISSION_RESULT * pTransmissionResult)
{
  UNUSED(pTransmissionResult);
  if (NULL != myOta.pOtaFinish)
  {
    myOta.pOtaFinish(myOta.finishStatus);
  }

  // Reboot if the firmware update went well and activation is disabled.
  if ((OTA_STATUS_DONE == myOta.finishStatus) &&
      !ActivationIsEnabled())
  {
    DPRINT("Reboot from ZCB_FinishFwUpdate\n");
    DPRINT("Now telling the bootloader new image to boot install\n");
    bootloader_rebootAndInstall();
  }
}

/*============================ OtaVerifyImage ==========================
** Function description
** Uses bootloder functionality to verify downloaded image.
**
** Side effects: Returns false in case of app version downgrade
**
**-------------------------------------------------------------------------*/
bool OtaVerifyImage()
{
  int32_t verifystat;

  static bool watchdogPrev_state = false;

  if (WDOG_IsEnabled())
  {
    watchdogPrev_state = true;
    WDOG_Enable(false);
  }

  verifystat = bootloader_verifyImage(APPLICATION_IMAGE_STORAGE_SLOT, NULL);

  if (BOOTLOADER_OK == verifystat)
  {
    DPRINT("bootloader_verifyImage went OK\n");
    WDOG_Enable(watchdogPrev_state);
    return true;
  }
  else
  {
    DPRINTF("booloader_verifyImage went WRONG!!   ERR: %4x\n", verifystat);
    // Prepare for the next OTA update attempt
    // Rewind the page counters and erase the storage
    pageno=1;
    prevpage=0;
    int32_t retvalue = bootloader_eraseStorageSlot(APPLICATION_IMAGE_STORAGE_SLOT);
    if (BOOTLOADER_OK != retvalue)
    {
      DPRINTF("bootloader_eraseStorageSlot FAILED!!   ERR: %4x\n", retvalue);
    }
    OtaFlashWriteEraseDone(); // Required after end flash write/erase operation
    WDOG_Enable(watchdogPrev_state);
    return false;
  }
}


/*============================ TimerCancelFwUpdateFrameGet ==================
** Function description
** Cancel timer for retries on Get next firmware update frame.
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void
TimerCancelFwUpdateFrameGet(void)
{
  if (myOta.pTimerFwUpdateFrameGet)
  {
    TimerStop(myOta.pTimerFwUpdateFrameGet);
    myOta.pTimerFwUpdateFrameGet = NULL;
  }
  myOta.fw_numOfRetries = 0;
}


/*============================ ZCB_TimerOutFwUpdateFrameGet =================
** Function description
** Callback on timeout on Get next firmware update frame. It retry to Send
** a new Get frame.
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void ZCB_TimerOutFwUpdateFrameGet(SSwTimer* pTimer)
{
  UNUSED(pTimer);
  SetEvState(FW_EV_RETRY_NEXT_FW_FRAME);
}


/*============================ TimerStartFwUpdateFrameGet ==================
** Function description
** Start or restart timer for retries on Get next firmware update frame.
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void
TimerStartFwUpdateFrameGet(void)
{
  myOta.fw_numOfRetries = 0;
  myOta.timerFWUpdateCount = 0;
  //DPRINT("Registering timer for pTimerFwUpdateFrameGeT\n");
  if (NULL == myOta.pTimerFwUpdateFrameGet)
  {
    myOta.pTimerFwUpdateFrameGet = &TimerFwUpdateFrameGet;
    bool timerRegisterStatus = AppTimerRegister(myOta.pTimerFwUpdateFrameGet, true, ZCB_TimerOutFwUpdateFrameGet);
    ASSERT(timerRegisterStatus);
    ESwTimerStatus timerStatus = TimerStart(myOta.pTimerFwUpdateFrameGet, FIRMWARE_UPDATE_REQEUST_TIMEOUT_UNIT);

    if (ESWTIMER_STATUS_FAILED == timerStatus)
    {
      /* No timer! we update without a timer for retries */
      myOta.pTimerFwUpdateFrameGet = NULL;
    }
  }
  else
  {
    TimerRestart(myOta.pTimerFwUpdateFrameGet);
  }
}

/*============================ TimerCancelHostResponse ===============================
** Function description
** Cancel timeout timer for host to response on OtaHostFWU_WriteFinish()
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void
TimerCancelHostResponse(void)
{
  TimerStop(myOta.pTimerHostResponse);
}

/*============================ TimerStartHostResponse ===============================
** Function description
** Start timeout timer for host to response on OtaHostFWU_WriteFinish()
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void
TimerStartHostResponse(void)
{
  DPRINT("Registering timer for TimerStartHostResponse\n");
  if( NULL == myOta.pTimerHostResponse )
  {
    myOta.pTimerHostResponse = &TimerHostResponse;
    AppTimerRegister(myOta.pTimerHostResponse, false, ZCB_TimerTimeoutHostResponse);
    TimerStart(myOta.pTimerHostResponse, HOST_REPONSE_TIMEOUT);
  }
  else
  {
    TimerRestart(myOta.pTimerHostResponse);
  }
}

/*============================ TimerTimeoutHostResponse ====================
** Function description
** Host did not response finish reading last frame. Just fail the process!
**
** Side effects:
**
**-------------------------------------------------------------------------*/
void ZCB_TimerTimeoutHostResponse(SSwTimer* pTimer)
{
  UNUSED(pTimer);
  SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
}

void
OtaHostFWU_WriteFinish(void)
{
  DPRINT("OtaHostFWU_WriteFinish");
//  DPRINT("\r\n");
  /* cancel timer*/
  TimerCancelHostResponse();
  if( ST_DATA_WRITE_PROGRESS == st_data_write_to_host)
  {
    //DPRINT("next F");
    /* get next frame*/
    SetEvState(FW_EV_GET_NEXT_FW_FRAME);
  }
  else if(ST_DATA_WRITE_LAST == st_data_write_to_host)
  {
    DPRINT("last F");
    /*Wait on host status.*/
    st_data_write_to_host = ST_DATA_WRITE_WAIT_HOST_STATUS;
    /* Tell host it was last frame*/
    if (NON_NULL( myOta.pOtaExtWrite ))
      myOta.pOtaExtWrite( NULL, 0);
  }
}

void
OtaHostFWU_Status( bool userReboot, bool status )
{
  DPRINT("OtaHostFWU_Status B:");
  DPRINTF("%d", userReboot);
  DPRINTF("%c", '_');
  DPRINTF("%d", status);
  DPRINT("\r\n");
  TimerCancelHostResponse();

  if( true == status )
  {
    /*Check state machine is finish geting data. We both states: ST_DATA_WRITE_LAST
      and ST_DATA_WRITE_WAIT_HOST_STATUS*/
    if((ST_DATA_WRITE_WAIT_HOST_STATUS == st_data_write_to_host) ||
       (ST_DATA_WRITE_LAST == st_data_write_to_host))
    {
      myOta.userReboot = userReboot;
      /* Delay starting the image verification so we can transmit
       * the ack or routed ack first */
      if(ESWTIMER_STATUS_FAILED == TimerStart(&Timerotasuccess, 100)) 
      {
        DPRINT("OTA_SUCCESS_NOTIMER");
        ZCB_VerifyImage(NULL);
      }
    }
    else{
      /* We are not finish getting data!!!*/
      SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
    }
  }
  else
  {
    /*host failed.. something is wrong.. stop process*/
    SetEvState(FW_EV_UPDATE_STATUS_UNABLE_TO_RECEIVE);
  }
}

/*
 * Maximum fragment size definitions.
 */
#define FIRMWARE_UPDATE_MD_REPORT_ENCAPSULATION_LENGTH 6

uint16_t handleCommandClassFirmwareUpdateMaxFragmentSize(void)
{
  uint16_t maxFragmentSize;

  maxFragmentSize = ZAF_getAppHandle()->pNetworkInfo->MaxPayloadSize - FIRMWARE_UPDATE_MD_REPORT_ENCAPSULATION_LENGTH;

  // Align with 32 bit due to the writing to flash.
  maxFragmentSize = maxFragmentSize - (maxFragmentSize % 4);

  return maxFragmentSize;
}

bool CC_FirmwareUpdate_ActivationSet_handler(
    ZW_FIRMWARE_UPDATE_ACTIVATION_SET_V5_FRAME * pFrame,
    uint8_t * pStatus)
{
  uint16_t firmwareId = handleFirmWareIdGet(pFrame->firmwareTarget);
  uint16_t manufacturerID = 0;
  uint16_t productID      = 0;
  CC_ManufacturerSpecific_ManufacturerSpecificGet_handler(&manufacturerID, &productID);
  uint8_t hardwareVersion = CC_Version_GetHardwareVersion_handler();

  uint16_t manufacturerIdIncoming = (((uint16_t)pFrame->manufacturerId1) << 8)
                                    | (uint16_t)pFrame->manufacturerId2;
  uint16_t firmwareIdIncoming = (((uint16_t)pFrame->firmwareId1) << 8)
                                | (uint16_t)pFrame->firmwareId2;
  uint16_t checksumIncoming = (((uint16_t)pFrame->checksum1) << 8)
                              | (uint16_t)pFrame->checksum2;
  if ((manufacturerIdIncoming != manufacturerID) ||
      (firmwareIdIncoming != firmwareId) ||
      (checksumIncoming != myOta.firmwareCrc) ||
      (pFrame->hardwareVersion != hardwareVersion))
  {
    *pStatus = FIRMWARE_UPDATE_ACTIVATION_STATUS_REPORT_INVALID_COMBINATION_V5;
    return false;
  }
  DPRINTF("\n manufacturerIdIncoming: %4x", manufacturerIdIncoming);
  DPRINTF("\n manufacturerID: %4x", manufacturerID);
  DPRINTF("\n firmwareIdIncoming: %4x", firmwareIdIncoming);
  DPRINTF("\n firmwareId: %4x", firmwareId);

  bootloader_rebootAndInstall();
  return true;
}
