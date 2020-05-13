/***************************************************************************//**
 * @brief Core Connect stack functions
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include PLATFORM_HEADER

// The public APIs in this file should always be renamed.
#if defined(SKIP_API_RENAME)
#undef SKIP_API_RENAME
#endif

#include "stack/include/ember.h"
#include "hal/hal.h"

// Frequency Hopping

#if (!defined(EMBER_AF_PLUGIN_FREQUENCY_HOPPING) || defined(EMBER_TEST))

WEAK(EmberStatus emGetFrequencyHoppingLibraryStatus(void))
{
  return EMBER_LIBRARY_IS_STUB;
}

WEAK(uint8_t emFrequencyHoppingGetCurrentChannelIndex(void))
{
  return 0;
}

WEAK(uint32_t emFrequencyHoppingGetCurrentTimestampUs(void))
{
  return 0;
}

WEAK(bool emFrequencyHoppingOngoing(void))
{
  return false;
}

WEAK(void emMarkFrequencyHoppingBuffers(void))
{
}

WEAK(bool emFrequencyHoppingMacActivityHandler(void))
{
  return true;
}

WEAK(void emFrequencyHoppingEventHandler(void))
{
}

WEAK(void emIncomingMacFrequencyHoppingInfoRequestHandler(EmberNodeId sourceNodeId,
                                                          EmberPanId sourcePanId,
                                                          uint8_t control))
{
  (void)sourceNodeId;
  (void)sourcePanId;
  (void)control;
}

WEAK(void emMacFrequencyHoppingInfoRequestSentHandler(EmberStatus status))
{
  (void)status;
}

WEAK(void emIncomingMacFrequencyHoppingInfoHandler(EmberNodeId sourceNodeId,
                                                   EmberPanId sourcePanId,
                                                   uint8_t control,
                                                   uint16_t seed,
                                                   uint8_t channelIndex,
                                                   uint32_t timestamp))
{
  (void)sourceNodeId;
  (void)sourcePanId;
  (void)control;
  (void)seed;
  (void)channelIndex;
  (void)timestamp;
}

WEAK(void emMacFrequencyHoppingInfoSentHandler(EmberStatus status))
{
  (void)status;
}

WEAK(bool emFrequencyHoppingClientMaybeResync(ClientResyncCallback callback))
{
  (void)callback;

  return false;
}

WEAK(EmberStatus emberFrequencyHoppingStartServer(void))
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberStatus emberFrequencyHoppingStartClient(EmberNodeId serverNodeId,
                                                  EmberPanId serverPanId))
{
  (void)serverNodeId;
  (void)serverPanId;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberStatus emberFrequencyHoppingStop(void))
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

#endif // !EMBER_AF_PLUGIN_FREQUENCY_HOPPING || EMBER_TEST

// Parent Support

#if (!defined(EMBER_AF_PLUGIN_STACK_PARENT_SUPPORT) || defined(EMBER_TEST))

extern EmberNodeId emNewChildShortId;

WEAK(EmberStatus emGetParentSupportLibraryStatus(void)) {
  return EMBER_LIBRARY_IS_STUB;
}

WEAK(EmberStatus emberRemoveChild(EmberMacAddress *address))
{
  (void)address;

  return EMBER_INVALID_CALL;
}

WEAK(EmberStatus emParentSubmit(EmberNodeId macSource,
                                EmberNodeId nwkSource,
                                EmberNodeId nwkDestination,
                                uint8_t endpoint,
                                uint8_t packetTag,
                                EmberMessageLength packetLength,
                                uint8_t *packet,
                                EmberMessageOptions options))
{
  (void)macSource;
  (void)nwkSource;
  (void)nwkDestination;
  (void)endpoint;
  (void)packetTag;
  (void)packetLength;
  (void)packet;
  (void)options;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberNodeId emParentGetNewChildId(void))
{
  if (emNewChildShortId == EMBER_NULL_NODE_ID) {
    return halCommonGetRandom();
  } else {
    return emNewChildShortId;
  }
}

WEAK(void emParentInitiateShortAddressRequest(EmberEUI64 longId,
                                              EmberNodeId requestedNodeId))
{
  (void)longId;

  emNewChildShortId = requestedNodeId;
}

WEAK(void emParentEventHandler(void))
{
}

WEAK(void emNwkIncomingRangeExtenderUpdateRequestHandler(EmberMessageOptions options))
{
  (void)options;
}

WEAK(void emNwkIncomingShortAddressResponseHandler(EmberNodeId allocatedId,
                                                   EmberMessageOptions options))
{
  (void)allocatedId;
  (void)options;
}

WEAK(bool emPendingRemovalChildHandler(uint8_t childIndex, bool rxIsDataPoll))
{
  (void)childIndex;
  (void)rxIsDataPoll;

  return false;
}

WEAK(void emNwkLeaveRequestSentHandler(EmberStatus status,
                                       EmberNodeId destination))
{
  (void)status;
  (void)destination;
}

WEAK(void emNwkIncomingLeaveNotificationHandler(EmberNodeId source,
                                                EmberMessageOptions options))
{
  (void)source;
  (void)options;
}

// Indirect Queue

WEAK(void emIndirectQueueInit(void))
{
}

WEAK(void emMarkIndirectQueueBuffers(void))
{
}

WEAK(void emIndirectQueueEventHandler(void))
{
}

WEAK(void emIncomingMacDataRequestIndirectQueueHandler(uint8_t *packet))
{
  (void)packet;
}

WEAK(bool emIndirectQueueIsEmpty(void))
{
  return true;
}

WEAK(bool emIndirectQueueLookupByAddress(EmberMacAddress *destAddr))
{
  (void)destAddr;

  return false;
}

WEAK(EmberStatus emIndirectQueueAddPacket(EmberNodeId source,
                                          EmberNodeId destination,
                                          uint8_t endpoint,
                                          uint8_t packetTag,
                                          uint8_t *packet,
                                          EmberMessageLength packetLength,
                                          EmberMessageOptions options))
{
  (void)source;
  (void)destination;
  (void)endpoint;
  (void)packetTag;
  (void)packet;
  (void)packetLength;
  (void)options;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberStatus emberPurgeIndirectMessages(void))
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberStatus emberSetIndirectQueueTimeout(uint32_t timeoutMs))
{
  (void)timeoutMs;

  return EMBER_LIBRARY_NOT_PRESENT;
}

// Child Table

WEAK(EmberStatus emberGetChildFlags(EmberMacAddress *address,
                                    EmberChildFlags *flags))
{
  (void)address;
  (void)flags;

  return EMBER_INVALID_CALL;
}

WEAK(void emChildTableInit(void))
{
}

WEAK(void emChildTableEventHandler(void))
{
}

WEAK(void emChildTableLoadFromToken(void))
{
}

WEAK(uint8_t emChildTableGetUnusedEntryIndex(void))
{
  return 0;
}

WEAK(uint8_t emChildTableLookUp(EmberNodeId shortId,
                                EmberEUI64 longId))
{
  (void)shortId;
  (void)longId;

  return 0xFF;
}

WEAK(EmberStatus emChildTableAddOrUpdateEntry(EmberNodeId shortId,
                                              EmberEUI64 longId,
                                              uint8_t macCapabilities))
{
  (void)shortId;
  (void)longId;
  (void)macCapabilities;

  return EMBER_SUCCESS;
}

WEAK(EmberStatus emChildTableRemoveEntry(EmberNodeId shortId,
                                         EmberEUI64 longId))
{
  (void)shortId;
  (void)longId;

  return EMBER_SUCCESS;
}

WEAK(bool emChildTableDataPending(EmberNodeId shortId,
                                  EmberEUI64 longId))
{
  (void)shortId;
  (void)longId;

  return false;
}

WEAK(bool emChildTableSetSecurityTxOptions(EmberNodeId shortId,
                                           EmberMessageOptions *options))
{
  (void)shortId;
  (void)options;

  return false;
}

WEAK(void emChildTableNotifyActivity(uint8_t *packet, bool incoming))
{
  (void)packet;
  (void)incoming;
}

WEAK(bool emChildTableCheckAndUpdateFrameCounter(EmberNodeId shortId,
                                                 uint32_t frameCounter))
{
  (void)shortId;
  (void)frameCounter;

  return false;
}

WEAK(EmberNodeId emGetChildShortId(uint8_t childIndex))
{
  (void)childIndex;

  return 0xFFFF;
}

// Coordinator Support

WEAK(void emCoordinatorInit(void))
{
}

WEAK(void emMarkCoordinatorBuffers(void))
{
}

WEAK(EmberNodeId emCoordinatorCheckOrAllocateShortId(EmberNodeId requestedNodeId))
{
  (void)requestedNodeId;

  return EMBER_NULL_NODE_ID;
}

WEAK(void emCoordinatorResetLastAssignedId(void))
{
}

WEAK(void emChildTableEntryChangedHandler(uint8_t childTableIndex))
{
  (void)childTableIndex;
}

WEAK(void emNwkIncomingShortAddressRequestHandler(EmberNodeId source,
                                                  EmberNodeId requestedShortId,
                                                  EmberEUI64 endDeviceLongId,
                                                  EmberMessageOptions options))
{
  (void)source;
  (void)requestedShortId;
  (void)endDeviceLongId;
  (void)options;
}

WEAK(void emNwkIncomingRangeExtenderUpdateHandler(EmberNodeId source,
                                                  EmberMessageOptions options,
                                                  uint8_t *shortIdList,
                                                  uint8_t shortIdListLengthBytes))
{
  (void)source;
  (void)options;
  (void)shortIdList;
  (void)shortIdListLengthBytes;
}

#endif // !EMBER_AF_PLUGIN_STACK_PARENT_SUPPORT || EMBER_TEST

// Mac Queue

#if (!defined(EMBER_AF_PLUGIN_STACK_MAC_QUEUE) || defined(EMBER_TEST))

WEAK(EmberStatus emGetMacQueueLibraryStatus(void)) {
  return EMBER_LIBRARY_IS_STUB;
}

WEAK(void emMacQueueInit(void))
{
}

WEAK(void emMarkMacQueueBuffers(void))
{
}

WEAK(EmberStatus emMacOutgoingQueueSubmit(EmberNodeId destination,
                                          uint8_t tag,
                                          uint8_t headerLength,
                                          uint8_t *header,
                                          EmberMessageLength payloadLength,
                                          uint8_t *payload,
                                          EmberMessageOptions options))
{
  (void)destination;
  (void)tag;
  (void)headerLength;
  (void)header;
  (void)payloadLength;
  (void)payload;
  (void)options;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(bool emMacOutgoingDequeue(EmberOutgoingMessage *packet))
{
  (void)packet;

  return false;
}

WEAK(bool emMacOutgoingQueueIsEmpty(void))
{
  return true;
}

WEAK(EmberStatus emMacIncomingQueueSubmit(EmberMessageLength payloadLength,
                                          uint8_t *payload,
                                          int8_t rssi))
{
  (void)payloadLength;
  (void)payload;
  (void)rssi;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(bool emMacIncomingDequeue(EmberOutgoingMessage *packet))
{
  (void)packet;

  return false;
}

WEAK(bool emMacIncomingQueueIsEmpty(void))
{
  return true;
}

#endif // !EMBER_AF_PLUGIN_STACK_MAC_QUEUE || EMBER_TEST

// Stack counter

#if (!defined(EMBER_AF_PLUGIN_STACK_COUNTERS) || defined(EMBER_TEST))

WEAK(void emCountersInit(void))
{
}

WEAK(void emCounterHandler(EmberCounterType counterType, uint8_t count))
{
  (void)counterType;
  (void)count;
}

WEAK(EmberStatus emberGetCounter(EmberCounterType counterType, uint32_t *count))
{
  (void)counterType;
  (void)count;

  return EMBER_LIBRARY_NOT_PRESENT;
}

#endif // !EMBER_AF_PLUGIN_STACK_COUNTERS || EMBER_TEST

// AES security

#if (!defined(EMBER_AF_PLUGIN_STACK_AES_SECURITY) || defined(EMBER_TEST))

WEAK(void emSecurityAesInit(void))
{
}

WEAK(uint8_t emGetSecurityPaddingLengthAes(EmberMessageLength payloadLength))
{
  (void)payloadLength;

  return 0;
}

WEAK(void emAddAuxiliaryMacHeaderAes(uint8_t *finger,
                                     EmberMessageLength payloadLength,
                                     EmberMessageOptions options))
{
  (void)finger;
  (void)payloadLength;
  (void)options;
}

WEAK(EmberStatus emEncryptAndSignPacketAes(uint8_t *packet))
{
  (void)packet;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberStatus emDecryptAndAuthenticatePacketAes(uint8_t *packet))
{
  (void)packet;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberLibraryStatus emGetAesSecurityLibraryStatus(void))
{
  return EMBER_LIBRARY_IS_STUB;
}

#endif // !EMBER_AF_PLUGIN_STACK_AES_SECURITY || EMBER_TEST

#if (!defined(EMBER_AF_PLUGIN_STACK_XXTEA_SECURITY) || defined(EMBER_TEST))

WEAK(void emSecurityXxteaInit(void))
{
}

WEAK(uint8_t emGetSecurityPaddingLengthXxtea(EmberMessageLength payloadLength))
{
  (void)payloadLength;

  return 0;
}

WEAK(void emAddAuxiliaryMacHeaderXxtea(uint8_t *finger,
                                       EmberMessageLength payloadLength,
                                       EmberMessageOptions options))
{
  (void)finger;
  (void)payloadLength;
  (void)options;
}

WEAK(EmberStatus emEncryptAndSignPacketXxtea(uint8_t *packet))
{
  (void)packet;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberStatus emDecryptAndAuthenticatePacketXxtea(uint8_t *packet))
{
  (void)packet;

  return EMBER_LIBRARY_NOT_PRESENT;
}

WEAK(EmberLibraryStatus emGetXxteaSecurityLibraryStatus(void))
{
  return EMBER_LIBRARY_IS_STUB;
}

#endif // !EMBER_AF_PLUGIN_STACK_XXTEA_SECURITY || EMBER_TEST
