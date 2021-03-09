/*
** Copyright 2018 Intel Corporation
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <string.h>
#include "cg_protocol.h"
#include "cg_log.h"

CGProtocol::CGProtocol(acgmsg_terminal_t source, acgmsg_terminal_t destinate,
                       uint32_t instance_id) {
    m_source = source;
    m_destinate = destinate;
    m_instance_id = instance_id;
}

CGProtocol::~CGProtocol() {}

int CGProtocol::set_acgmsg_header(void* buff_ptr, acgmsg_header_type_t msg_type,
                                  int payload_length) {
    acgmsg_header_t* header_ptr = (acgmsg_header_t*)buff_ptr;

    header_ptr->msg_magic = ACG_MAGIC;
    header_ptr->msg_size = (uint32_t)(LENGTH_MAIN_HEADER + payload_length);
    header_ptr->msg_type = msg_type;
    header_ptr->msg_source = m_source;
    header_ptr->msg_destination = m_destinate;
    header_ptr->timestamp = (uint64_t)0;

    header_ptr->ack_required = 0;
    header_ptr->instance_id = m_instance_id;
    return LENGTH_MAIN_HEADER;
}

void CGProtocol::set_instance_id(int id) { m_instance_id = id; }

acgmsg_header_t* CGProtocol::get_acgmsg_header() { return m_acgmsg_header; }

unique_ptr<CGBuffer> CGProtocol::build_acg_packet(acgmsg_header_type_t type,
                                                  const void* payload_ptr, int payload_length) {
    unique_ptr<CGBuffer> buff_ptr(new CGBuffer(LENGTH_MAIN_HEADER + payload_length));
    set_acgmsg_header(buff_ptr->m_ptr, type, payload_length);
    memcpy(buff_ptr->m_ptr + LENGTH_MAIN_HEADER, payload_ptr, payload_length);
    return buff_ptr;
}

unique_ptr<CGBuffer> CGProtocol::build_message_packet(acgmsg_message_type_t type,
                                                      const void* payload_ptr, int payload_length) {
    unique_ptr<CGBuffer> buff_ptr(new CGBuffer(LENGTH_FULL_HEADER + payload_length));
    set_acgmsg_header(buff_ptr->m_ptr, ACG_TYPE_MESSAGE, LENGTH_PAYLOAD_HEADER + payload_length);
    acgmsg_message_header_t* header_ptr =
        (acgmsg_message_header_t*)(buff_ptr->m_ptr + LENGTH_MAIN_HEADER);
    header_ptr->message_type = type;
    header_ptr->message_size = LENGTH_PAYLOAD_HEADER + payload_length;
    memcpy(buff_ptr->m_ptr + LENGTH_FULL_HEADER, payload_ptr, payload_length);
    return buff_ptr;
}

unique_ptr<CGBuffer> CGProtocol::build_config_packet(acgmsg_config_type_t type,
                                                     const void* payload_ptr, int payload_length) {
    unique_ptr<CGBuffer> buff_ptr(new CGBuffer(LENGTH_FULL_HEADER + payload_length));
    set_acgmsg_header(buff_ptr->m_ptr, ACG_TYPE_CONFIG, LENGTH_PAYLOAD_HEADER + payload_length);
    acgmsg_config_header_t* header_ptr =
        (acgmsg_config_header_t*)(buff_ptr->m_ptr + LENGTH_MAIN_HEADER);
    header_ptr->config_type = type;
    header_ptr->config_size = LENGTH_PAYLOAD_HEADER + payload_length;
    memcpy(buff_ptr->m_ptr + LENGTH_FULL_HEADER, payload_ptr, payload_length);
    return buff_ptr;
}

void CGProtocol::parse_packet(const void* packet_ptr, int packet_len) {
    int len = packet_len;
    char* ptr = (char*)packet_ptr;

    while (len > LENGTH_MAIN_HEADER) {
        acgmsg_header_t* header_ptr = (acgmsg_header_t*)(ptr);
        if (header_ptr->msg_destination != m_source) {
            ALOGW("recieve wrong destination, m_source: %d, %d", header_ptr->msg_destination,
                  m_source);
            break;
        }

        if (header_ptr->msg_size < LENGTH_MAIN_HEADER || (uint32_t)len < header_ptr->msg_size) {
            ALOGW("recieve wrong size: %d, len = %d", header_ptr->msg_size, len);
            break;
        }

        m_acgmsg_header = header_ptr;

        char* payload_ptr = ptr + LENGTH_MAIN_HEADER;

        switch (header_ptr->msg_type) {
            case ACG_TYPE_CONNECTION: {
                if (header_ptr->msg_size != sizeof(acgmsg_connection_t) + LENGTH_MAIN_HEADER) {
                    ALOGW("message length check failed");
                    break;
                }
                if (m_connection_callback) {
                    m_connection_callback((acgmsg_connection_t*)payload_ptr);
                }
                break;
            }

            case ACG_TYPE_MESSAGE: {
                if (len < LENGTH_FULL_HEADER) {
                    ALOGW("not enough data to parse message header");
                    break;
                }
                acgmsg_message_header_t* message_header = (acgmsg_message_header_t*)(payload_ptr);
                int message_size = message_header->message_size;

                if (header_ptr->msg_size != (uint32_t)(message_size + LENGTH_MAIN_HEADER)) {
                    ALOGW("message length check failed, header_size is %d, message_size is %d",
                          header_ptr->msg_size, message_size);
                    break;
                }

                if (m_message_callback) {
                    m_message_callback((acgmsg_message_type_t)message_header->message_type,
                                       payload_ptr + LENGTH_PAYLOAD_HEADER,
                                       message_size - LENGTH_PAYLOAD_HEADER);
                }
                break;
            }

            case ACG_TYPE_CONFIG: {
                if (len < LENGTH_FULL_HEADER) {
                    ALOGW("not enough data to prase config header");
                    break;
                }
                acgmsg_config_header_t* config_header = (acgmsg_config_header_t*)(payload_ptr);
                int config_size = (int)config_header->config_size;
                if (header_ptr->msg_size != (uint32_t)(config_size + LENGTH_MAIN_HEADER)) {
                    ALOGW("config length check failed, header_size is %d, config_size is %d",
                          header_ptr->msg_size, config_size);
                    break;
                }
                if (m_config_callback) {
                    m_config_callback((acgmsg_config_type_t)config_header->config_type,
                                      payload_ptr + LENGTH_PAYLOAD_HEADER,
                                      config_size - LENGTH_PAYLOAD_HEADER);
                }
                break;
            }

            default:
                ALOGW("recieve unknown acg_msg type: %d", header_ptr->msg_type);
        }
        ptr += header_ptr->msg_size;
        len -= header_ptr->msg_size;
    }
}

void CGProtocol::register_handle_connection_callback(handle_connection_callback_t callback) {
    m_connection_callback = callback;
}

void CGProtocol::register_handle_message_callback(handle_message_callback_t callback) {
    m_message_callback = callback;
}

void CGProtocol::register_handle_config_callback(handle_config_callback_t callback) {
    m_config_callback = callback;
}
