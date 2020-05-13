/**
 * @file
 * @brief Framework for application's handlers used by CC and Application Utilities.
 * @copyright 2018 Silicon Laboratories Inc.
 */

#ifndef _ZAF_COMMON_IF_H_
#define _ZAF_COMMON_IF_H_

#include <ZAF_CmdPublisher.h>

// setters

/**
 * Load relevant data from application to ZAF framework to make it accessible for CC-s
 *
 * @param pAppTaskHandle Task handle
 * @param pAppHandle Application handle
 * @param pAppProtocolConfig Protocol configuration
 */
void ZAF_setApplicationData(TaskHandle_t pAppTaskHandle,
                            SApplicationHandles* pAppHandle,
                            const SProtocolConfig_t * pAppProtocolConfig
                            );

/**
 * Pass an application handle to the ZAF framework with pointers to application resources. Can be later used by CC-s.
 * @param pAppHandle pointer to Application Handle in application
 */
void ZAF_setAppHandle(SApplicationHandles* pAppHandle);

/**
 * Pass an application power lock handle to the ZAF framework. Can be later used by CC-s.
 * Used for Power management in case of wake up of FLiRS nodes.
 *
 * @param powerLock pointer to power lock object in application
 */
void ZAF_setPowerLock(SPowerLock_t* powerLock);

/**
 * Pass an application protocol configuration handle to the ZAF framework. Can be later used by CC-s.
 * @param pProtocolConfig pointer to Protocol Configuration in application
 */
void ZAF_setAppProtocolConfig(SProtocolConfig_t* pProtocolConfig);

/**
 * Set the CP context. The application should use this an initialization,
 * so the ZAF_Common interface can redistribute the pContext address subsequently
 *
 * @param handle to set
 */
void ZAF_SetCPHandle(CP_Handle_t handle);


// Getters
/**
 * Get m_AppTaskHandle object
 * @return m_AppTaskHandle
 */
TaskHandle_t ZAF_getAppTaskHandle();

/**
 * Get m_pAppHandles object
 * @return m_pAppHandles
 */
SApplicationHandles* ZAF_getAppHandle();

/**
 * Get Notifying queue object. It is stored in m_pAppHandles
 * @return pZwTxQueue member of m_pAppHandles
 */
SQueueNotifying* ZAF_getZwTxQueue();

/**
 * Get Application Node Info object from m_pAppProtocolConfig
 * @return pNodeInfo member of m_pAppProtocolConfig
 */
const SAppNodeInfo_t* ZAF_getAppNodeInfo();

/**
 * Get m_pAppProtocolConfig object
 * @return m_pAppProtocolConfig
 */
const SProtocolConfig_t* ZAF_getAppProtocolConfig();

/**
 * Get m_PowerLock object
 * @return m_PowerLock
 */
SPowerLock_t* ZAF_getPowerLock();

/**
 * Returns the granted security keys.
 *
 * Refer to ZW_security_api.h for the security key masks.
 * @return 8 bit mask where a set bit indicates that a security key is granted.
 */
uint8_t ZAF_GetSecurityKeys(void);


/**
 * Get Command Publisher handle
 * Any other module can request the address of the CP context to use the API
 * @return Pointer to Command Publisher context
 */
CP_Handle_t ZAF_getCPHandle();


/**
 * Check if device is FLiRS based on device option mask
 *
 * @param pAppNodeInfo
 * @return True or false, based on pAppNodeInfo
 */
bool isFLiRS(const SAppNodeInfo_t * pAppNodeInfo);



#endif /*_ZAF_COMMON_IF_H_ */
