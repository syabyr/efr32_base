/**
 * @file
 * Multilevel switch utilities source file
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <multilevel_switch.h>
#include <ZW_basis_api.h>
#include <ZW_TransportLayer.h>
#include "config_app.h"
#include <CC_MultilevelSwitch.h>
#include <misc.h>
#include <endpoint_lookup.h>
#include <AppTimer.h>
#include <SwTimer.h>

#include "DebugPrintConfig.h"
//#define DEBUGPRINT
#include "DebugPrint.h"
/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/

#define SWITCH_ENDPOINT_IDX  endpointidx

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

#define SWITCH_DIMMING_UP   0x01
#define SWITCH_IS_DIMMING   0x02
#define SWITCH_IS_ON        0x04

typedef struct __MultiLvlSwitch_
{
  uint8_t bOnStateSwitchLevel;  /*save the on state level value when we set the HW to off*/
  uint8_t bCurrentSwitchLevel;  /*hold the current switch level value*/
  uint8_t bTargetSwitchLevel;   /*hold the value we want to set the switch to when we are changing*/
  uint32_t lTicksCountReload;
  uint32_t lTicksCount;
  uint8_t switchFlag;
}_MultiLvlSwitch;


static _MultiLvlSwitch MultiLvlSwitch[SWITCH_MULTI_ENDPOINTS];
static ESwTimerStatus bMultiLevelSwTimerStatus;
static SSwTimer MultiLevelSwTimer;
static ENDPOINT_LOOKUP multiLevelEpLookup;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/
void ZCB_SwitchLevelHandler(SSwTimer* pTimer);

void
ZCB_SwitchLevelHandler(SSwTimer* pTimer)
{
  uint8_t i;
  bool boAllTimersOff = true;
  for (i = 0; i < GetEndPointCount(&multiLevelEpLookup); i++)
  {
    if (!(MultiLvlSwitch[i].switchFlag & SWITCH_IS_DIMMING))
      continue;
    if (!--MultiLvlSwitch[i].lTicksCount)
    {
      DPRINTF("Timeout: %d ", MultiLvlSwitch[i].bCurrentSwitchLevel);

      (MultiLvlSwitch[i].switchFlag & SWITCH_DIMMING_UP)? MultiLvlSwitch[i].bCurrentSwitchLevel++: MultiLvlSwitch[i].bCurrentSwitchLevel--;
      CommandClassMultilevelSwitchSet(MultiLvlSwitch[i].bCurrentSwitchLevel, FindEndPointID(&multiLevelEpLookup,i));
      if (MultiLvlSwitch[i].bCurrentSwitchLevel == MultiLvlSwitch[i].bTargetSwitchLevel)
      {
        DPRINTF("\r\nS %d", MultiLvlSwitch[i].bTargetSwitchLevel);
        MultiLvlSwitch[i].switchFlag &= ~SWITCH_IS_DIMMING;
        MultiLvlSwitch[i].bCurrentSwitchLevel =  CC_MultilevelSwitch_GetCurrentValue_handler( FindEndPointID(&multiLevelEpLookup,i) );
        if (MultiLvlSwitch[i].bCurrentSwitchLevel)
          MultiLvlSwitch[i].bOnStateSwitchLevel = MultiLvlSwitch[i].bCurrentSwitchLevel;
      }
      else
      {
        MultiLvlSwitch[i].lTicksCount =  MultiLvlSwitch[i].lTicksCountReload;
        boAllTimersOff = false;
      }
      DPRINT("\r\n");
    }
    else
    {
      boAllTimersOff = false;
    }
  }
  if (boAllTimersOff)
  {
    TimerStop(pTimer);
    bMultiLevelSwTimerStatus = ESWTIMER_STATUS_FAILED;

    DPRINT("T\r\n");
  }
}



/**
 * Starts a timer that does the level changing.
 * @param bEndPointIdx
 * @param bDuration
 * @param blevels
 */
static void StartLevelChange(uint8_t bEndPointIdx, uint8_t bDuration, uint8_t blevels)
{
  uint8_t endpointID = FindEndPointID(&multiLevelEpLookup,bEndPointIdx);
  DPRINTF("StartLevelChange : %d %d %d ", bEndPointIdx, bDuration, blevels);

  if(0 == blevels)
  {
	return;
  }

  if ((bDuration == 0) || (bDuration == 0xFF)) /*set the level instantly*/
                                               /*0xff is used to indicate factory default duration whic is 0 in our case.*/
  {
    CommandClassMultilevelSwitchSet( MultiLvlSwitch[bEndPointIdx].bTargetSwitchLevel, endpointID );
    MultiLvlSwitch[bEndPointIdx].bCurrentSwitchLevel = CC_MultilevelSwitch_GetCurrentValue_handler(endpointID);
    if (MultiLvlSwitch[bEndPointIdx].bCurrentSwitchLevel)
    {
      MultiLvlSwitch[bEndPointIdx].bOnStateSwitchLevel = MultiLvlSwitch[bEndPointIdx].bCurrentSwitchLevel;
    }
    DPRINTF(" %d", MultiLvlSwitch[bEndPointIdx].bCurrentSwitchLevel);
  }
  else
  {
    if (bDuration> 0x7F) /*duration is in minutes*/
    {
      MultiLvlSwitch[bEndPointIdx].lTicksCountReload = bDuration - 0x7F;
      /*convert the minutes to 10ms units */
      MultiLvlSwitch[bEndPointIdx].lTicksCountReload *= 6000; /*convert the minutes to 10ms units*/
    }
    else
    {/*duration in seconds*/
      MultiLvlSwitch[bEndPointIdx].lTicksCountReload = bDuration;
      MultiLvlSwitch[bEndPointIdx].lTicksCountReload *= 100; /*convert the seconds to 10ms units*/
    }
    /*calculate the number of 10ms ticks between each level change*/
    MultiLvlSwitch[bEndPointIdx].lTicksCountReload /= blevels;
    MultiLvlSwitch[bEndPointIdx].lTicksCount = MultiLvlSwitch[bEndPointIdx].lTicksCountReload; /* lTicksCount is used to count down */
    MultiLvlSwitch[bEndPointIdx].switchFlag |= SWITCH_IS_DIMMING;

    if (ESWTIMER_STATUS_FAILED == bMultiLevelSwTimerStatus)
    {
      bMultiLevelSwTimerStatus = TimerStart(&MultiLevelSwTimer, 10);
    }
    DPRINTF("%dEN\r\n", bMultiLevelSwTimerStatus);
  }
}

uint8_t GetOnStateLevel(uint8_t endpoint)
{
  uint8_t endpointidx = 0;
  endpointidx = FindEndPointIndex(&multiLevelEpLookup,endpoint);
  if (endpointidx == 0xFF)
    return 0;

  DPRINTF("GetOnStateLevel :%d ", endpoint);

  return MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel;
}

uint8_t GetTargetLevel(uint8_t endpoint)
{

  uint8_t endpointidx = 0;
  endpointidx = FindEndPointIndex(&multiLevelEpLookup,endpoint);
  if (endpointidx == 0xFF)
    return 0;

  DPRINTF("GetTargetLevel :%d ", endpoint);

  if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag & SWITCH_IS_DIMMING )
  {
    return MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel;
  }
  else
  {
    DPRINTF(" Dimming OFF ");
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel = CC_MultilevelSwitch_GetCurrentValue_handler(endpoint);
    return MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel;
  }
}

uint8_t GetCurrentDuration(uint8_t endpoint)
{
  uint32_t tmpDuration;
  uint8_t tmpLvl;

  uint8_t endpointidx = 0;
  endpointidx = FindEndPointIndex(&multiLevelEpLookup,endpoint);
  if (0xFF == endpointidx)
  {
    return 0xFE;
  }

  DPRINTF("GetCurrentDuration: %d ", endpointidx);

  if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag & SWITCH_IS_DIMMING )
  {
    if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel > MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel)
    {
      tmpLvl = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel - MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel;
    }
    else
    {
      tmpLvl = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel - MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel;
    }
    /*convert duration from 10ms ticks per level to seconds*/
    /*lTicksCountReload hold the number of 10ms ticks per level*/
    /*tmpLvl hild the number of remaining levels to reach the target.*/
    tmpDuration = (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].lTicksCountReload * tmpLvl)/100 ;
    if (tmpDuration > 127)
    {
      tmpDuration /= 60; /*convert to minuts*/
      tmpDuration += 127; /*add offset according to command class specification*/
    }
    return (uint8_t) tmpDuration;
  }
  else
  {
    return 0;
  }
}


void StopSwitchDimming( uint8_t endpoint)
{
  uint8_t endpointidx = 0;
  DPRINT("StopSwitchDimming : ");
  endpointidx = FindEndPointIndex(&multiLevelEpLookup,endpoint);
  if (endpointidx == 0xFF)
    return;

  if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag & SWITCH_IS_DIMMING)
  {
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag &= ~SWITCH_IS_DIMMING;
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel = CC_MultilevelSwitch_GetCurrentValue_handler(endpoint);
    if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel)
      MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel;
  }
  DPRINTF("%d\r\n", MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel);
}


e_cmd_handler_return_code_t
HandleStartChangeCmd(uint8_t bStartLevel,
                     bool boIgnoreStartLvl,
                     bool boDimUp,
                     uint8_t bDimmingDuration,
                     uint8_t endpoint )
{

  uint8_t endpointidx = 0;
  endpointidx = FindEndPointIndex(&multiLevelEpLookup,endpoint);
  if (endpointidx == 0xFF)
  {
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }

  DPRINTF("HandleStartChangeCmd : %d %d %d %d %d %d %d\r\n", SWITCH_ENDPOINT_IDX, SWITCH_ENDPOINT_IDX, boDimUp,
          bDimmingDuration, bStartLevel, boIgnoreStartLvl, MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel);

      /*primary switch Up/Down bit field value are Up*/
  if (boDimUp)
  {
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag |= SWITCH_DIMMING_UP;
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel = 99;
  }
  else
  {
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag &= ~SWITCH_DIMMING_UP; /*we assume the up/down flag is down*/
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel = 0;
  }

  if (!boIgnoreStartLvl)
  {
    if (bStartLevel == 0xFF) /*On state*/
    {/*if we are in off state set the target level to the most recent non-zero level value*/
      bStartLevel = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel;
    }
    else if (bStartLevel == 0x00)
    {/*set off state */
      if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel)  /*we are in off state then save the current level */
        MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel;
    }

  CommandClassMultilevelSwitchSet( bStartLevel, endpoint);
  }
  MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel = CC_MultilevelSwitch_GetCurrentValue_handler(endpoint);
  DPRINT("\r\n");
  if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel != MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel)
  {
     StartLevelChange(SWITCH_ENDPOINT_IDX, bDimmingDuration, 100);
  }

  return E_CMD_HANDLER_RETURN_CODE_HANDLED;
}

e_cmd_handler_return_code_t
CC_MultilevelSwitch_SetValue(uint8_t bTargetlevel,
                             uint8_t bDuration,
                             uint8_t endpoint )
{
  uint8_t levels;

  uint8_t endpointidx = 0;
  endpointidx = FindEndPointIndex(&multiLevelEpLookup,endpoint);
  if (0xFF == endpointidx)
  {
    return E_CMD_HANDLER_RETURN_CODE_FAIL;
  }

  StopSwitchDimming(endpoint);

  DPRINTF("%s : %d ",__func__, SWITCH_ENDPOINT_IDX);
  MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel = bTargetlevel;
  DPRINTF("%d",MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel);
  MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel = CC_MultilevelSwitch_GetCurrentValue_handler(endpoint);

  if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel == 0xFF) /*On state*/
  {/*if we are in off state set the target level to the most recent non-zero level value*/
    if (!MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel)
    {
      if(MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel)
      {
        MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel;
      }
      else
      {
        MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel = 0x63; /*last level was 0. Set it to full level*/
      }
    }
    else
    {
      DPRINT("N\r\n");
      return E_CMD_HANDLER_RETURN_CODE_HANDLED; /*we are already on then ignore the on command*/
    }
  }
  else if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel == 0x00)
  {/*set off state */

    DPRINTF(" bCurrentSwitchLevel %d\r\n", MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel);
    if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel)  /*we are in off state then save the current level */
    {
      MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bOnStateSwitchLevel = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel;
    }
    else
    {
      DPRINT("%\r\n");
      return E_CMD_HANDLER_RETURN_CODE_HANDLED; /*we are already off then ignore the off command*/
    }
  }

  if (MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel < MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel)
  {
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag |= SWITCH_DIMMING_UP;
    levels = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel - MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel;

  }
  else
  {
    MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag &= ~SWITCH_DIMMING_UP;
    levels = MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bCurrentSwitchLevel - MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel;
  }
  DPRINTF(" levels %d %d\r\n", MultiLvlSwitch[SWITCH_ENDPOINT_IDX].switchFlag & SWITCH_DIMMING_UP, MultiLvlSwitch[SWITCH_ENDPOINT_IDX].bTargetSwitchLevel);
  StartLevelChange(SWITCH_ENDPOINT_IDX, bDuration, levels);

  return (bDuration > 0) ? E_CMD_HANDLER_RETURN_CODE_WORKING : E_CMD_HANDLER_RETURN_CODE_HANDLED;
}


bool SetSwitchHwLevel(uint8_t bInitHwLevel, uint8_t endpoint )
{
  uint8_t i;
  if (endpoint)
  {
    i =  FindEndPointIndex(&multiLevelEpLookup,endpoint);
    if ( i != 0xFF)
    {
      MultiLvlSwitch[i].bCurrentSwitchLevel = bInitHwLevel;
      MultiLvlSwitch[i].bOnStateSwitchLevel = bInitHwLevel;
      return true;
    }
  }
  return false;
}


void MultilevelSwitchInit(uint8_t bEndPointCount, uint8_t const * const pEndPointList)
{
  uint8_t i;
  multiLevelEpLookup.bEndPointsCount = bEndPointCount;
  multiLevelEpLookup.pEndPointList = (uint8_t *)pEndPointList;
  for (i = 0; i < bEndPointCount; i++)
  {
    MultiLvlSwitch[i].bCurrentSwitchLevel = 0;
    MultiLvlSwitch[i].bOnStateSwitchLevel = 0;
    MultiLvlSwitch[i].lTicksCount = 0;
    MultiLvlSwitch[i].switchFlag = 0;
  }
  bMultiLevelSwTimerStatus = ESWTIMER_STATUS_FAILED;
  AppTimerRegister(&MultiLevelSwTimer, true, ZCB_SwitchLevelHandler);
}

