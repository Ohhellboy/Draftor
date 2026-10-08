#include "hanime_ble_ownership.hpp"
/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 * Copyright 2017 Neil Kolban
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * BLERemoteDescriptor.cpp
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "WString.h"
#include <sstream>
#include "BLERemoteDescriptor.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"
#include <inttypes.h>

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <atomic>
#include <freertos/task.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include <host/ble_hs.h>
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLERemoteDescriptor::~BLERemoteDescriptor() { --hanime_ble::live.descriptors; }

BLERemoteDescriptor::BLERemoteDescriptor(uint16_t handle, BLEUUID uuid, BLERemoteCharacteristic *pRemoteCharacteristic) {
  ++hanime_ble::live.descriptors;
  m_handle = handle;
  m_uuid = uuid;
  m_pRemoteCharacteristic = pRemoteCharacteristic;
  m_auth = 0;
}

/**
 * @brief Retrieve the handle associated with this remote descriptor.
 * @return The handle associated with this remote descriptor.
 */
uint16_t BLERemoteDescriptor::getHandle() {
  return m_handle;
}  // getHandle

/**
 * @brief Get the characteristic that owns this descriptor.
 * @return The characteristic that owns this descriptor.
 */
BLERemoteCharacteristic *BLERemoteDescriptor::getRemoteCharacteristic() {
  return m_pRemoteCharacteristic;
}  // getRemoteCharacteristic

/**
 * @brief Retrieve the UUID associated this remote descriptor.
 * @return The UUID associated this remote descriptor.
 */
BLEUUID BLERemoteDescriptor::getUUID() {
  return m_uuid;
}  // getUUID

uint8_t BLERemoteDescriptor::readUInt8() {
  String value = readValue();
  if (value.length() >= 1) {
    return (uint8_t)value[0];
  }
  return 0;
}  // readUInt8

uint16_t BLERemoteDescriptor::readUInt16() {
  String value = readValue();
  if (value.length() >= 2) {
    return *(uint16_t *)value.c_str();
  }
  return 0;
}  // readUInt16

uint32_t BLERemoteDescriptor::readUInt32() {
  String value = readValue();
  if (value.length() >= 4) {
    return *(uint32_t *)value.c_str();
  }
  return 0;
}  // readUInt32

/**
 * @brief Return a string representation of this BLE Remote Descriptor.
 * @return A string representation of this BLE Remote Descriptor.
 */
String BLERemoteDescriptor::toString() {
  char val[6];
  snprintf(val, sizeof(val), "%u", getHandle());
  String res = "handle: ";
  res += val;
  res += ", uuid: " + getUUID().toString();
  return res;
}  // toString

/**
 * @brief Write data represented as a string to the BLE Remote Descriptor.
 * @param [in] newValue The data to send to the remote descriptor.
 * @param [in] response True if we expect a response.
 */
bool BLERemoteDescriptor::writeValue(String newValue, bool response) {
  return writeValue((uint8_t *)newValue.c_str(), newValue.length(), response);
}  // writeValue

/**
 * @brief Write a byte value to the Descriptor.
 * @param [in] The single byte to write.
 * @param [in] True if we expect a response.
 */
bool BLERemoteDescriptor::writeValue(uint8_t newValue, bool response) {
  return writeValue(&newValue, 1, response);
}  // writeValue

/**
 * @brief Set authentication request type for characteristic
 * @param [in] auth Authentication request type.
 */
void BLERemoteDescriptor::setAuth(uint8_t auth) {
  m_auth = auth;
}

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

void BLERemoteDescriptor::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam) {
  switch (event) {
    // ESP_GATTC_READ_DESCR_EVT
    // This event indicates that the server has responded to the read request.
    //
    // read:
    // - esp_gatt_status_t  status
    // - uint16_t           conn_id
    // - uint16_t           handle
    // - uint8_t*           value
    // - uint16_t           value_len
    case ESP_GATTC_READ_DESCR_EVT:
      // If this event is not for us, then nothing further to do.
      if (evtParam->read.handle != getHandle()) {
        break;
      }
      // At this point, we have determined that the event is for us, so now we save the value
      if (evtParam->read.status == ESP_GATT_OK) {
        // it will read the cached value of the descriptor
        m_value = String((char *)evtParam->read.value, evtParam->read.value_len);
      } else {
        m_value = "";
      }
      // Unlock the semaphore to ensure that the requester of the data can continue.
      // Pass the GATT status through the semaphore value for retry logic.
      m_semaphoreReadDescrEvt.give(evtParam->read.status);
      break;

    case ESP_GATTC_WRITE_DESCR_EVT:
      if (evtParam->write.handle != getHandle()) {
        break;
      }
      m_semaphoreWriteDescrEvt.give(evtParam->write.status);
      break;
    default: break;
  }
}

String BLERemoteDescriptor::readValue() {
  log_v(">> readValue: %s", toString().c_str());

  BLEClient *pClient = getRemoteCharacteristic()->getRemoteService()->getClient();

  // Check to see that we are connected.
  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return String();
  }

  // Wait for authentication to complete if security was started on connection
  BLESecurity::waitForAuthenticationComplete();

  int retryCount = 1;
  uint32_t status = ESP_GATT_OK;

  do {
    m_semaphoreReadDescrEvt.take("readValue");

    // Ask the BLE subsystem to retrieve the value for the remote hosted descriptor.
    esp_err_t errRc = ::esp_ble_gattc_read_char_descr(
      pClient->getGattcIf(),
      pClient->getConnId(),  // The connection ID to the BLE server
      getHandle(),           // The handle of this descriptor
      (esp_gatt_auth_req_t)m_auth
    );  // Security

    if (errRc != ESP_OK) {
      log_e("esp_ble_gattc_read_char_descr: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      m_semaphoreReadDescrEvt.give();
      return "";
    }

    // Block waiting for the event that indicates that the read has completed.
    // The GATT status is passed through the semaphore value.
    status = m_semaphoreReadDescrEvt.wait("readValue");

    switch (status) {
      case ESP_GATT_OK: break;
      case ESP_GATT_INSUF_AUTHENTICATION:
      case ESP_GATT_INSUF_AUTHORIZATION:
      case ESP_GATT_INSUF_ENCRYPTION:
        if (BLESecurity::m_securityEnabled && retryCount && pClient->secureConnection()) {
          break;
        }
      /* Else falls through. */
      default: log_e("readValue: status=%d", status); break;
    }
  } while (status != ESP_GATT_OK && retryCount--);

  log_v("<< readValue(): length: %u", m_value.length());
  return m_value;
}  // readValue

/**
 * @brief Write data to the BLE Remote Descriptor.
 * @param [in] data The data to send to the remote descriptor.
 * @param [in] length The length of the data to send.
 * @param [in] response True if we expect a response.
 * @return True if successful
 */
bool BLERemoteDescriptor::writeValue(uint8_t *data, size_t length, bool response) {
  log_v(">> writeValue: %s", toString().c_str());

  BLEClient *pClient = getRemoteCharacteristic()->getRemoteService()->getClient();

  // Check to see that we are connected.
  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return false;
  }

  // Wait for authentication to complete if security was started on connection
  BLESecurity::waitForAuthenticationComplete();

  int retryCount = 1;
  uint32_t status = ESP_GATT_OK;

  do {
    m_semaphoreWriteDescrEvt.take("writeValue");

    esp_err_t errRc = ::esp_ble_gattc_write_char_descr(
      pClient->getGattcIf(), pClient->getConnId(), getHandle(),
      length,  // Data length
      data,    // Data
      response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, (esp_gatt_auth_req_t)m_auth
    );

    if (errRc != ESP_OK) {
      log_e("esp_ble_gattc_write_char_descr: %d", errRc);
      m_semaphoreWriteDescrEvt.give();
      return false;
    }

    // Block waiting for the event that indicates that the write has completed.
    // The GATT status is passed through the semaphore value.
    status = m_semaphoreWriteDescrEvt.wait("writeValue");

    switch (status) {
      case ESP_GATT_OK: break;
      case ESP_GATT_INSUF_AUTHENTICATION:
      case ESP_GATT_INSUF_AUTHORIZATION:
      case ESP_GATT_INSUF_ENCRYPTION:
        if (BLESecurity::m_securityEnabled && retryCount && pClient->secureConnection()) {
          break;
        }
      /* Else falls through. */
      default: log_e("writeValue: status=%d", status); break;
    }
  } while (status != ESP_GATT_OK && retryCount--);

  log_v("<< writeValue");
  return (status == ESP_GATT_OK);
}  // writeValue

#endif

/***************************************************************************
 *                           NimBLE functions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief Remote descriptor constructor.
 * @param [in] pRemoteCharacteristic A pointer to the Characteristic that this belongs to.
 * @param [in] dsc A pointer to the struct that contains the descriptor information.
 */
BLERemoteDescriptor::BLERemoteDescriptor(BLERemoteCharacteristic *pRemoteCharacteristic, const struct ble_gatt_dsc *dsc) {
  ++hanime_ble::live.descriptors;
  log_d(">> BLERemoteDescriptor()");
  switch (dsc->uuid.u.type) {
    case BLE_UUID_TYPE_16:  m_uuid = BLEUUID(dsc->uuid.u16.value); break;
    case BLE_UUID_TYPE_32:  m_uuid = BLEUUID(dsc->uuid.u32.value); break;
    case BLE_UUID_TYPE_128: m_uuid = BLEUUID(const_cast<ble_uuid128_t *>(&dsc->uuid.u128)); break;
    default:                break;
  }

  m_handle = dsc->handle;
  m_pRemoteCharacteristic = pRemoteCharacteristic;
  m_auth = 0;

  log_d("<< BLERemoteDescriptor(): %s", m_uuid.toString().c_str());
}

/**
 * @brief Read the value of the remote descriptor.
 * @return The value of the remote descriptor.
 */
// DRAFTOR_DESCRIPTOR_CONTEXT_BEGIN
// ble_hs_is_parent_task is an exported symbol in the pinned IDF NimBLE host.
// Synchronous reads must never wait inside the host callback task itself.
extern "C" int ble_hs_is_parent_task(void);
extern "C" void draftor_ble_read_event(uint16_t, uint16_t, uint8_t, int, uint16_t) __attribute__((weak));
namespace {
#ifndef CONFIG_NIMBLE_FREERTOS_TASK_BLOCK_BIT
#define CONFIG_NIMBLE_FREERTOS_TASK_BLOCK_BIT 31
#endif
constexpr uint32_t DescriptorReadNotifyBit = uint32_t{1} << CONFIG_NIMBLE_FREERTOS_TASK_BLOCK_BIT;
struct DescriptorReadContext {
  String* value;
  TaskHandle_t waiter;
  uint16_t connection;
  bool longRead;
  std::atomic<bool> complete{false};
  int status = 0;
};
void descriptorReadEvent(uint16_t connection, uint16_t handle, uint8_t stage, int status, size_t bytes) {
  if (draftor_ble_read_event) draftor_ble_read_event(connection, handle, stage, status, uint16_t(bytes));
}
int finishDescriptorRead(DescriptorReadContext& request, int status) {
  request.status = status;
  // Notify first. Publication is the callback's LAST request access; the
  // caller may destroy its stack immediately after observing complete.
  xTaskNotify(request.waiter, DescriptorReadNotifyBit, eSetBits);
  request.complete.store(true, std::memory_order_release);
  return status;
}
void waitDescriptorRead(DescriptorReadContext& request) {
  // Notification can wake us before the final publication. A short bounded
  // wait only rechecks that publication; it NEVER abandons an active request.
  // SDK ATT timeout / disconnect still supplies the terminal callback.
  while (!request.complete.load(std::memory_order_acquire)) {
    uint32_t ignored = 0;
    xTaskNotifyWait(0, DescriptorReadNotifyBit, &ignored, pdMS_TO_TICKS(10) ? pdMS_TO_TICKS(10) : 1);
  }
  // complete acquires the callback's LAST context access, after its notify.
  // A terminal delivered before the loop (or between wait and recheck) still
  // owns a pending notification. Consume only this BLE bit, without waiting,
  // before this worker can submit a retry/security/next BLE operation using
  // the same notification slot. The pinned keyboard worker serializes slot-0
  // BLE users: no next operation is submitted yet. This consumes pending state;
  // it preserves other value bits, not another owner's pending-state contract.
  xTaskNotifyWait(0, DescriptorReadNotifyBit, nullptr, 0);
}
}  // namespace
// DRAFTOR_DESCRIPTOR_CONTEXT_END

String BLERemoteDescriptor::readValue() {
  BLEClient* client = getRemoteCharacteristic()->getRemoteService()->getClient();
  String value;
  if (ble_hs_is_parent_task() || !client->isConnected()) return value;
  const uint16_t connection = client->getConnId();
  bool longRead = true;
  unsigned securityRetries = 1;
  int rc = 0;
  for (;;) {
    if (!client->isConnected() || client->getConnId() != connection) return String();
    value = String();  // A retry/fallback is a fresh value, not appended fragments.
    DescriptorReadContext request{&value, xTaskGetCurrentTaskHandle(), connection, longRead};
    descriptorReadEvent(connection, m_handle, longRead ? 4 : 5, 0, 0);
    rc = longRead ? ble_gattc_read_long(connection, m_handle, 0, onReadCB, &request)
                  : ble_gattc_read(connection, m_handle, onReadCB, &request);
    if (rc == 0) {
      waitDescriptorRead(request);  // No caller timeout, cancellation, or early disconnect return.
      rc = request.status;
    } // Nonzero submission means SDK retained no callback argument.
    descriptorReadEvent(connection, m_handle, 6, rc, value.length());
    if (rc == 0 || rc == BLE_HS_EDONE) break;
    if (longRead && rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG)) {
      longRead = false;
      continue;
    }
    if ((rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) ||
         rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR) ||
         rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC)) &&
        BLESecurity::m_securityEnabled && securityRetries && client->isConnected() &&
        client->getConnId() == connection) {
      --securityRetries;
      if (client->secureConnection()) continue;
    }
    return String();  // Do not present a partial/error response as valid HID data.
  }
  if (!client->isConnected() || client->getConnId() != connection) return String();
  // Prepare the new cached value before replacing the last successful read.
  String cached(value);
  if (cached.length() != value.length()) {
    descriptorReadEvent(connection, m_handle, 7, BLE_HS_ENOMEM, value.length());
    return String();
  }
  m_semaphoreReadDescrEvt.take("readValue");
  m_value = std::move(cached);
  m_semaphoreReadDescrEvt.give();
  return value;
}  // readValue

/**
 * @brief Callback for Descriptor read operation.
 * @return success == 0 or error code.
 */
int BLERemoteDescriptor::onReadCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto& request = *static_cast<DescriptorReadContext*>(arg);
  int rc = error->status;
  if (conn_handle != request.connection) return finishDescriptorRead(request, BLE_HS_ENOTCONN);
  if (rc == 0) {
    if (!attr || !attr->om) return finishDescriptorRead(request, BLE_HS_EBADDATA);
    String& value = *request.value;
    const size_t dataLength = OS_MBUF_PKTLEN(attr->om);
    if (value.length() > BLE_ATT_ATTR_MAX_LEN || dataLength > BLE_ATT_ATTR_MAX_LEN - value.length()) {
      return finishDescriptorRead(request, BLE_HS_ATT_ERR(BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN));
    }
    if (!value.reserve(value.length() + dataLength)) return finishDescriptorRead(request, BLE_HS_ENOMEM);
    // A response mbuf may be chained. PKTLEN is not the first node's length.
    size_t copied = 0;
    for (const os_mbuf* part = attr->om; part; part = SLIST_NEXT(part, om_next)) {
      if (part->om_len > dataLength - copied) return finishDescriptorRead(request, BLE_HS_EBADDATA);
      if (part->om_len && !value.concat(reinterpret_cast<const char*>(part->om_data), part->om_len)) {
        return finishDescriptorRead(request, BLE_HS_ENOMEM);
      }
      copied += part->om_len;
    }
    if (copied != dataLength) return finishDescriptorRead(request, BLE_HS_EBADDATA);
    if (request.longRead) return 0;  // SDK will call again with EDONE/error.
    // A plain read has ONE successful callback, no later EDONE callback.
  }
  return finishDescriptorRead(request, rc);
}

/**
 * @brief Callback for descriptor write operation.
 * @return success == 0 or error code.
 */
int BLERemoteDescriptor::onWriteCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  BLETaskData *pTaskData = static_cast<BLETaskData *>(arg);
  BLERemoteDescriptor *descriptor = static_cast<BLERemoteDescriptor *>(pTaskData->m_pInstance);
  int rc = error->status;

  if (rc == BLE_HS_ENOTCONN) {
    log_e("<< Descriptor Write; Not connected");
    BLEUtils::taskRelease(*pTaskData, rc);
    return rc;
  }

  if (descriptor->getRemoteCharacteristic()->getRemoteService()->getClient()->getConnId() != conn_handle) {
    return 0;
  }

  log_i("Write complete; status=%u conn_handle=%u", rc, conn_handle);

  BLEUtils::taskRelease(*pTaskData, rc);
  return 0;
}

/**
 * @brief Write data to the BLE Remote Descriptor.
 * @param [in] data The data to send to the remote descriptor.
 * @param [in] length The length of the data to send.
 * @param [in] response True if we expect a write response.
 * @return True if successful
 */
bool BLERemoteDescriptor::writeValue(uint8_t *data, size_t length, bool response) {
  log_d(">> Descriptor writeValue: %s", toString().c_str());

  BLEClient *pClient = getRemoteCharacteristic()->getRemoteService()->getClient();

  // Check to see that we are connected.
  if (!pClient->isConnected()) {
    log_e("Disconnected");
    return false;
  }

  int rc = 0;
  int retryCount = 1;
  uint16_t mtu = ble_att_mtu(pClient->getConnId()) - 3;
  BLETaskData taskData(const_cast<BLERemoteDescriptor *>(this));

  // Check if the data length is longer than we can write in 1 connection event.
  // If so we must do a long write which requires a response.
  if (length <= mtu && !response) {
    rc = ble_gattc_write_no_rsp_flat(pClient->getConnId(), m_handle, data, length);
    goto exit;
  }

  do {
    if (length > mtu) {
      log_i("long write %lu bytes", (unsigned long)length);
      os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
      rc = ble_gattc_write_long(pClient->getConnId(), m_handle, 0, om, BLERemoteDescriptor::onWriteCB, &taskData);
    } else {
      rc = ble_gattc_write_flat(pClient->getConnId(), m_handle, data, length, BLERemoteDescriptor::onWriteCB, &taskData);
    }

    if (rc != 0) {
      goto exit;
    }

    BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    rc = taskData.m_flags;

    switch (rc) {
      case 0:
      case BLE_HS_EDONE: rc = 0; break;
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG):
        log_e("Long write not supported by peer; Truncating length to %u", mtu);
        retryCount++;
        length = mtu;
        break;

      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN):
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR):
      case BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC):
        if (BLESecurity::m_securityEnabled && retryCount && pClient->secureConnection()) {
          break;
        }
      /* Else falls through. */
      default: goto exit;
    }
  } while (rc != 0 && retryCount--);

exit:
  if (rc != 0) {
    log_e("<< writeValue failed rc=%d, %s", rc, BLEUtils::returnCodeToString(rc));
  } else {
    log_d("<< writeValue success. length: %lu rc=%d", (unsigned long)length, rc);
  }

  return (rc == 0);
}  // writeValue

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
