/**
 * @file
 * Multilevel switch helper module.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _MULTILEVEL_SWITCH_H_
#define _MULTILEVEL_SWITCH_H_

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/

#include <ZAF_types.h>

/****************************************************************************/
/*                     EXPORTED TYPES and DEFINITIONS                       */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

// Nothing here.

/****************************************************************************/
/*                           EXPORTED FUNCTIONS                             */
/****************************************************************************/


/**
 * @brief GetSwitchHWLevel
 *  Read the switch actual HW level
 * @param[in] endpoint is the destination endpoint
 * @return The current device HW level.
 *
 */
uint8_t GetSwitchHWLevel(uint8_t endpoint);

/**
 * @brief StopSwitchDimming
 * Stop the ongoing switch dimming if it running
 * @param[in] endpoint is the destination endpoint
 */
void StopSwitchDimming(uint8_t endpoint);


/**
 * @brief HandleStartChangeCmd
 *  Handle the multilevel switch start change command
 * @param[in] bStartLevel IN the the switch level the dimming should start from.
 * @param[in] boIgnoreStartLvl IN true if bStartLevel should be ignored false if bStartLevel should be used.
 * @param[in] boDimUp IN true if switch level should be incremented, false if the level should be decremented.
 * @param[in] bDimmingDuration IN the time it should take the switch to transit from 0 to 99
 * The encoding of the value is as follow:
 * 0x00  Instantly
 * 0x01-0x7F 1 second (0x01) to 127 seconds (0x7F) in 1-second resolution.
 * 0x80-0xFE 1 minute (0x80) to 127 minutes (0xFE) in 1-minute resolution.
 * 0xFF Factory default duration.
 * @param endpoint is the destination endpoint
 * @return Success if Level Change started or device is already at final state
 *         Fail if function is called with invalid parameters
 */
e_cmd_handler_return_code_t HandleStartChangeCmd(uint8_t bStartLevel,
                     bool boIgnoreStartLvl,
                     bool boDimUp,
                     uint8_t bDimmingDuration,
                     uint8_t endpoint );

/**
 * @brief Handle the multilevel switch set command
 * @param[in] bTargetlevel IN the wanted switch level.
 * @param[in] bDuration IN the time it should take the switch to go from current level to bTargetlevel
 * The encoding of the value is as follow:
 * 0x00  Instantly
 * 0x01-0x7F 1 second (0x01) to 127 seconds (0x7F) in 1-second resolution.
 * 0x80-0xFE 1 minute (0x80) to 127 minutes (0xFE) in 1-minute resolution.
 * 0xFF Factory default duration.
 * @param[in] endpoint is the destination endpoint
 * @return command handler return code
 */
e_cmd_handler_return_code_t CC_MultilevelSwitch_SetValue(uint8_t bTargetlevel, uint8_t bDuration, uint8_t endpoint);

/**
 * @brief SetSwitchHwLevel
 * Set the intial a multilevel switch hw level
 * The number of endpoints that can be initailsed are defiend by the constant SWITCH_MULTI_ENDPOINTS
 * @param[in]  bInitHwLevel IN a multilevel switch initiale HW level value.
 * @param[in]  endpoint IN multilevel switch endpoint ID, Endpoint 0 is invalid.
 * @return True if the endpoint initalised correctly, false if the endpoint not initalised
 */
bool SetSwitchHwLevel(uint8_t bInitHwLevel, uint8_t endpoint);

/**
 * @brief Initializes the Multilevel Switch command class by telling it which endpoints are
 * capable of handling Multilevel Switch commands.
 *
 * This function must be called once by the application.
 * @param[in] bEndPointCount Number of multilevel switch endpoints.
 * @param[in] pEndPointList Pointer to a list of endpoints.
 */
void MultilevelSwitchInit(uint8_t bEndPointCount, uint8_t const * const pEndPointList);

/**
 * @brief GetOnStateLevel
 * returns the on state level value when the HW is set to off
 * @param[in] endpoint number of multilevel switch endpoints
 */
uint8_t
GetOnStateLevel(uint8_t endpoint);

/**
 * @brief GetTargetLevel
 * returns the target level of the ongoing or the most recent transision
 * @param[in] endpoint number of multilevel switch endpoints
 */
uint8_t
GetTargetLevel(uint8_t endpoint);

/**
 * @brief GetTargetLevel
 * Returns the duration the HW takes to reach the target level from the current level
 * The duration is zero if the target level was reached
 * @param[in] endpoint number of multilevel switch endpoints
 * @return the duration encoded as fellow 0x01-0x7F 1 second (0x01) to 127 seconds (0x7F) in 1 second resolution.
 *         0x80-0xFD 1 minute (0x80) to 126 minutes (0xFD) in 1 minute resolution.
 *         0xFE Unknown duration.
 */
uint8_t
GetCurrentDuration(uint8_t endpoint);

#endif /*_MULTILEVEL_SWITCH_H_*/

