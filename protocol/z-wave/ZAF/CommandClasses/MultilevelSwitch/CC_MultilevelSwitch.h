/**
 * @file
 * Handler for Command Class Multilevel Switch.
 *
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _CC_MULTILEVEL_SWITCH_H_
#define _CC_MULTILEVEL_SWITCH_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <ZW_classcmd.h>
#include <CC_Common.h>
#include <ZAF_types.h>
#include <ZW_TransportEndpoint.h>

/**
 * Multi-level switch type
 */
typedef enum
{
  MULTILEVEL_SWITCH_OFF_ON  = 0x01,	/**< (Direction/Endpoint A) value 0x00 = = off, (Direction/Endpoint A) value 0x63/0xFF = on)*/
  MULTILEVEL_SWITCH_DOWN_UP = 0x02,	/**< (Direction/Endpoint A) value 0x00 = = down, (Direction/Endpoint A) value 0x63/0xFF = up)*/
  MULTILEVEL_SWITCH_CLOSE_OPEN = 0x03,	/** (Direction/Endpoint A) value 0x00 = = close, (Direction/Endpoint A) value 0x63/0xFF = open)*/
  MULTILEVEL_SWITCH_COUNTER_CLOCKWISE =0x04,	/**< (Direction/Endpoint A) value 0x00 = = counter-clockwise, (Direction/Endpoint A) value 0x63/0xFF = clockwise)*/
  MULTILEVEL_SWITCH_LEFT_RIGHT = 0x05,	/**< (Direction/Endpoint A) value 0x00 = left, (Direction/Endpoint A) value 0x63/0xFF = right)*/
  MULTILEVEL_SWITCH_REVERSE_FORWARD = 0x06,	/** (Direction/Endpoint A) value 0x00 = reverse, (Direction/Endpoint A) value 0x63/0xFF = forward)*/
  MULTILEVEL_SWITCH_PULL_PUSH = 0x07	/**< (Direction/Endpoint A) value 0x00 = pull, (Direction/Endpoint A) value 0x63/0xFF = push)*/
} MULTILEVEL_SWITCH_TYPE;

/**
 * Enumeration for "Start Level Change" command.
 */
typedef enum
{
  CCMLS_PRIMARY_SWITCH_UP,              //!< CCMLS_PRIMARY_SWITCH_UP
  CCMLS_PRIMARY_SWITCH_DOWN,            //!< CCMLS_PRIMARY_SWITCH_DOWN
  CCMLS_PRIMARY_SWITCH_RESERVED,        //!< CCMLS_PRIMARY_SWITCH_RESERVED
  CCMLS_PRIMARY_SWITCH_NO_UP_DOWN_MOTION//!< CCMLS_PRIMARY_SWITCH_NO_UP_DOWN_MOTION
}
CCMLS_PRIMARY_SWITCH_T;

/**
 * Enumeration for "Start Level Change" command.
 */
typedef enum
{
  CCMLS_IGNORE_START_LEVEL_FALSE,//!< CCMLS_IGNORE_START_LEVEL_FALSE
  CCMLS_IGNORE_START_LEVEL_TRUE  //!< CCMLS_IGNORE_START_LEVEL_TRUE
}
CCMLS_IGNORE_START_LEVEL_T;

/**
 * Enumeration for "Start Level Change" command.
 */
typedef enum
{
  CCMLS_SECONDARY_SWITCH_INCREMENT,//!< CCMLS_SECONDARY_SWITCH_INCREMENT
  CCMLS_SECONDARY_SWITCH_DECREMENT,//!< CCMLS_SECONDARY_SWITCH_DECREMENT
  CCMLS_SECONDARY_SWITCH_RESERVED, //!< CCMLS_SECONDARY_SWITCH_RESERVED
  CCMLS_SECONDARY_SWITCH_NO_INC_DEC//!< CCMLS_SECONDARY_SWITCH_NO_INC_DEC
}
CCMLS_SECONDARY_SWITCH_T;

/**
 * Struct used to pass operational data to TSE module
 */
typedef struct s_CC_multilevelSwitch_data_t_
{
  RECEIVE_OPTIONS_TYPE_EX rxOptions; /**< rxOptions */
} s_CC_multilevelSwitch_data_t;

/**
 * Set the Multilevel Switch to the specified value/level.
 *
 * @param[in] bLevel The target level of the multilevel switch. The valid range is from 0 to 99.
 *        The device may not support all the 100 level. If the HW support less levels than 100
 *        then the HW levels should be distributed uniformly over the entire range.
 *        The mapping of command values to hardware levels MUST be monotonous, i.e. a higher value
 *        MUST be mapped to either the same or a higher hardware level.
 * @param[in] endpoint is the destination endpoint
 * @return none
 */
extern void CC_MultilevelSwitch_Set_handler(uint8_t bLevel, uint8_t endpoint);

/**
 * For backwards compatibility.
 */
#define CommandClassMultilevelSwitchSet(a, b) CC_MultilevelSwitch_Set_handler(a, b)

/**
 * Returns the current value for a given endpoint.
 * @param[in] endpoint Given endpoint.
 * @return Current value.
 */
extern uint8_t CC_MultilevelSwitch_GetCurrentValue_handler(uint8_t endpoint);

/**
 * For backwards compatibility.
 */
#define CommandClassMultilevelSwitchGet(a) CC_MultilevelSwitch_GetCurrentValue_handler(a)

/**
 * @brief Handler for CC Multilevel switch.
 * @param[in] rxOpt pointer receive options of type RECEIVE_OPTIONS_TYPE_EX
 * @param[in] pCmd pointer Payload from the received frame
 * @param[in] cmdLength Number of command uint8_ts including the command
 * @return receive frame status.
 */
received_frame_status_t handleCommandClassMultilevelSwitch(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  uint8_t cmdLength);


/**
 * @brief CommandClassMultilevelSwitchPrimaryTypeGet
 * Get the supported primary switch type used by the HW.
 * @param[in] endpoint is the destination endpoint
 * @return switch type
 */
extern MULTILEVEL_SWITCH_TYPE
CommandClassMultilevelSwitchPrimaryTypeGet(uint8_t endpoint);

/**
 * @brief Initiates the transmission of a "Multilevel Switch Start Level Change"
 * command.
 * @param[in] pProfile pointer to AGI profile
 * @param[in] sourceEndpoint source endpoint
 * @param[out] pCbFunc Callback function to be called when transmission is done/failed.
 * @param[in] primarySwitch Controls the primary device functionality.
 * @param[in] fIgnoreStartLevel Ignore start level.
 * @param[in] secondarySwitch Controls the secondary device functionality.
 * @param[in] primarySwitchStartLevel Start level for the primary device functionality.
 * @param[in] duration The duration from lowest to highest value.
 * @param[in] secondarySwitchStepSize Step size for secondary device functionality.
 * @return Status of the job.
 */
JOB_STATUS
CmdClassMultilevelSwitchStartLevelChange(
  AGI_PROFILE* pProfile,
  uint8_t sourceEndpoint,
  VOID_CALLBACKFUNC(pCbFunc)(TRANSMISSION_RESULT * pTransmissionResult),
  CCMLS_PRIMARY_SWITCH_T primarySwitch,
  CCMLS_IGNORE_START_LEVEL_T fIgnoreStartLevel,
  CCMLS_SECONDARY_SWITCH_T secondarySwitch,
  uint8_t primarySwitchStartLevel,
  uint8_t duration,
  uint8_t secondarySwitchStepSize);

/**
 * @brief Initiates the transmission of a "Multilevel Switch Stop Level Change"
 * command.
 * @param[in] pProfile pointer to AGI profile
 * @param[in] sourceEndpoint source endpoint
 * @param[out] pCbFunc Callback function to be called when transmission is done/failed.
 * @return Status of the job.
 */
JOB_STATUS
CmdClassMultilevelSwitchStopLevelChange(
  AGI_PROFILE* pProfile,
  uint8_t sourceEndpoint,
  VOID_CALLBACKFUNC(pCbFunc)(TRANSMISSION_RESULT * pTransmissionResult));

/**
 * @brief Initiates the transmission of a "Multilevel Switch Set" command.
 * @param[in] pProfile pointer to AGI profile
 * @param[in] sourceEndpoint source endpoint
 * @param[out] pCbFunc Callback function to be called when transmission is done/failed.
 * @param[in] value Multilevel value.
 * @param[in] duration The duration from current value to the new given value.
 * @return Status of the job.
 */
JOB_STATUS
CmdClassMultilevelSwitchSetTransmit(
  AGI_PROFILE* pProfile,
  uint8_t sourceEndpoint,
  VOID_CALLBACKFUNC(pCbFunc)(TRANSMISSION_RESULT * pTransmissionResult),
  uint8_t value,
  uint8_t duration);

/**
 * Returns the factory default dimming duration used when handling Multilevel Switch Set or Start
 * Level Change commands.
 *
 * NOTICE: This function must be implemented by the application.
 *
 * Please see Table 7 in SDS13781 for information on how the duration is encoded.
 * @param[in] boIsSetCmd If set to true, the function must return the dimming duration used for
 * the Multilevel Switch Set command.
 * If set to false, the function must return the dimming duration used for the Multilevel Switch
 * Start Level Change command.
 * These values can be identical.
 * @param[in] endpoint Endpoint destination.
 * @return The endpoint's default dimming duration.
 *
 */
extern uint8_t GetFactoryDefaultDimmingDuration(bool boIsSetCmd, uint8_t endpoint);

/**
 * Send report when change happen via lifeLine.
 *
 * Callback used by TSE module. Refer to @ref ZAF_TSE.h for more details.
 *
 * @param txOptions txOptions
 * @param pData Command payload for the report
 */
void CC_MultilevelSwitch_report_stx(TRANSMIT_OPTIONS_TYPE_SINGLE_EX txOptions, s_CC_multilevelSwitch_data_t* pData);

/**
 * This function is used to notify the Application that the CC MultilevelSwitch Set
 * status is in a WORKING state. The application can subsequently make the TSE Trigger
 * using the information passed on from the rxOptions.
 * @param pRxOpt pointer used to when triggering the "working state"
 * @param duration Time in seconds needed to complete dimming. If duration is 0, then value of 10 ms will be used
 */
extern void CC_MultilevelSwitch_report_notifyWorking(RECEIVE_OPTIONS_TYPE_EX *pRxOpt, uint8_t duration);

/**
 * Prepare the data input for the TSE for any Multilevel Switch CC command based on the pRxOption pointer.
 * @param pRxOpt pointer used to indicate how the frame was received (if any) and what endpoints are affected
*/
extern void* CC_MultilevelSwitch_prepare_zaf_tse_data(RECEIVE_OPTIONS_TYPE_EX* pRxOpt);

#endif
