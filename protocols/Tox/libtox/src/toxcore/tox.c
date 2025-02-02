/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2016-2018 The TokTok team.
 * Copyright © 2013 Tox project.
 */

/**
 * The Tox public API.
 */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif /* _XOPEN_SOURCE */

#include "tox.h"

#include <assert.h>
#include <string.h>

#include "DHT.h"
#include "Messenger.h"
#include "TCP_client.h"
#include "attributes.h"
#include "ccompat.h"
#include "crypto_core.h"
#include "friend_requests.h"
#include "group.h"
#include "group_chats.h"
#include "group_common.h"
#include "logger.h"
#include "mem.h"
#include "mono_time.h"
#include "net_crypto.h"
#include "network.h"
#include "onion_client.h"
#include "state.h"
#include "tox_private.h"
#include "tox_struct.h"
#include "util.h"

#include "../toxencryptsave/defines.h"

#define SET_ERROR_PARAMETER(param, x) \
    do {                              \
        if (param != nullptr) {       \
            *param = x;               \
        }                             \
    } while (0)

static_assert(TOX_HASH_LENGTH == CRYPTO_SHA256_SIZE,
              "TOX_HASH_LENGTH is assumed to be equal to CRYPTO_SHA256_SIZE");
static_assert(FILE_ID_LENGTH == CRYPTO_SYMMETRIC_KEY_SIZE,
              "FILE_ID_LENGTH is assumed to be equal to CRYPTO_SYMMETRIC_KEY_SIZE");
static_assert(TOX_DHT_NODE_IP_STRING_SIZE == IP_NTOA_LEN,
              "TOX_DHT_NODE_IP_STRING_SIZE is assumed to be equal to IP_NTOA_LEN");
static_assert(TOX_GROUP_PEER_IP_STRING_MAX_LENGTH == IP_NTOA_LEN,
              "TOX_GROUP_PEER_IP_STRING_MAX_LENGTH is assumed to be equal to IP_NTOA_LEN");
static_assert(TOX_DHT_NODE_PUBLIC_KEY_SIZE == CRYPTO_PUBLIC_KEY_SIZE,
              "TOX_DHT_NODE_PUBLIC_KEY_SIZE is assumed to be equal to CRYPTO_PUBLIC_KEY_SIZE");
static_assert(TOX_FILE_ID_LENGTH == CRYPTO_SYMMETRIC_KEY_SIZE,
              "TOX_FILE_ID_LENGTH is assumed to be equal to CRYPTO_SYMMETRIC_KEY_SIZE");
static_assert(TOX_FILE_ID_LENGTH == TOX_HASH_LENGTH,
              "TOX_FILE_ID_LENGTH is assumed to be equal to TOX_HASH_LENGTH");
static_assert(TOX_PUBLIC_KEY_SIZE == CRYPTO_PUBLIC_KEY_SIZE,
              "TOX_PUBLIC_KEY_SIZE is assumed to be equal to CRYPTO_PUBLIC_KEY_SIZE");
static_assert(TOX_SECRET_KEY_SIZE == CRYPTO_SECRET_KEY_SIZE,
              "TOX_SECRET_KEY_SIZE is assumed to be equal to CRYPTO_SECRET_KEY_SIZE");
static_assert(TOX_MAX_NAME_LENGTH == MAX_NAME_LENGTH,
              "TOX_MAX_NAME_LENGTH is assumed to be equal to MAX_NAME_LENGTH");
static_assert(TOX_MAX_STATUS_MESSAGE_LENGTH == MAX_STATUSMESSAGE_LENGTH,
              "TOX_MAX_STATUS_MESSAGE_LENGTH is assumed to be equal to MAX_STATUSMESSAGE_LENGTH");
static_assert(TOX_GROUP_MAX_MESSAGE_LENGTH == GROUP_MAX_MESSAGE_LENGTH,
              "TOX_GROUP_MAX_MESSAGE_LENGTH is assumed to be equal to GROUP_MAX_MESSAGE_LENGTH");
static_assert(TOX_MAX_CUSTOM_PACKET_SIZE == MAX_GC_CUSTOM_LOSSLESS_PACKET_SIZE,
              "TOX_MAX_CUSTOM_PACKET_SIZE is assumed to be equal to MAX_GC_CUSTOM_LOSSLESS_PACKET_SIZE");

struct Tox_Userdata {
    Tox *tox;
    void *user_data;
};

static logger_cb tox_log_handler;
non_null(1, 3, 5, 6) nullable(7)
static void tox_log_handler(void *context, Logger_Level level, const char *file, int line, const char *func,
                            const char *message, void *userdata)
{
    Tox *tox = (Tox *)context;
    assert(tox != nullptr);

    if (tox->log_callback != nullptr) {
        tox->log_callback(tox, (Tox_Log_Level)level, file, line, func, message, userdata);
    }
}

static m_self_connection_status_cb tox_self_connection_status_handler;
non_null(1) nullable(3)
static void tox_self_connection_status_handler(Messenger *m, Onion_Connection_Status connection_status, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->self_connection_status_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->self_connection_status_callback(tox_data->tox, (Tox_Connection)connection_status, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_name_cb tox_friend_name_handler;
non_null(1, 3) nullable(5)
static void tox_friend_name_handler(Messenger *m, uint32_t friend_number, const uint8_t *name, size_t length,
                                    void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_name_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_name_callback(tox_data->tox, friend_number, name, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_status_message_cb tox_friend_status_message_handler;
non_null(1, 3) nullable(5)
static void tox_friend_status_message_handler(Messenger *m, uint32_t friend_number, const uint8_t *message,
        size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_status_message_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_status_message_callback(tox_data->tox, friend_number, message, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_status_cb tox_friend_status_handler;
non_null(1) nullable(4)
static void tox_friend_status_handler(Messenger *m, uint32_t friend_number, unsigned int status, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_status_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_status_callback(tox_data->tox, friend_number, (Tox_User_Status)status, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_connection_status_cb tox_friend_connection_status_handler;
non_null(1) nullable(4)
static void tox_friend_connection_status_handler(Messenger *m, uint32_t friend_number, unsigned int connection_status,
        void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_connection_status_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_connection_status_callback(tox_data->tox, friend_number, (Tox_Connection)connection_status,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_typing_cb tox_friend_typing_handler;
non_null(1) nullable(4)
static void tox_friend_typing_handler(Messenger *m, uint32_t friend_number, bool is_typing, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_typing_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_typing_callback(tox_data->tox, friend_number, is_typing, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_read_receipt_cb tox_friend_read_receipt_handler;
non_null(1) nullable(4)
static void tox_friend_read_receipt_handler(Messenger *m, uint32_t friend_number, uint32_t message_id, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_read_receipt_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_read_receipt_callback(tox_data->tox, friend_number, message_id, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_request_cb tox_friend_request_handler;
non_null(1, 2, 3) nullable(5)
static void tox_friend_request_handler(Messenger *m, const uint8_t public_key[TOX_PUBLIC_KEY_SIZE], const uint8_t *message, size_t length,
                                       void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_request_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_request_callback(tox_data->tox, public_key, message, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_message_cb tox_friend_message_handler;
non_null(1, 4) nullable(6)
static void tox_friend_message_handler(Messenger *m, uint32_t friend_number, unsigned int message_type,
                                       const uint8_t *message, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->friend_message_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_message_callback(tox_data->tox, friend_number, (Tox_Message_Type)message_type, message, length,
                                               tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_file_recv_control_cb tox_file_recv_control_handler;
non_null(1) nullable(5)
static void tox_file_recv_control_handler(Messenger *m, uint32_t friend_number, uint32_t file_number,
        unsigned int control, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->file_recv_control_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->file_recv_control_callback(tox_data->tox, friend_number, file_number, (Tox_File_Control)control,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_file_chunk_request_cb tox_file_chunk_request_handler;
non_null(1) nullable(6)
static void tox_file_chunk_request_handler(Messenger *m, uint32_t friend_number, uint32_t file_number,
        uint64_t position, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->file_chunk_request_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->file_chunk_request_callback(tox_data->tox, friend_number, file_number, position, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_file_recv_cb tox_file_recv_handler;
non_null(1, 6) nullable(8)
static void tox_file_recv_handler(Messenger *m, uint32_t friend_number, uint32_t file_number, uint32_t kind,
                                  uint64_t file_size, const uint8_t *filename, size_t filename_length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->file_recv_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->file_recv_callback(tox_data->tox, friend_number, file_number, kind, file_size, filename, filename_length,
                                          tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_file_recv_chunk_cb tox_file_recv_chunk_handler;
non_null(1, 5) nullable(7)
static void tox_file_recv_chunk_handler(Messenger *m, uint32_t friend_number, uint32_t file_number, uint64_t position,
                                        const uint8_t *data, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->file_recv_chunk_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->file_recv_chunk_callback(tox_data->tox, friend_number, file_number, position, data, length,
                                                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static g_conference_invite_cb tox_conference_invite_handler;
non_null(1, 4) nullable(6)
static void tox_conference_invite_handler(Messenger *m, uint32_t friend_number, int type, const uint8_t *cookie,
        size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->conference_invite_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->conference_invite_callback(tox_data->tox, friend_number, (Tox_Conference_Type)type, cookie, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static g_conference_connected_cb tox_conference_connected_handler;
non_null(1) nullable(3)
static void tox_conference_connected_handler(Messenger *m, uint32_t conference_number, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->conference_connected_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->conference_connected_callback(tox_data->tox, conference_number, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static g_conference_message_cb tox_conference_message_handler;
non_null(1, 5) nullable(7)
static void tox_conference_message_handler(Messenger *m, uint32_t conference_number, uint32_t peer_number, int type,
        const uint8_t *message, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->conference_message_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->conference_message_callback(tox_data->tox, conference_number, peer_number, (Tox_Message_Type)type,
                message, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static title_cb tox_conference_title_handler;
non_null(1, 4) nullable(6)
static void tox_conference_title_handler(Messenger *m, uint32_t conference_number, uint32_t peer_number,
        const uint8_t *title, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->conference_title_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->conference_title_callback(tox_data->tox, conference_number, peer_number, title, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static peer_name_cb tox_conference_peer_name_handler;
non_null(1, 4) nullable(6)
static void tox_conference_peer_name_handler(Messenger *m, uint32_t conference_number, uint32_t peer_number,
        const uint8_t *name, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->conference_peer_name_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->conference_peer_name_callback(tox_data->tox, conference_number, peer_number, name, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static peer_list_changed_cb tox_conference_peer_list_changed_handler;
non_null(1) nullable(3)
static void tox_conference_peer_list_changed_handler(Messenger *m, uint32_t conference_number, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->conference_peer_list_changed_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->conference_peer_list_changed_callback(tox_data->tox, conference_number, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static dht_get_nodes_response_cb tox_dht_get_nodes_response_handler;
non_null(1, 2) nullable(3)
static void tox_dht_get_nodes_response_handler(const DHT *dht, const Node_format *node, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->dht_get_nodes_response_callback == nullptr) {
        return;
    }

    Ip_Ntoa ip_str;
    tox_unlock(tox_data->tox);
    tox_data->tox->dht_get_nodes_response_callback(
        tox_data->tox, node->public_key, net_ip_ntoa(&node->ip_port.ip, &ip_str), net_ntohs(node->ip_port.port),
        tox_data->user_data);
    tox_lock(tox_data->tox);
}

static m_friend_lossy_packet_cb tox_friend_lossy_packet_handler;
non_null(1, 4) nullable(6)
static void tox_friend_lossy_packet_handler(Messenger *m, uint32_t friend_number, uint8_t packet_id,
        const uint8_t *data, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    assert(data != nullptr);
    assert(length > 0);

    if (tox_data->tox->friend_lossy_packet_callback_per_pktid[packet_id] != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_lossy_packet_callback_per_pktid[packet_id](tox_data->tox, friend_number, data, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

static m_friend_lossless_packet_cb tox_friend_lossless_packet_handler;
non_null(1, 4) nullable(6)
static void tox_friend_lossless_packet_handler(Messenger *m, uint32_t friend_number, uint8_t packet_id,
        const uint8_t *data, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    assert(data != nullptr);
    assert(length > 0);

    if (tox_data->tox->friend_lossless_packet_callback_per_pktid[packet_id] != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->friend_lossless_packet_callback_per_pktid[packet_id](tox_data->tox, friend_number, data, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 4) nullable(6)
static void tox_group_peer_name_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id,
                                        const uint8_t *name, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_peer_name_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_peer_name_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), name, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(5)
static void tox_group_peer_status_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id,
        unsigned int status, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_peer_status_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_peer_status_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), (Tox_User_Status)status,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 4) nullable(6)
static void tox_group_topic_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id, const uint8_t *topic,
                                    size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_topic_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_topic_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), topic, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(4)
static void tox_group_topic_lock_handler(const Messenger *m, uint32_t group_number, unsigned int topic_lock,
        void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_topic_lock_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_topic_lock_callback(tox_data->tox, group_number, (Tox_Group_Topic_Lock)topic_lock,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(4)
static void tox_group_voice_state_handler(const Messenger *m, uint32_t group_number, unsigned int voice_state,
        void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_voice_state_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_voice_state_callback(tox_data->tox, group_number, (Tox_Group_Voice_State)voice_state,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(4)
static void tox_group_peer_limit_handler(const Messenger *m, uint32_t group_number, uint32_t peer_limit,
        void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_peer_limit_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_peer_limit_callback(tox_data->tox, group_number, peer_limit, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(4)
static void tox_group_privacy_state_handler(const Messenger *m, uint32_t group_number, unsigned int privacy_state,
        void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_privacy_state_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_privacy_state_callback(tox_data->tox, group_number, (Tox_Group_Privacy_State)privacy_state,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(3, 5)
static void tox_group_password_handler(const Messenger *m, uint32_t group_number, const uint8_t *password,
                                       size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_password_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_password_callback(tox_data->tox, group_number, password, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 5) nullable(8)
static void tox_group_message_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id, unsigned int type,
                                      const uint8_t *message, size_t length, Tox_Group_Message_Id message_id, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_message_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_message_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), (Tox_Message_Type)type, message, length,
                                              message_id, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 5) nullable(8)
static void tox_group_private_message_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id,
        unsigned int type, const uint8_t *message, size_t length, Tox_Group_Message_Id message_id, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_private_message_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_private_message_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), (Tox_Message_Type)type, message,
                length, message_id, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 4) nullable(6)
static void tox_group_custom_packet_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id,
        const uint8_t *data, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_custom_packet_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_custom_packet_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), data, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 4) nullable(6)
static void tox_group_custom_private_packet_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id,
        const uint8_t *data, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_custom_private_packet_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_custom_private_packet_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), data, length,
                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 3, 5) nullable(7)
static void tox_group_invite_handler(const Messenger *m, uint32_t friend_number, const uint8_t *invite_data,
                                     size_t length, const uint8_t *group_name, size_t group_name_length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_invite_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_invite_callback(tox_data->tox, friend_number, invite_data, length, group_name, group_name_length,
                                             tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(4)
static void tox_group_peer_join_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_peer_join_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_peer_join_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id), tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1, 5) nullable(7, 9)
static void tox_group_peer_exit_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id peer_id,
                                        unsigned int exit_type, const uint8_t *name, size_t name_length,
                                        const uint8_t *part_message, size_t length, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_peer_exit_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_peer_exit_callback(tox_data->tox, group_number, gc_peer_id_to_int(peer_id),
                                                (Tox_Group_Exit_Type)exit_type, name, name_length,
                                                part_message, length, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(3)
static void tox_group_self_join_handler(const Messenger *m, uint32_t group_number, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_self_join_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_self_join_callback(tox_data->tox, group_number, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(4)
static void tox_group_join_fail_handler(const Messenger *m, uint32_t group_number, unsigned int fail_type,
                                        void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_join_fail_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_join_fail_callback(tox_data->tox, group_number, (Tox_Group_Join_Fail)fail_type,
                                                tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

non_null(1) nullable(6)
static void tox_group_moderation_handler(const Messenger *m, uint32_t group_number, GC_Peer_Id source_peer_number,
        GC_Peer_Id target_peer_number, unsigned int mod_type, void *user_data)
{
    struct Tox_Userdata *tox_data = (struct Tox_Userdata *)user_data;

    if (tox_data->tox->group_moderation_callback != nullptr) {
        tox_unlock(tox_data->tox);
        tox_data->tox->group_moderation_callback(tox_data->tox, group_number,
                gc_peer_id_to_int(source_peer_number), gc_peer_id_to_int(target_peer_number),
                (Tox_Group_Mod_Event)mod_type, tox_data->user_data);
        tox_lock(tox_data->tox);
    }
}

bool tox_version_is_compatible(uint32_t major, uint32_t minor, uint32_t patch)
{
    return TOX_VERSION_IS_API_COMPATIBLE(major, minor, patch);
}

non_null()
static State_Load_Status state_load_callback(void *outer, const uint8_t *data, uint32_t length, uint16_t type)
{
    const Tox *tox = (const Tox *)outer;
    State_Load_Status status = STATE_LOAD_STATUS_CONTINUE;

    if (messenger_load_state_section(tox->m, data, length, type, &status)
            || conferences_load_state_section(tox->m->conferences_object, data, length, type, &status)) {
        return status;
    }

    if (type == STATE_TYPE_END) {
        if (length != 0) {
            return STATE_LOAD_STATUS_ERROR;
        }

        return STATE_LOAD_STATUS_END;
    }

    LOGGER_ERROR(tox->m->log, "Load state: contains unrecognized part (len %u, type %u)",
                 length, type);

    return STATE_LOAD_STATUS_CONTINUE;
}

/** Load tox from data of size length. */
non_null()
static int tox_load(Tox *tox, const uint8_t *data, uint32_t length)
{
    uint32_t data32[2];
    const uint32_t cookie_len = sizeof(data32);

    if (length < cookie_len) {
        return -1;
    }

    memcpy(data32, data, sizeof(uint32_t));
    lendian_bytes_to_host32(data32 + 1, data + sizeof(uint32_t));

    if (data32[0] != 0 || data32[1] != STATE_COOKIE_GLOBAL) {
        return -1;
    }

    return state_load(tox->m->log, state_load_callback, tox, data + cookie_len,
                      length - cookie_len, STATE_COOKIE_TYPE);
}

nullable(1, 2, 3)
static Tox *tox_new_system(const struct Tox_Options *options, Tox_Err_New *error, const Tox_System *sys)
{
    struct Tox_Options *default_options = nullptr;

    if (options == nullptr) {
        Tox_Err_Options_New err;
        default_options = tox_options_new(&err);

        switch (err) {
            case TOX_ERR_OPTIONS_NEW_OK: {
                assert(default_options != nullptr);
                break;
            }

            case TOX_ERR_OPTIONS_NEW_MALLOC: {
                SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
                return nullptr;
            }
        }
    }

    const struct Tox_Options *const opts = options != nullptr ? options : default_options;
    assert(opts != nullptr);

    const Tox_System default_system = tox_default_system();

    if (sys == nullptr) {
        sys = &default_system;
    }

    if (sys->rng == nullptr || sys->ns == nullptr || sys->mem == nullptr) {
        // TODO(iphydf): Not quite right, but similar.
        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
        tox_options_free(default_options);
        return nullptr;
    }

    Messenger_Options m_options = {false};

    bool load_savedata_sk = false;
    bool load_savedata_tox = false;

    if (tox_options_get_savedata_type(opts) != TOX_SAVEDATA_TYPE_NONE) {
        if (tox_options_get_savedata_data(opts) == nullptr || tox_options_get_savedata_length(opts) == 0) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_LOAD_BAD_FORMAT);
            tox_options_free(default_options);
            return nullptr;
        }
    }

    if (tox_options_get_savedata_type(opts) == TOX_SAVEDATA_TYPE_SECRET_KEY) {
        if (tox_options_get_savedata_length(opts) != TOX_SECRET_KEY_SIZE) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_LOAD_BAD_FORMAT);
            tox_options_free(default_options);
            return nullptr;
        }

        load_savedata_sk = true;
    } else if (tox_options_get_savedata_type(opts) == TOX_SAVEDATA_TYPE_TOX_SAVE) {
        if (tox_options_get_savedata_length(opts) < TOX_ENC_SAVE_MAGIC_LENGTH) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_LOAD_BAD_FORMAT);
            tox_options_free(default_options);
            return nullptr;
        }

        if (memcmp(tox_options_get_savedata_data(opts), TOX_ENC_SAVE_MAGIC_NUMBER, TOX_ENC_SAVE_MAGIC_LENGTH) == 0) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_LOAD_ENCRYPTED);
            tox_options_free(default_options);
            return nullptr;
        }

        load_savedata_tox = true;
    }

    m_options.ipv6enabled = tox_options_get_ipv6_enabled(opts);
    m_options.udp_disabled = !tox_options_get_udp_enabled(opts);
    m_options.port_range[0] = tox_options_get_start_port(opts);
    m_options.port_range[1] = tox_options_get_end_port(opts);
    m_options.tcp_server_port = tox_options_get_tcp_port(opts);
    m_options.hole_punching_enabled = tox_options_get_hole_punching_enabled(opts);
    m_options.local_discovery_enabled = tox_options_get_local_discovery_enabled(opts);
    m_options.dht_announcements_enabled = tox_options_get_dht_announcements_enabled(opts);
    m_options.groups_persistence_enabled = tox_options_get_experimental_groups_persistence(opts);

    if (m_options.udp_disabled) {
        m_options.local_discovery_enabled = false;
    }

    Tox *tox = (Tox *)mem_alloc(sys->mem, sizeof(Tox));

    if (tox == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
        tox_options_free(default_options);
        return nullptr;
    }

    tox->log_callback = tox_options_get_log_callback(opts);
    m_options.log_callback = tox_log_handler;
    m_options.log_context = tox;
    m_options.log_user_data = tox_options_get_log_user_data(opts);

    switch (tox_options_get_proxy_type(opts)) {
        case TOX_PROXY_TYPE_HTTP: {
            m_options.proxy_info.proxy_type = TCP_PROXY_HTTP;
            break;
        }

        case TOX_PROXY_TYPE_SOCKS5: {
            m_options.proxy_info.proxy_type = TCP_PROXY_SOCKS5;
            break;
        }

        case TOX_PROXY_TYPE_NONE: {
            m_options.proxy_info.proxy_type = TCP_PROXY_NONE;
            break;
        }

        default: {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_PROXY_BAD_TYPE);
            mem_delete(sys->mem, tox);
            tox_options_free(default_options);
            return nullptr;
        }
    }

    tox->sys = *sys;

    if (m_options.proxy_info.proxy_type != TCP_PROXY_NONE) {
        if (tox_options_get_proxy_port(opts) == 0) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_PROXY_BAD_PORT);
            mem_delete(sys->mem, tox);
            tox_options_free(default_options);
            return nullptr;
        }

        ip_init(&m_options.proxy_info.ip_port.ip, m_options.ipv6enabled);

        if (m_options.ipv6enabled) {
            m_options.proxy_info.ip_port.ip.family = net_family_unspec();
        }

        const char *const proxy_host = tox_options_get_proxy_host(opts);

        if (proxy_host == nullptr
                || !addr_resolve_or_parse_ip(tox->sys.ns, proxy_host, &m_options.proxy_info.ip_port.ip, nullptr)) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_PROXY_BAD_HOST);
            // TODO(irungentoo): TOX_ERR_NEW_PROXY_NOT_FOUND if domain.
            mem_delete(sys->mem, tox);
            tox_options_free(default_options);
            return nullptr;
        }

        m_options.proxy_info.ip_port.port = net_htons(tox_options_get_proxy_port(opts));
    }

    tox->mono_time = mono_time_new(tox->sys.mem, sys->mono_time_callback, sys->mono_time_user_data);

    if (tox->mono_time == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
        mem_delete(sys->mem, tox);
        tox_options_free(default_options);
        return nullptr;
    }

    if (tox_options_get_experimental_thread_safety(opts)) {
        pthread_mutex_t *mutex = (pthread_mutex_t *)mem_alloc(sys->mem, sizeof(pthread_mutex_t));

        if (mutex == nullptr) {
            SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
            mem_delete(sys->mem, tox);
            tox_options_free(default_options);
            return nullptr;
        }

        pthread_mutex_init(mutex, nullptr);

        tox->mutex = mutex;
    } else {
        tox->mutex = nullptr;
    }

    tox_lock(tox);

    Messenger_Error m_error;
    tox->m = new_messenger(tox->mono_time, tox->sys.mem, tox->sys.rng, tox->sys.ns, &m_options, &m_error);

    if (tox->m == nullptr) {
        switch (m_error) {
            case MESSENGER_ERROR_PORT:
            case MESSENGER_ERROR_TCP_SERVER: {
                SET_ERROR_PARAMETER(error, TOX_ERR_NEW_PORT_ALLOC);
                break;
            }
            case MESSENGER_ERROR_OTHER:
            case MESSENGER_ERROR_NONE: {
                SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
                break;
            }
        }

        mono_time_free(tox->sys.mem, tox->mono_time);
        tox_unlock(tox);

        if (tox->mutex != nullptr) {
            pthread_mutex_destroy(tox->mutex);
        }

        mem_delete(sys->mem, tox->mutex);
        mem_delete(sys->mem, tox);
        tox_options_free(default_options);
        return nullptr;
    }

    tox->m->conferences_object = new_groupchats(tox->mono_time, tox->m);

    if (tox->m->conferences_object == nullptr) {
        kill_messenger(tox->m);

        mono_time_free(tox->sys.mem, tox->mono_time);
        tox_unlock(tox);

        if (tox->mutex != nullptr) {
            pthread_mutex_destroy(tox->mutex);
        }

        mem_delete(sys->mem, tox->mutex);
        mem_delete(sys->mem, tox);

        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_MALLOC);
        tox_options_free(default_options);
        return nullptr;
    }

    if (load_savedata_tox
            && tox_load(tox, tox_options_get_savedata_data(opts), tox_options_get_savedata_length(opts)) == -1) {
        kill_groupchats(tox->m->conferences_object);
        kill_messenger(tox->m);

        mono_time_free(tox->sys.mem, tox->mono_time);
        tox_unlock(tox);

        if (tox->mutex != nullptr) {
            pthread_mutex_destroy(tox->mutex);
        }

        mem_delete(sys->mem, tox->mutex);
        mem_delete(sys->mem, tox);

        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_LOAD_BAD_FORMAT);
        tox_options_free(default_options);
        return nullptr;
    }

    if (load_savedata_sk) {
        load_secret_key(tox->m->net_crypto, tox_options_get_savedata_data(opts));
    }

    m_callback_namechange(tox->m, tox_friend_name_handler);
    m_callback_core_connection(tox->m, tox_self_connection_status_handler);
    m_callback_statusmessage(tox->m, tox_friend_status_message_handler);
    m_callback_userstatus(tox->m, tox_friend_status_handler);
    m_callback_connectionstatus(tox->m, tox_friend_connection_status_handler);
    m_callback_typingchange(tox->m, tox_friend_typing_handler);
    m_callback_read_receipt(tox->m, tox_friend_read_receipt_handler);
    m_callback_friendrequest(tox->m, tox_friend_request_handler);
    m_callback_friendmessage(tox->m, tox_friend_message_handler);
    callback_file_control(tox->m, tox_file_recv_control_handler);
    callback_file_reqchunk(tox->m, tox_file_chunk_request_handler);
    callback_file_sendrequest(tox->m, tox_file_recv_handler);
    callback_file_data(tox->m, tox_file_recv_chunk_handler);
    dht_callback_get_nodes_response(tox->m->dht, tox_dht_get_nodes_response_handler);
    g_callback_group_invite(tox->m->conferences_object, tox_conference_invite_handler);
    g_callback_group_connected(tox->m->conferences_object, tox_conference_connected_handler);
    g_callback_group_message(tox->m->conferences_object, tox_conference_message_handler);
    g_callback_group_title(tox->m->conferences_object, tox_conference_title_handler);
    g_callback_peer_name(tox->m->conferences_object, tox_conference_peer_name_handler);
    g_callback_peer_list_changed(tox->m->conferences_object, tox_conference_peer_list_changed_handler);
    custom_lossy_packet_registerhandler(tox->m, tox_friend_lossy_packet_handler);
    custom_lossless_packet_registerhandler(tox->m, tox_friend_lossless_packet_handler);

    m_callback_group_invite(tox->m, tox_group_invite_handler);
    gc_callback_message(tox->m, tox_group_message_handler);
    gc_callback_private_message(tox->m, tox_group_private_message_handler);
    gc_callback_custom_packet(tox->m, tox_group_custom_packet_handler);
    gc_callback_custom_private_packet(tox->m, tox_group_custom_private_packet_handler);
    gc_callback_moderation(tox->m, tox_group_moderation_handler);
    gc_callback_nick_change(tox->m, tox_group_peer_name_handler);
    gc_callback_status_change(tox->m, tox_group_peer_status_handler);
    gc_callback_topic_change(tox->m, tox_group_topic_handler);
    gc_callback_peer_limit(tox->m, tox_group_peer_limit_handler);
    gc_callback_privacy_state(tox->m, tox_group_privacy_state_handler);
    gc_callback_topic_lock(tox->m, tox_group_topic_lock_handler);
    gc_callback_password(tox->m, tox_group_password_handler);
    gc_callback_peer_join(tox->m, tox_group_peer_join_handler);
    gc_callback_peer_exit(tox->m, tox_group_peer_exit_handler);
    gc_callback_self_join(tox->m, tox_group_self_join_handler);
    gc_callback_rejected(tox->m, tox_group_join_fail_handler);
    gc_callback_voice_state(tox->m, tox_group_voice_state_handler);

    tox_unlock(tox);

    SET_ERROR_PARAMETER(error, TOX_ERR_NEW_OK);

    tox_options_free(default_options);
    return tox;
}

Tox *tox_new(const struct Tox_Options *options, Tox_Err_New *error)
{
    return tox_new_system(options, error, nullptr);
}

Tox *tox_new_testing(const Tox_Options *options, Tox_Err_New *error, const Tox_Options_Testing *testing, Tox_Err_New_Testing *testing_error)
{
    if (testing == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_NULL);
        SET_ERROR_PARAMETER(testing_error, TOX_ERR_NEW_TESTING_NULL);
        return nullptr;
    }

    if (testing->operating_system == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_NULL);
        SET_ERROR_PARAMETER(testing_error, TOX_ERR_NEW_TESTING_NULL);
        return nullptr;
    }

    const Tox_System *sys = testing->operating_system;

    if (sys->rng == nullptr || sys->ns == nullptr || sys->mem == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_NEW_NULL);
        SET_ERROR_PARAMETER(testing_error, TOX_ERR_NEW_TESTING_NULL);
        return nullptr;
    }

    SET_ERROR_PARAMETER(testing_error, TOX_ERR_NEW_TESTING_OK);
    return tox_new_system(options, error, sys);
}

void tox_kill(Tox *tox)
{
    if (tox == nullptr) {
        return;
    }

    tox_lock(tox);
    LOGGER_ASSERT(tox->m->log, tox->toxav_object == nullptr, "Attempted to kill tox while toxav is still alive");
    kill_groupchats(tox->m->conferences_object);
    kill_messenger(tox->m);
    mono_time_free(tox->sys.mem, tox->mono_time);
    tox_unlock(tox);

    if (tox->mutex != nullptr) {
        pthread_mutex_destroy(tox->mutex);
        mem_delete(tox->sys.mem, tox->mutex);
    }

    mem_delete(tox->sys.mem, tox);
}

static uint32_t end_size(void)
{
    return 2 * sizeof(uint32_t);
}

non_null()
static void end_save(uint8_t *data)
{
    state_write_section_header(data, STATE_COOKIE_TYPE, 0, STATE_TYPE_END);
}

size_t tox_get_savedata_size(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const size_t ret = 2 * sizeof(uint32_t)
                       + messenger_size(tox->m)
                       + conferences_size(tox->m->conferences_object)
                       + end_size();
    tox_unlock(tox);
    return ret;
}

void tox_get_savedata(const Tox *tox, uint8_t *savedata)
{
    assert(tox != nullptr);

    if (savedata == nullptr) {
        return;
    }

    memzero(savedata, tox_get_savedata_size(tox));

    tox_lock(tox);

    const uint32_t size32 = sizeof(uint32_t);

    // write cookie
    memzero(savedata, size32);
    savedata += size32;
    host_to_lendian_bytes32(savedata, STATE_COOKIE_GLOBAL);
    savedata += size32;

    savedata = messenger_save(tox->m, savedata);
    savedata = conferences_save(tox->m->conferences_object, savedata);
    end_save(savedata);

    tox_unlock(tox);
}

non_null(5) nullable(1, 2, 4, 6)
static int32_t resolve_bootstrap_node(Tox *tox, const char *host, uint16_t port, const uint8_t public_key[TOX_PUBLIC_KEY_SIZE],
                                      IP_Port **root, Tox_Err_Bootstrap *error)
{
    assert(tox != nullptr);
    assert(root != nullptr);

    if (host == nullptr || public_key == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_NULL);
        return -1;
    }

    if (port == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_BAD_PORT);
        return -1;
    }

    const int32_t count = net_getipport(tox->sys.mem, host, root, TOX_SOCK_DGRAM);

    if (count < 1) {
        LOGGER_DEBUG(tox->m->log, "could not resolve bootstrap node '%s'", host);
        net_freeipport(tox->sys.mem, *root);
        SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_BAD_HOST);
        return -1;
    }

    assert(*root != nullptr);
    return count;
}

bool tox_bootstrap(Tox *tox, const char *host, uint16_t port, const uint8_t public_key[TOX_PUBLIC_KEY_SIZE], Tox_Err_Bootstrap *error)
{
    IP_Port *root;
    const int32_t count = resolve_bootstrap_node(tox, host, port, public_key, &root, error);

    if (count == -1) {
        return false;
    }

    tox_lock(tox);
    assert(count >= 0);
    bool onion_success = false;
    // UDP bootstrap is default success if it's disabled (because we don't even try).
    bool udp_success = tox->m->options.udp_disabled;

    for (int32_t i = 0; i < count; ++i) {
        if (!tox->m->options.ipv6enabled && net_family_is_ipv6(root[i].ip.family)) {
            // We can't use ipv6 when it's disabled.
            continue;
        }

        root[i].port = net_htons(port);

        if (onion_add_bs_path_node(tox->m->onion_c, &root[i], public_key)) {
            // If UDP is enabled, the caller cares about whether any of the
            // bootstrap calls below will succeed. In TCP-only mode, adding
            // onion path nodes successfully is sufficient.
            onion_success = true;
        }

        if (!tox->m->options.udp_disabled) {
            if (dht_bootstrap(tox->m->dht, &root[i], public_key)) {
                // If any of the bootstrap calls worked, we call it success.
                udp_success = true;
            }
        }
    }

    tox_unlock(tox);

    net_freeipport(tox->sys.mem, root);

    if (count == 0 || !onion_success || !udp_success) {
        LOGGER_DEBUG(tox->m->log, "bootstrap node '%s' resolved to %d IP_Ports%s (onion: %s, UDP: %s)",
                     host, count,
                     count > 0 ? ", but failed to bootstrap with any of them" : "",
                     onion_success ? "success" : "FAILURE",
                     tox->m->options.udp_disabled ? "disabled" : (udp_success ? "success" : "FAILURE"));
        SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_BAD_HOST);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_OK);
    return true;
}

bool tox_add_tcp_relay(Tox *tox, const char *host, uint16_t port, const uint8_t public_key[TOX_PUBLIC_KEY_SIZE],
                       Tox_Err_Bootstrap *error)
{
    IP_Port *root;
    const int32_t count = resolve_bootstrap_node(tox, host, port, public_key, &root, error);

    if (count == -1) {
        return false;
    }

    tox_lock(tox);
    assert(count >= 0);

    for (int32_t i = 0; i < count; ++i) {
        root[i].port = net_htons(port);

        add_tcp_relay(tox->m->net_crypto, &root[i], public_key);
    }

    tox_unlock(tox);

    net_freeipport(tox->sys.mem, root);

    if (count == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_BAD_HOST);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_BOOTSTRAP_OK);
    return true;
}

Tox_Connection tox_self_get_connection_status(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const Onion_Connection_Status ret = onion_connection_status(tox->m->onion_c);
    tox_unlock(tox);

    switch (ret) {
        case ONION_CONNECTION_STATUS_NONE:
            return TOX_CONNECTION_NONE;

        case ONION_CONNECTION_STATUS_TCP:
            return TOX_CONNECTION_TCP;

        case ONION_CONNECTION_STATUS_UDP:
            return TOX_CONNECTION_UDP;
    }

    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);
    return TOX_CONNECTION_NONE;
}

void tox_callback_self_connection_status(Tox *tox, tox_self_connection_status_cb *callback)
{
    assert(tox != nullptr);
    tox->self_connection_status_callback = callback;
}

uint32_t tox_iteration_interval(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    uint32_t ret = messenger_run_interval(tox->m);

    if (m_is_receiving_file(tox->m)) {
        ret = 1;
    }

    tox_unlock(tox);
    return ret;
}

void tox_iterate(Tox *tox, void *user_data)
{
    assert(tox != nullptr);
    tox_lock(tox);

    mono_time_update(tox->mono_time);

    struct Tox_Userdata tox_data = { tox, user_data };
    do_messenger(tox->m, &tox_data);
    do_groupchats(tox->m->conferences_object, &tox_data);

    tox_unlock(tox);
}

void tox_self_get_address(const Tox *tox, uint8_t address[TOX_ADDRESS_SIZE])
{
    assert(tox != nullptr);

    if (address != nullptr) {
        tox_lock(tox);
        getaddress(tox->m, address);
        tox_unlock(tox);
    }
}

void tox_self_set_nospam(Tox *tox, uint32_t nospam)
{
    assert(tox != nullptr);
    tox_lock(tox);
    set_nospam(tox->m->fr, net_htonl(nospam));
    tox_unlock(tox);
}

uint32_t tox_self_get_nospam(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const uint32_t ret = net_ntohl(get_nospam(tox->m->fr));
    tox_unlock(tox);
    return ret;
}

void tox_self_get_public_key(const Tox *tox, uint8_t public_key[TOX_PUBLIC_KEY_SIZE])
{
    assert(tox != nullptr);

    if (public_key != nullptr) {
        tox_lock(tox);
        memcpy(public_key, nc_get_self_public_key(tox->m->net_crypto), CRYPTO_PUBLIC_KEY_SIZE);
        tox_unlock(tox);
    }
}

void tox_self_get_secret_key(const Tox *tox, uint8_t secret_key[TOX_SECRET_KEY_SIZE])
{
    assert(tox != nullptr);

    if (secret_key != nullptr) {
        tox_lock(tox);
        memcpy(secret_key, nc_get_self_secret_key(tox->m->net_crypto), CRYPTO_SECRET_KEY_SIZE);
        tox_unlock(tox);
    }
}

bool tox_self_set_name(Tox *tox, const uint8_t *name, size_t length, Tox_Err_Set_Info *error)
{
    assert(tox != nullptr);

    if (name == nullptr && length != 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_SET_INFO_NULL);
        return false;
    }

    tox_lock(tox);

    if (setname(tox->m, name, length) == 0) {
        // TODO(irungentoo): function to set different per group names?
        send_name_all_groups(tox->m->conferences_object);
        SET_ERROR_PARAMETER(error, TOX_ERR_SET_INFO_OK);
        tox_unlock(tox);
        return true;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_SET_INFO_TOO_LONG);
    tox_unlock(tox);
    return false;
}

size_t tox_self_get_name_size(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const size_t ret = m_get_self_name_size(tox->m);
    tox_unlock(tox);
    return ret;
}

void tox_self_get_name(const Tox *tox, uint8_t *name)
{
    assert(tox != nullptr);

    if (name != nullptr) {
        tox_lock(tox);
        getself_name(tox->m, name);
        tox_unlock(tox);
    }
}

bool tox_self_set_status_message(Tox *tox, const uint8_t *status_message, size_t length, Tox_Err_Set_Info *error)
{
    assert(tox != nullptr);

    if (status_message == nullptr && length != 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_SET_INFO_NULL);
        return false;
    }

    tox_lock(tox);

    if (m_set_statusmessage(tox->m, status_message, length) == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_SET_INFO_OK);
        tox_unlock(tox);
        return true;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_SET_INFO_TOO_LONG);
    tox_unlock(tox);
    return false;
}

size_t tox_self_get_status_message_size(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const size_t ret = m_get_self_statusmessage_size(tox->m);
    tox_unlock(tox);
    return ret;
}

void tox_self_get_status_message(const Tox *tox, uint8_t *status_message)
{
    assert(tox != nullptr);

    if (status_message != nullptr) {
        tox_lock(tox);
        m_copy_self_statusmessage(tox->m, status_message);
        tox_unlock(tox);
    }
}

void tox_self_set_status(Tox *tox, Tox_User_Status status)
{
    assert(tox != nullptr);
    tox_lock(tox);
    m_set_userstatus(tox->m, status);
    tox_unlock(tox);
}

Tox_User_Status tox_self_get_status(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const uint8_t status = m_get_self_userstatus(tox->m);
    tox_unlock(tox);
    return (Tox_User_Status)status;
}

non_null(1) nullable(3)
static void set_friend_error(const Logger *log, int32_t ret, Tox_Err_Friend_Add *error)
{
    switch (ret) {
        case FAERR_TOOLONG: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_TOO_LONG);
            break;
        }

        case FAERR_NOMESSAGE: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_NO_MESSAGE);
            break;
        }

        case FAERR_OWNKEY: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_OWN_KEY);
            break;
        }

        case FAERR_ALREADYSENT: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_ALREADY_SENT);
            break;
        }

        case FAERR_BADCHECKSUM: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_BAD_CHECKSUM);
            break;
        }

        case FAERR_SETNEWNOSPAM: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM);
            break;
        }

        case FAERR_NOMEM: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_MALLOC);
            break;
        }

        default: {
            /* can't happen */
            LOGGER_FATAL(log, "impossible return value: %d", ret);
            break;
        }
    }
}

uint32_t tox_friend_add(Tox *tox, const uint8_t address[TOX_ADDRESS_SIZE], const uint8_t *message, size_t length,
                        Tox_Err_Friend_Add *error)
{
    assert(tox != nullptr);

    if (address == nullptr || message == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_NULL);
        return UINT32_MAX;
    }

    tox_lock(tox);
    const int32_t ret = m_addfriend(tox->m, address, message, length);

    if (ret >= 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_OK);
        tox_unlock(tox);
        return (uint32_t)ret;
    }

    set_friend_error(tox->m->log, ret, error);
    tox_unlock(tox);
    return UINT32_MAX;
}

uint32_t tox_friend_add_norequest(Tox *tox, const uint8_t public_key[TOX_PUBLIC_KEY_SIZE], Tox_Err_Friend_Add *error)
{
    assert(tox != nullptr);

    if (public_key == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_NULL);
        return UINT32_MAX;
    }

    tox_lock(tox);
    const int32_t ret = m_addfriend_norequest(tox->m, public_key);

    if (ret >= 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_ADD_OK);
        tox_unlock(tox);
        return (uint32_t)ret;
    }

    set_friend_error(tox->m->log, ret, error);
    tox_unlock(tox);
    return UINT32_MAX;
}

bool tox_friend_delete(Tox *tox, uint32_t friend_number, Tox_Err_Friend_Delete *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = m_delfriend(tox->m, friend_number);
    tox_unlock(tox);

    // TODO(irungentoo): handle if realloc fails?
    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_DELETE_FRIEND_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_DELETE_OK);
    return true;
}

uint32_t tox_friend_by_public_key(const Tox *tox, const uint8_t public_key[TOX_PUBLIC_KEY_SIZE], Tox_Err_Friend_By_Public_Key *error)
{
    assert(tox != nullptr);

    if (public_key == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_BY_PUBLIC_KEY_NULL);
        return UINT32_MAX;
    }

    tox_lock(tox);
    const int32_t ret = getfriend_id(tox->m, public_key);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_BY_PUBLIC_KEY_NOT_FOUND);
        return UINT32_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_BY_PUBLIC_KEY_OK);
    assert(ret >= 0);
    return (uint32_t)ret;
}

bool tox_friend_get_public_key(const Tox *tox, uint32_t friend_number, uint8_t public_key[TOX_PUBLIC_KEY_SIZE],
                               Tox_Err_Friend_Get_Public_Key *error)
{
    assert(tox != nullptr);

    if (public_key == nullptr) {
        return false;
    }

    tox_lock(tox);

    if (get_real_pk(tox->m, friend_number, public_key) == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_GET_PUBLIC_KEY_FRIEND_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK);
    tox_unlock(tox);
    return true;
}

bool tox_friend_exists(const Tox *tox, uint32_t friend_number)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const bool ret = m_friend_exists(tox->m, friend_number);
    tox_unlock(tox);
    return ret;
}

uint64_t tox_friend_get_last_online(const Tox *tox, uint32_t friend_number, Tox_Err_Friend_Get_Last_Online *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const uint64_t timestamp = m_get_last_online(tox->m, friend_number);
    tox_unlock(tox);

    if (timestamp == UINT64_MAX) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_GET_LAST_ONLINE_FRIEND_NOT_FOUND);
        return UINT64_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_GET_LAST_ONLINE_OK);
    return timestamp;
}

size_t tox_self_get_friend_list_size(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const size_t ret = count_friendlist(tox->m);
    tox_unlock(tox);
    return ret;
}

void tox_self_get_friend_list(const Tox *tox, uint32_t *friend_list)
{
    assert(tox != nullptr);

    if (friend_list != nullptr) {
        tox_lock(tox);
        // TODO(irungentoo): size parameter?
        copy_friendlist(tox->m, friend_list, count_friendlist(tox->m));
        tox_unlock(tox);
    }
}

size_t tox_friend_get_name_size(const Tox *tox, uint32_t friend_number, Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = m_get_name_size(tox->m, friend_number);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        return SIZE_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    return ret;
}

bool tox_friend_get_name(const Tox *tox, uint32_t friend_number, uint8_t *name, Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);

    if (name == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_NULL);
        return false;
    }

    tox_lock(tox);
    const int ret = getname(tox->m, friend_number, name);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    return true;
}

void tox_callback_friend_name(Tox *tox, tox_friend_name_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_name_callback = callback;
}

size_t tox_friend_get_status_message_size(const Tox *tox, uint32_t friend_number, Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = m_get_statusmessage_size(tox->m, friend_number);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        return SIZE_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    return ret;
}

bool tox_friend_get_status_message(const Tox *tox, uint32_t friend_number, uint8_t *status_message,
                                   Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);

    if (status_message == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_NULL);
        return false;
    }

    tox_lock(tox);
    const int size = m_get_statusmessage_size(tox->m, friend_number);

    if (size == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const int ret = m_copy_statusmessage(tox->m, friend_number, status_message, size);
    LOGGER_ASSERT(tox->m->log, ret == size, "concurrency problem: friend status message changed");

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    tox_unlock(tox);
    return ret == size;
}

void tox_callback_friend_status_message(Tox *tox, tox_friend_status_message_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_status_message_callback = callback;
}

Tox_User_Status tox_friend_get_status(const Tox *tox, uint32_t friend_number, Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = m_get_userstatus(tox->m, friend_number);
    tox_unlock(tox);

    if (ret == USERSTATUS_INVALID) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        return TOX_USER_STATUS_NONE;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    return (Tox_User_Status)ret;
}

void tox_callback_friend_status(Tox *tox, tox_friend_status_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_status_callback = callback;
}

Tox_Connection tox_friend_get_connection_status(const Tox *tox, uint32_t friend_number, Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = m_get_friend_connectionstatus(tox->m, friend_number);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        return TOX_CONNECTION_NONE;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    return (Tox_Connection)ret;
}

void tox_callback_friend_connection_status(Tox *tox, tox_friend_connection_status_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_connection_status_callback = callback;
}

bool tox_friend_get_typing(const Tox *tox, uint32_t friend_number, Tox_Err_Friend_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = m_get_istyping(tox->m, friend_number);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_QUERY_OK);
    return ret != 0;
}

void tox_callback_friend_typing(Tox *tox, tox_friend_typing_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_typing_callback = callback;
}

bool tox_self_set_typing(Tox *tox, uint32_t friend_number, bool typing, Tox_Err_Set_Typing *error)
{
    assert(tox != nullptr);
    tox_lock(tox);

    if (m_set_usertyping(tox->m, friend_number, typing) == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_SET_TYPING_FRIEND_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_SET_TYPING_OK);
    tox_unlock(tox);
    return true;
}

non_null(1) nullable(3)
static void set_message_error(const Logger *log, int ret, Tox_Err_Friend_Send_Message *error)
{
    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_OK);
            break;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_FOUND);
            break;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_TOO_LONG);
            break;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_CONNECTED);
            break;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_SENDQ);
            break;
        }

        case -5: {
            LOGGER_FATAL(log, "impossible: Messenger and Tox disagree on message types");
            break;
        }

        default: {
            /* can't happen */
            LOGGER_FATAL(log, "impossible return value: %d", ret);
            break;
        }
    }
}

uint32_t tox_friend_send_message(Tox *tox, uint32_t friend_number, Tox_Message_Type type, const uint8_t *message,
                                 size_t length, Tox_Err_Friend_Send_Message *error)
{
    assert(tox != nullptr);

    if (message == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_NULL);
        return 0;
    }

    if (length == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_SEND_MESSAGE_EMPTY);
        return 0;
    }

    uint32_t message_id = 0;
    tox_lock(tox);
    set_message_error(tox->m->log, m_send_message_generic(tox->m, friend_number, type, message, length, &message_id),
                      error);
    tox_unlock(tox);
    return message_id;
}

void tox_callback_friend_read_receipt(Tox *tox, tox_friend_read_receipt_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_read_receipt_callback = callback;
}

void tox_callback_friend_request(Tox *tox, tox_friend_request_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_request_callback = callback;
}

void tox_callback_friend_message(Tox *tox, tox_friend_message_cb *callback)
{
    assert(tox != nullptr);
    tox->friend_message_callback = callback;
}

bool tox_hash(uint8_t hash[TOX_HASH_LENGTH], const uint8_t *data, size_t length)
{
    if (hash == nullptr || (data == nullptr && length != 0)) {
        return false;
    }

    crypto_sha256(hash, data, length);
    return true;
}

bool tox_file_control(Tox *tox, uint32_t friend_number, uint32_t file_number, Tox_File_Control control,
                      Tox_Err_File_Control *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = file_control(tox->m, friend_number, file_number, control);
    tox_unlock(tox);

    if (ret == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_OK);
        return true;
    }

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_NOT_FOUND);
            return false;
        }

        case -4: {
            /* can't happen (this code is returned if `control` is invalid type) */
            LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_ALREADY_PAUSED);
            return false;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_DENIED);
            return false;
        }

        case -7: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_NOT_PAUSED);
            return false;
        }

        case -8: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_CONTROL_SENDQ);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_file_seek(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position,
                   Tox_Err_File_Seek *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = file_seek(tox->m, friend_number, file_number, position);
    tox_unlock(tox);

    if (ret == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_OK);
        return true;
    }

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_FRIEND_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_FRIEND_NOT_CONNECTED);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_NOT_FOUND);
            return false;
        }

        case -4: // fall-through
        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_DENIED);
            return false;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_INVALID_POSITION);
            return false;
        }

        case -8: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEEK_SENDQ);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

void tox_callback_file_recv_control(Tox *tox, tox_file_recv_control_cb *callback)
{
    assert(tox != nullptr);
    tox->file_recv_control_callback = callback;
}

bool tox_file_get_file_id(const Tox *tox, uint32_t friend_number, uint32_t file_number, uint8_t file_id[TOX_FILE_ID_LENGTH],
                          Tox_Err_File_Get *error)
{
    assert(tox != nullptr);

    if (file_id == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_GET_NULL);
        return false;
    }

    tox_lock(tox);
    const int ret = file_get_id(tox->m, friend_number, file_number, file_id);
    tox_unlock(tox);

    if (ret == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_GET_OK);
        return true;
    }

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_GET_FRIEND_NOT_FOUND);
    } else {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_GET_NOT_FOUND);
    }

    return false;
}

uint32_t tox_file_send(Tox *tox, uint32_t friend_number, uint32_t kind, uint64_t file_size, const uint8_t file_id[TOX_FILE_ID_LENGTH],
                       const uint8_t *filename, size_t filename_length, Tox_Err_File_Send *error)
{
    assert(tox != nullptr);

    if (filename == nullptr && filename_length != 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_NULL);
        return UINT32_MAX;
    }

    uint8_t f_id[FILE_ID_LENGTH];

    if (file_id == nullptr) {
        /* Tox keys are 32 bytes like FILE_ID_LENGTH. */
        new_symmetric_key(tox->sys.rng, f_id);
        file_id = f_id;
    }

    tox_lock(tox);
    const long int file_num = new_filesender(tox->m, friend_number, kind, file_size, file_id, filename, filename_length);
    tox_unlock(tox);

    if (file_num >= 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_OK);
        return file_num;
    }

    switch (file_num) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND);
            return UINT32_MAX;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_NAME_TOO_LONG);
            return UINT32_MAX;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_TOO_MANY);
            return UINT32_MAX;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED);
            return UINT32_MAX;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %ld", file_num);

    return UINT32_MAX;
}

bool tox_file_send_chunk(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, const uint8_t *data,
                         size_t length, Tox_Err_File_Send_Chunk *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = send_file_data(tox->m, friend_number, file_number, position, data, length);
    tox_unlock(tox);

    if (ret == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_OK);
        return true;
    }

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_CONNECTED);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_NOT_FOUND);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_NOT_TRANSFERRING);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_INVALID_LENGTH);
            return false;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_SENDQ);
            return false;
        }

        case -7: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FILE_SEND_CHUNK_WRONG_POSITION);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

void tox_callback_file_chunk_request(Tox *tox, tox_file_chunk_request_cb *callback)
{
    assert(tox != nullptr);
    tox->file_chunk_request_callback = callback;
}

void tox_callback_file_recv(Tox *tox, tox_file_recv_cb *callback)
{
    assert(tox != nullptr);
    tox->file_recv_callback = callback;
}

void tox_callback_file_recv_chunk(Tox *tox, tox_file_recv_chunk_cb *callback)
{
    assert(tox != nullptr);
    tox->file_recv_chunk_callback = callback;
}

void tox_callback_conference_invite(Tox *tox, tox_conference_invite_cb *callback)
{
    assert(tox != nullptr);
    tox->conference_invite_callback = callback;
}

void tox_callback_conference_connected(Tox *tox, tox_conference_connected_cb *callback)
{
    assert(tox != nullptr);
    tox->conference_connected_callback = callback;
}

void tox_callback_conference_message(Tox *tox, tox_conference_message_cb *callback)
{
    assert(tox != nullptr);
    tox->conference_message_callback = callback;
}

void tox_callback_conference_title(Tox *tox, tox_conference_title_cb *callback)
{
    assert(tox != nullptr);
    tox->conference_title_callback = callback;
}

void tox_callback_conference_peer_name(Tox *tox, tox_conference_peer_name_cb *callback)
{
    assert(tox != nullptr);
    tox->conference_peer_name_callback = callback;
}

void tox_callback_conference_peer_list_changed(Tox *tox, tox_conference_peer_list_changed_cb *callback)
{
    assert(tox != nullptr);
    tox->conference_peer_list_changed_callback = callback;
}

uint32_t tox_conference_new(Tox *tox, Tox_Err_Conference_New *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = add_groupchat(tox->m->conferences_object, tox->sys.rng, GROUPCHAT_TYPE_TEXT);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_NEW_INIT);
        return UINT32_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_NEW_OK);
    return ret;
}

bool tox_conference_delete(Tox *tox, uint32_t conference_number, Tox_Err_Conference_Delete *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const bool ret = del_groupchat(tox->m->conferences_object, conference_number, true);
    tox_unlock(tox);

    if (!ret) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_DELETE_CONFERENCE_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_DELETE_OK);
    return true;
}

uint32_t tox_conference_peer_count(const Tox *tox, uint32_t conference_number, Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_number_peers(tox->m->conferences_object, conference_number, false);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
        return UINT32_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return ret;
}

size_t tox_conference_peer_get_name_size(const Tox *tox, uint32_t conference_number, uint32_t peer_number,
        Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peername_size(tox->m->conferences_object, conference_number, peer_number, false);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return -1;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return -1;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return ret;
}

bool tox_conference_peer_get_name(const Tox *tox, uint32_t conference_number, uint32_t peer_number, uint8_t *name,
                                  Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peername(tox->m->conferences_object, conference_number, peer_number, name, false);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return true;
}

bool tox_conference_peer_get_public_key(const Tox *tox, uint32_t conference_number, uint32_t peer_number,
                                        uint8_t public_key[TOX_PUBLIC_KEY_SIZE], Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peer_pubkey(tox->m->conferences_object, conference_number, peer_number, public_key, false);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return true;
}

bool tox_conference_peer_number_is_ours(const Tox *tox, uint32_t conference_number, uint32_t peer_number,
                                        Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peernumber_is_ours(tox->m->conferences_object, conference_number, peer_number);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_NO_CONNECTION);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return ret != 0;
}

uint32_t tox_conference_offline_peer_count(const Tox *tox, uint32_t conference_number,
        Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_number_peers(tox->m->conferences_object, conference_number, true);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
        return UINT32_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return ret;
}

size_t tox_conference_offline_peer_get_name_size(const Tox *tox, uint32_t conference_number,
        uint32_t offline_peer_number,
        Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peername_size(tox->m->conferences_object, conference_number, offline_peer_number, true);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return -1;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return -1;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return ret;
}

bool tox_conference_offline_peer_get_name(const Tox *tox, uint32_t conference_number, uint32_t offline_peer_number,
        uint8_t *name,
        Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peername(tox->m->conferences_object, conference_number, offline_peer_number, name, true);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return true;
}

bool tox_conference_offline_peer_get_public_key(const Tox *tox, uint32_t conference_number,
        uint32_t offline_peer_number,
        uint8_t public_key[TOX_PUBLIC_KEY_SIZE], Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_peer_pubkey(tox->m->conferences_object, conference_number, offline_peer_number, public_key, true);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return true;
}

uint64_t tox_conference_offline_peer_get_last_active(const Tox *tox, uint32_t conference_number,
        uint32_t offline_peer_number,
        Tox_Err_Conference_Peer_Query *error)
{
    assert(tox != nullptr);
    uint64_t last_active = UINT64_MAX;
    tox_lock(tox);
    const int ret = group_frozen_last_active(tox->m->conferences_object, conference_number, offline_peer_number,
                    &last_active);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_CONFERENCE_NOT_FOUND);
            return UINT64_MAX;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_PEER_NOT_FOUND);
            return UINT64_MAX;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_PEER_QUERY_OK);
    return last_active;
}

bool tox_conference_set_max_offline(Tox *tox, uint32_t conference_number,
                                    uint32_t max_offline,
                                    Tox_Err_Conference_Set_Max_Offline *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_set_max_frozen(tox->m->conferences_object, conference_number, max_offline);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SET_MAX_OFFLINE_CONFERENCE_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SET_MAX_OFFLINE_OK);
    return true;
}

bool tox_conference_invite(Tox *tox, uint32_t friend_number, uint32_t conference_number,
                           Tox_Err_Conference_Invite *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = invite_friend(tox->m->conferences_object, friend_number, conference_number);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_INVITE_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_INVITE_FAIL_SEND);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_INVITE_NO_CONNECTION);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_INVITE_OK);
    return true;
}

uint32_t tox_conference_join(Tox *tox, uint32_t friend_number, const uint8_t *cookie, size_t length,
                             Tox_Err_Conference_Join *error)
{
    assert(tox != nullptr);

    if (cookie == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_NULL);
        return UINT32_MAX;
    }

    tox_lock(tox);
    const int ret = join_groupchat(tox->m->conferences_object, friend_number, GROUPCHAT_TYPE_TEXT, cookie, length);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_INVALID_LENGTH);
            return UINT32_MAX;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_WRONG_TYPE);
            return UINT32_MAX;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_FRIEND_NOT_FOUND);
            return UINT32_MAX;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_DUPLICATE);
            return UINT32_MAX;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_INIT_FAIL);
            return UINT32_MAX;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_FAIL_SEND);
            return UINT32_MAX;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_JOIN_OK);
    return ret;
}

bool tox_conference_send_message(Tox *tox, uint32_t conference_number, Tox_Message_Type type, const uint8_t *message,
                                 size_t length, Tox_Err_Conference_Send_Message *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    int ret = 0;

    if (type == TOX_MESSAGE_TYPE_NORMAL) {
        ret = group_message_send(tox->m->conferences_object, conference_number, message, length);
    } else {
        ret = group_action_send(tox->m->conferences_object, conference_number, message, length);
    }

    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SEND_MESSAGE_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SEND_MESSAGE_TOO_LONG);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SEND_MESSAGE_NO_CONNECTION);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SEND_MESSAGE_FAIL_SEND);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_SEND_MESSAGE_OK);
    return true;
}

size_t tox_conference_get_title_size(const Tox *tox, uint32_t conference_number, Tox_Err_Conference_Title *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_title_get_size(tox->m->conferences_object, conference_number);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_CONFERENCE_NOT_FOUND);
            return -1;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_INVALID_LENGTH);
            return -1;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_OK);
    return ret;
}

bool tox_conference_get_title(const Tox *tox, uint32_t conference_number, uint8_t *title,
                              Tox_Err_Conference_Title *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_title_get(tox->m->conferences_object, conference_number, title);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_INVALID_LENGTH);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_OK);
    return true;
}

bool tox_conference_set_title(Tox *tox, uint32_t conference_number, const uint8_t *title, size_t length,
                              Tox_Err_Conference_Title *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_title_send(tox->m->conferences_object, conference_number, title, length);
    tox_unlock(tox);

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_CONFERENCE_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_INVALID_LENGTH);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_FAIL_SEND);
            return false;
        }
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_TITLE_OK);
    return true;
}

size_t tox_conference_get_chatlist_size(const Tox *tox)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const size_t ret = count_chatlist(tox->m->conferences_object);
    tox_unlock(tox);
    return ret;
}

void tox_conference_get_chatlist(const Tox *tox, uint32_t *chatlist)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const size_t list_size = count_chatlist(tox->m->conferences_object);
    copy_chatlist(tox->m->conferences_object, chatlist, list_size);
    tox_unlock(tox);
}

Tox_Conference_Type tox_conference_get_type(const Tox *tox, uint32_t conference_number,
        Tox_Err_Conference_Get_Type *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const int ret = group_get_type(tox->m->conferences_object, conference_number);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_GET_TYPE_CONFERENCE_NOT_FOUND);
        return (Tox_Conference_Type)ret;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_GET_TYPE_OK);
    return (Tox_Conference_Type)ret;
}

bool tox_conference_get_id(const Tox *tox, uint32_t conference_number, uint8_t id[TOX_CONFERENCE_ID_SIZE])
{
    assert(tox != nullptr);
    tox_lock(tox);
    const bool ret = conference_get_id(tox->m->conferences_object, conference_number, id);
    tox_unlock(tox);
    return ret;
}

// TODO(iphydf): Delete in 0.3.0.
bool tox_conference_get_uid(const Tox *tox, uint32_t conference_number, uint8_t uid[TOX_CONFERENCE_UID_SIZE])
{
    assert(tox != nullptr);
    return tox_conference_get_id(tox, conference_number, uid);
}

uint32_t tox_conference_by_id(const Tox *tox, const uint8_t id[TOX_CONFERENCE_ID_SIZE], Tox_Err_Conference_By_Id *error)
{
    assert(tox != nullptr);

    if (id == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_BY_ID_NULL);
        return UINT32_MAX;
    }

    tox_lock(tox);
    const int32_t ret = conference_by_id(tox->m->conferences_object, id);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_BY_ID_NOT_FOUND);
        return UINT32_MAX;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_BY_ID_OK);
    assert(ret >= 0);
    return (uint32_t)ret;
}

// TODO(iphydf): Delete in 0.3.0.
uint32_t tox_conference_by_uid(const Tox *tox, const uint8_t uid[TOX_CONFERENCE_UID_SIZE], Tox_Err_Conference_By_Uid *error)
{
    assert(tox != nullptr);
    Tox_Err_Conference_By_Id id_error;
    const uint32_t res = tox_conference_by_id(tox, uid, &id_error);

    switch (id_error) {
        case TOX_ERR_CONFERENCE_BY_ID_OK: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_BY_UID_OK);
            break;
        }

        case TOX_ERR_CONFERENCE_BY_ID_NULL: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_BY_UID_NULL);
            break;
        }

        case TOX_ERR_CONFERENCE_BY_ID_NOT_FOUND: {
            SET_ERROR_PARAMETER(error, TOX_ERR_CONFERENCE_BY_UID_NOT_FOUND);
            break;
        }
    }

    return res;
}

nullable(2)
static void set_custom_packet_error(int ret, Tox_Err_Friend_Custom_Packet *error)
{
    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_OK);
            break;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_FOUND);
            break;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_TOO_LONG);
            break;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_INVALID);
            break;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_CONNECTED);
            break;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_SENDQ);
            break;
        }
    }
}

// cppcheck-suppress constParameterPointer
bool tox_friend_send_lossy_packet(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length,
                                  Tox_Err_Friend_Custom_Packet *error)
{
    assert(tox != nullptr);

    if (data == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_NULL);
        return false;
    }

    if (length == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_EMPTY);
        return false;
    }

    if (data[0] < PACKET_ID_RANGE_LOSSY_START || data[0] > PACKET_ID_RANGE_LOSSY_END) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_INVALID);
        return false;
    }

    tox_lock(tox);
    const int ret = m_send_custom_lossy_packet(tox->m, friend_number, data, length);
    tox_unlock(tox);

    set_custom_packet_error(ret, error);

    return ret == 0;
}

void tox_callback_friend_lossy_packet(Tox *tox, tox_friend_lossy_packet_cb *callback)
{
    assert(tox != nullptr);

    /* start at PACKET_ID_RANGE_LOSSY_CUSTOM_START so ToxAV Packets are excluded */
    for (uint8_t i = PACKET_ID_RANGE_LOSSY_CUSTOM_START; i <= PACKET_ID_RANGE_LOSSY_END; ++i) {
        tox->friend_lossy_packet_callback_per_pktid[i] = callback;
    }
}

// cppcheck-suppress constParameterPointer
bool tox_friend_send_lossless_packet(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length,
                                     Tox_Err_Friend_Custom_Packet *error)
{
    assert(tox != nullptr);

    if (data == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_NULL);
        return false;
    }

    if (length == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_FRIEND_CUSTOM_PACKET_EMPTY);
        return false;
    }

    tox_lock(tox);
    const int ret = send_custom_lossless_packet(tox->m, friend_number, data, length);
    tox_unlock(tox);

    set_custom_packet_error(ret, error);

    return ret == 0;
}

void tox_callback_friend_lossless_packet(Tox *tox, tox_friend_lossless_packet_cb *callback)
{
    assert(tox != nullptr);

    for (uint8_t i = PACKET_ID_RANGE_LOSSLESS_CUSTOM_START; i <= PACKET_ID_RANGE_LOSSLESS_CUSTOM_END; ++i) {
        tox->friend_lossless_packet_callback_per_pktid[i] = callback;
    }
}

void tox_self_get_dht_id(const Tox *tox, uint8_t dht_id[TOX_PUBLIC_KEY_SIZE])
{
    assert(tox != nullptr);

    if (dht_id != nullptr) {
        tox_lock(tox);
        memcpy(dht_id, dht_get_self_public_key(tox->m->dht), CRYPTO_PUBLIC_KEY_SIZE);
        tox_unlock(tox);
    }
}

uint16_t tox_self_get_udp_port(const Tox *tox, Tox_Err_Get_Port *error)
{
    assert(tox != nullptr);
    tox_lock(tox);
    const uint16_t port = tox->m == nullptr || tox->m->net == nullptr ? 0 : net_htons(net_port(tox->m->net));
    tox_unlock(tox);

    if (port == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GET_PORT_NOT_BOUND);
        return 0;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GET_PORT_OK);
    return port;
}

uint16_t tox_self_get_tcp_port(const Tox *tox, Tox_Err_Get_Port *error)
{
    assert(tox != nullptr);
    tox_lock(tox);

    if (tox->m != nullptr && tox->m->tcp_server != nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GET_PORT_OK);
        const uint16_t ret = tox->m->options.tcp_server_port;
        tox_unlock(tox);
        return ret;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GET_PORT_NOT_BOUND);
    tox_unlock(tox);
    return 0;
}

/* GROUPCHAT FUNCTIONS */

void tox_callback_group_invite(Tox *tox, tox_group_invite_cb *callback)
{
    assert(tox != nullptr);
    tox->group_invite_callback = callback;
}

void tox_callback_group_message(Tox *tox, tox_group_message_cb *callback)
{
    assert(tox != nullptr);
    tox->group_message_callback = callback;
}

void tox_callback_group_private_message(Tox *tox, tox_group_private_message_cb *callback)
{
    assert(tox != nullptr);
    tox->group_private_message_callback = callback;
}

void tox_callback_group_custom_packet(Tox *tox, tox_group_custom_packet_cb *callback)
{
    assert(tox != nullptr);
    tox->group_custom_packet_callback = callback;
}

void tox_callback_group_custom_private_packet(Tox *tox, tox_group_custom_private_packet_cb *callback)
{
    assert(tox != nullptr);
    tox->group_custom_private_packet_callback = callback;
}

void tox_callback_group_moderation(Tox *tox, tox_group_moderation_cb *callback)
{
    assert(tox != nullptr);
    tox->group_moderation_callback = callback;
}

void tox_callback_group_peer_name(Tox *tox, tox_group_peer_name_cb *callback)
{
    assert(tox != nullptr);
    tox->group_peer_name_callback = callback;
}

void tox_callback_group_peer_status(Tox *tox, tox_group_peer_status_cb *callback)
{
    assert(tox != nullptr);
    tox->group_peer_status_callback = callback;
}

void tox_callback_group_topic(Tox *tox, tox_group_topic_cb *callback)
{
    assert(tox != nullptr);
    tox->group_topic_callback = callback;
}

void tox_callback_group_privacy_state(Tox *tox, tox_group_privacy_state_cb *callback)
{
    assert(tox != nullptr);
    tox->group_privacy_state_callback = callback;
}

void tox_callback_group_topic_lock(Tox *tox, tox_group_topic_lock_cb *callback)
{
    assert(tox != nullptr);
    tox->group_topic_lock_callback = callback;
}

void tox_callback_group_voice_state(Tox *tox, tox_group_voice_state_cb *callback)
{
    assert(tox != nullptr);
    tox->group_voice_state_callback = callback;
}

void tox_callback_group_peer_limit(Tox *tox, tox_group_peer_limit_cb *callback)
{
    assert(tox != nullptr);
    tox->group_peer_limit_callback = callback;
}

void tox_callback_group_password(Tox *tox, tox_group_password_cb *callback)
{
    assert(tox != nullptr);
    tox->group_password_callback = callback;
}

void tox_callback_group_peer_join(Tox *tox, tox_group_peer_join_cb *callback)
{
    assert(tox != nullptr);
    tox->group_peer_join_callback = callback;
}

void tox_callback_group_peer_exit(Tox *tox, tox_group_peer_exit_cb *callback)
{
    assert(tox != nullptr);
    tox->group_peer_exit_callback = callback;
}

void tox_callback_group_self_join(Tox *tox, tox_group_self_join_cb *callback)
{
    assert(tox != nullptr);
    tox->group_self_join_callback = callback;
}

void tox_callback_group_join_fail(Tox *tox, tox_group_join_fail_cb *callback)
{
    assert(tox != nullptr);
    tox->group_join_fail_callback = callback;
}

uint32_t tox_group_new(Tox *tox, Tox_Group_Privacy_State privacy_state, const uint8_t *group_name,
                       size_t group_name_length, const uint8_t *name, size_t name_length, Tox_Err_Group_New *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_group_add(tox->m->group_handler, (Group_Privacy_State) privacy_state,
                                 group_name, group_name_length, name, name_length);
    tox_unlock(tox);

    if (ret >= 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_NEW_OK);
        return ret;
    }

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_NEW_TOO_LONG);
            return UINT32_MAX;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_NEW_EMPTY);
            return UINT32_MAX;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_NEW_INIT);
            return UINT32_MAX;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_NEW_STATE);
            return UINT32_MAX;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_NEW_ANNOUNCE);
            return UINT32_MAX;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return UINT32_MAX;
}

uint32_t tox_group_join(Tox *tox, const uint8_t chat_id[TOX_GROUP_CHAT_ID_SIZE], const uint8_t *name, size_t name_length,
                        const uint8_t *password, size_t password_length, Tox_Err_Group_Join *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_group_join(tox->m->group_handler, chat_id, name, name_length, password, password_length);
    tox_unlock(tox);

    if (ret >= 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_OK);
        return ret;
    }

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_INIT);
            return UINT32_MAX;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_BAD_CHAT_ID);
            return UINT32_MAX;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_TOO_LONG);
            return UINT32_MAX;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_EMPTY);
            return UINT32_MAX;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_PASSWORD);
            return UINT32_MAX;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_JOIN_CORE);
            return UINT32_MAX;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return UINT32_MAX;
}

bool tox_group_is_connected(const Tox *tox, uint32_t group_number, Tox_Err_Group_Is_Connected *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_IS_CONNECTED_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_IS_CONNECTED_OK);

    const bool ret = chat->connection_state == CS_CONNECTED || chat->connection_state == CS_CONNECTING;
    tox_unlock(tox);

    return ret;
}

bool tox_group_disconnect(const Tox *tox, uint32_t group_number, Tox_Err_Group_Disconnect *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_DISCONNECT_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_DISCONNECT_ALREADY_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    const bool ret = gc_disconnect_from_group(tox->m->group_handler, chat);

    tox_unlock(tox);

    if (!ret) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_DISCONNECT_GROUP_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_DISCONNECT_OK);

    return true;
}

bool tox_group_reconnect(Tox *tox, uint32_t group_number, Tox_Err_Group_Reconnect *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_RECONNECT_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_rejoin_group(tox->m->group_handler, chat);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_RECONNECT_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_RECONNECT_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_RECONNECT_CORE);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_leave(Tox *tox, uint32_t group_number, const uint8_t *part_message, size_t length,
                     Tox_Err_Group_Leave *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_LEAVE_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_group_exit(tox->m->group_handler, chat, part_message, length);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_LEAVE_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_LEAVE_TOO_LONG);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_LEAVE_FAIL_SEND);
            return true;   /* the group was still successfully deleted */
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_self_set_name(Tox *tox, uint32_t group_number, const uint8_t *name, size_t length,
                             Tox_Err_Group_Self_Name_Set *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_set_self_nick(tox->m, group_number, name, length);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_NAME_SET_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_NAME_SET_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_NAME_SET_TOO_LONG);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_NAME_SET_INVALID);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_NAME_SET_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

size_t tox_group_self_get_name_size(const Tox *tox, uint32_t group_number, Tox_Err_Group_Self_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_OK);

    const size_t ret = gc_get_self_nick_size(chat);
    tox_unlock(tox);

    return ret;
}

bool tox_group_self_get_name(const Tox *tox, uint32_t group_number, uint8_t *name, Tox_Err_Group_Self_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_OK);

    gc_get_self_nick(chat, name);
    tox_unlock(tox);

    return true;
}

bool tox_group_self_set_status(Tox *tox, uint32_t group_number, Tox_User_Status status,
                               Tox_Err_Group_Self_Status_Set *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_set_self_status(tox->m, group_number, (Group_Peer_Status) status);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_STATUS_SET_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_STATUS_SET_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_STATUS_SET_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

Tox_User_Status tox_group_self_get_status(const Tox *tox, uint32_t group_number, Tox_Err_Group_Self_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_User_Status) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_OK);

    const uint8_t status = gc_get_self_status(chat);
    tox_unlock(tox);

    return (Tox_User_Status)status;
}

Tox_Group_Role tox_group_self_get_role(const Tox *tox, uint32_t group_number, Tox_Err_Group_Self_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_Group_Role) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_OK);

    const Group_Role role = gc_get_self_role(chat);
    tox_unlock(tox);

    return (Tox_Group_Role)role;
}

uint32_t tox_group_self_get_peer_id(const Tox *tox, uint32_t group_number, Tox_Err_Group_Self_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_OK);

    const GC_Peer_Id ret = gc_get_self_peer_id(chat);
    tox_unlock(tox);

    return gc_peer_id_to_int(ret);
}

bool tox_group_self_get_public_key(const Tox *tox, uint32_t group_number, uint8_t public_key[TOX_PUBLIC_KEY_SIZE],
                                   Tox_Err_Group_Self_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SELF_QUERY_OK);

    gc_get_self_public_key(chat, public_key);
    tox_unlock(tox);

    return true;
}

size_t tox_group_peer_get_name_size(const Tox *tox, uint32_t group_number, uint32_t peer_id,
                                    Tox_Err_Group_Peer_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    const int ret = gc_get_peer_nick_size(chat, gc_peer_id_from_int(peer_id));
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_PEER_NOT_FOUND);
        return -1;
    } else {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_OK);
        return ret;
    }
}

bool tox_group_peer_get_name(const Tox *tox, uint32_t group_number, uint32_t peer_id, uint8_t *name,
                             Tox_Err_Group_Peer_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const bool ret = gc_get_peer_nick(chat, gc_peer_id_from_int(peer_id), name);
    tox_unlock(tox);

    if (!ret) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_PEER_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_OK);
    return true;
}

Tox_User_Status tox_group_peer_get_status(const Tox *tox, uint32_t group_number, uint32_t peer_id,
        Tox_Err_Group_Peer_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_User_Status) - 1;
    }

    const uint8_t ret = gc_get_status(chat, gc_peer_id_from_int(peer_id));
    tox_unlock(tox);

    if (ret == UINT8_MAX) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_PEER_NOT_FOUND);
        return (Tox_User_Status) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_OK);
    return (Tox_User_Status)ret;
}

Tox_Group_Role tox_group_peer_get_role(const Tox *tox, uint32_t group_number, uint32_t peer_id,
                                       Tox_Err_Group_Peer_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_Group_Role) - 1;
    }

    const uint8_t ret = gc_get_role(chat, gc_peer_id_from_int(peer_id));
    tox_unlock(tox);

    if (ret == (uint8_t) -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_PEER_NOT_FOUND);
        return (Tox_Group_Role) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_OK);
    return (Tox_Group_Role)ret;
}

bool tox_group_peer_get_public_key(const Tox *tox, uint32_t group_number, uint32_t peer_id, uint8_t public_key[TOX_PUBLIC_KEY_SIZE],
                                   Tox_Err_Group_Peer_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_get_peer_public_key_by_peer_id(chat, gc_peer_id_from_int(peer_id), public_key);
    tox_unlock(tox);

    if (ret == -1) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_PEER_NOT_FOUND);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_OK);
    return true;
}

Tox_Connection tox_group_peer_get_connection_status(const Tox *tox, uint32_t group_number, uint32_t peer_id,
        Tox_Err_Group_Peer_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return TOX_CONNECTION_NONE;
    }

    const unsigned int ret = gc_get_peer_connection_status(chat, gc_peer_id_from_int(peer_id));
    tox_unlock(tox);

    if (ret == 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_PEER_NOT_FOUND);
        return TOX_CONNECTION_NONE;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_PEER_QUERY_OK);
    return (Tox_Connection)ret;
}

bool tox_group_set_topic(Tox *tox, uint32_t group_number, const uint8_t *topic, size_t length,
                         Tox_Err_Group_Topic_Set *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_set_topic(chat, topic, length);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_TOO_LONG);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_PERMISSIONS);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_FAIL_CREATE);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_TOPIC_SET_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

size_t tox_group_get_topic_size(const Tox *tox, uint32_t group_number, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const size_t ret = gc_get_topic_size(chat);
    tox_unlock(tox);

    return ret;
}

bool tox_group_get_topic(const Tox *tox, uint32_t group_number, uint8_t *topic, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    gc_get_topic(chat, topic);
    tox_unlock(tox);

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);
    return true;
}

size_t tox_group_get_name_size(const Tox *tox, uint32_t group_number, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const size_t ret = gc_get_group_name_size(chat);
    tox_unlock(tox);

    return ret;
}

bool tox_group_get_name(const Tox *tox, uint32_t group_number, uint8_t *name, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    gc_get_group_name(chat, name);
    tox_unlock(tox);

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    return true;
}

bool tox_group_get_chat_id(const Tox *tox, uint32_t group_number, uint8_t chat_id[TOX_GROUP_CHAT_ID_SIZE], Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);
    gc_get_chat_id(chat, chat_id);
    tox_unlock(tox);

    return true;
}

uint32_t tox_group_get_number_groups(const Tox *tox)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const uint32_t ret = gc_count_groups(tox->m->group_handler);
    tox_unlock(tox);

    return ret;
}

Tox_Group_Privacy_State tox_group_get_privacy_state(const Tox *tox, uint32_t group_number,
        Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_Group_Privacy_State) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const uint8_t state = gc_get_privacy_state(chat);
    tox_unlock(tox);

    return (Tox_Group_Privacy_State)state;
}

Tox_Group_Topic_Lock tox_group_get_topic_lock(const Tox *tox, uint32_t group_number, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_Group_Topic_Lock) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const Group_Topic_Lock topic_lock = gc_get_topic_lock_state(chat);
    tox_unlock(tox);

    return (Tox_Group_Topic_Lock)topic_lock;
}

Tox_Group_Voice_State tox_group_get_voice_state(const Tox *tox, uint32_t group_number,
        Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return (Tox_Group_Voice_State) - 1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const Group_Voice_State voice_state = gc_get_voice_state(chat);
    tox_unlock(tox);

    return (Tox_Group_Voice_State)voice_state;
}

uint16_t tox_group_get_peer_limit(const Tox *tox, uint32_t group_number, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const uint16_t ret = gc_get_max_peers(chat);
    tox_unlock(tox);

    return ret;
}

size_t tox_group_get_password_size(const Tox *tox, uint32_t group_number, Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    const size_t ret = gc_get_password_size(chat);
    tox_unlock(tox);

    return ret;
}

bool tox_group_get_password(const Tox *tox, uint32_t group_number, uint8_t *password,
                            Tox_Err_Group_State_Query *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_STATE_QUERY_OK);

    gc_get_password(chat, password);
    tox_unlock(tox);

    return true;
}

Tox_Group_Message_Id tox_group_send_message(
    const Tox *tox, uint32_t group_number, Tox_Message_Type message_type, const uint8_t *message,
    size_t length, Tox_Err_Group_Send_Message *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_DISCONNECTED);
        tox_unlock(tox);
        return -1;
    }

    uint32_t message_id = 0;
    const int ret = gc_send_message(chat, message, length, message_type, &message_id);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_OK);
            return message_id;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_TOO_LONG);
            return -1;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_EMPTY);
            return -1;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_BAD_TYPE);
            return -1;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_PERMISSIONS);
            return -1;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_MESSAGE_FAIL_SEND);
            return -1;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return -1;
}

Tox_Group_Message_Id tox_group_send_private_message(const Tox *tox, uint32_t group_number, uint32_t peer_id,
        Tox_Message_Type message_type, const uint8_t *message, size_t length, Tox_Err_Group_Send_Private_Message *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return -1;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_DISCONNECTED);
        tox_unlock(tox);
        return -1;
    }

    uint32_t message_id = 0;
    const int ret = gc_send_private_message(chat, gc_peer_id_from_int(peer_id), message_type, message, length, &message_id);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_OK);
            return message_id;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_TOO_LONG);
            return -1;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_EMPTY);
            return -1;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_PEER_NOT_FOUND);
            return -1;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_BAD_TYPE);
            return -1;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_PERMISSIONS);
            return -1;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_PRIVATE_MESSAGE_FAIL_SEND);
            return -1;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return -1;
}

bool tox_group_send_custom_packet(const Tox *tox, uint32_t group_number, bool lossless, const uint8_t *data,
                                  size_t length, Tox_Err_Group_Send_Custom_Packet *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PACKET_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PACKET_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_send_custom_packet(chat, lossless, data, length);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PACKET_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PACKET_TOO_LONG);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PACKET_EMPTY);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PACKET_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_send_custom_private_packet(const Tox *tox, uint32_t group_number, uint32_t peer_id, bool lossless,
        const uint8_t *data, size_t length,
        Tox_Err_Group_Send_Custom_Private_Packet *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_send_custom_private_packet(chat, lossless, gc_peer_id_from_int(peer_id), data, length);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_TOO_LONG);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_EMPTY);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_PEER_NOT_FOUND);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SEND_CUSTOM_PRIVATE_PACKET_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_invite_friend(const Tox *tox, uint32_t group_number, uint32_t friend_number,
                             Tox_Err_Group_Invite_Friend *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    if (!friend_is_valid(tox->m, friend_number)) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_FRIEND_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_invite_friend(tox->m->group_handler, chat, friend_number, send_group_invite_packet);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_FRIEND_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_INVITE_FAIL);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_FRIEND_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

uint32_t tox_group_invite_accept(Tox *tox, uint32_t friend_number, const uint8_t *invite_data, size_t length,
                                 const uint8_t *name, size_t name_length, const uint8_t *password,
                                 size_t password_length, Tox_Err_Group_Invite_Accept *error)
{
    assert(tox != nullptr);

    if (invite_data == nullptr || name == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_NULL);
        return UINT32_MAX;
    }

    tox_lock(tox);
    const int ret = gc_accept_invite(tox->m->group_handler, friend_number, invite_data, length, name, name_length, password,
                                     password_length);
    tox_unlock(tox);

    if (ret >= 0) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_OK);
        return ret;
    }

    switch (ret) {
        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_BAD_INVITE);
            return UINT32_MAX;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_INIT_FAILED);
            return UINT32_MAX;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_TOO_LONG);
            return UINT32_MAX;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_EMPTY);
            return UINT32_MAX;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_PASSWORD);
            return UINT32_MAX;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_FRIEND_NOT_FOUND);
            return UINT32_MAX;
        }

        case -7: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_INVITE_ACCEPT_FAIL_SEND);
            return UINT32_MAX;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return UINT32_MAX;
}

bool tox_group_set_password(Tox *tox, uint32_t group_number, const uint8_t *password, size_t length,
                            Tox_Err_Group_Set_Password *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_founder_set_password(chat, password, length);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_PERMISSIONS);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_TOO_LONG);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_FAIL_SEND);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PASSWORD_MALLOC);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_set_privacy_state(Tox *tox, uint32_t group_number, Tox_Group_Privacy_State privacy_state,
                                 Tox_Err_Group_Set_Privacy_State *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_founder_set_privacy_state(tox->m, group_number, (Group_Privacy_State) privacy_state);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PRIVACY_STATE_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PRIVACY_STATE_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PRIVACY_STATE_PERMISSIONS);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PRIVACY_STATE_DISCONNECTED);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PRIVACY_STATE_FAIL_SET);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PRIVACY_STATE_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_set_topic_lock(Tox *tox, uint32_t group_number, Tox_Group_Topic_Lock topic_lock,
                              Tox_Err_Group_Set_Topic_Lock *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_founder_set_topic_lock(tox->m, group_number, (Group_Topic_Lock) topic_lock);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_INVALID);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_PERMISSIONS);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_DISCONNECTED);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_FAIL_SET);
            return false;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_TOPIC_LOCK_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_set_voice_state(Tox *tox, uint32_t group_number, Tox_Group_Voice_State voice_state,
                               Tox_Err_Group_Set_Voice_State *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_founder_set_voice_state(tox->m, group_number, (Group_Voice_State)voice_state);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_VOICE_STATE_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_VOICE_STATE_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_VOICE_STATE_PERMISSIONS);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_VOICE_STATE_DISCONNECTED);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_VOICE_STATE_FAIL_SET);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_VOICE_STATE_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_set_peer_limit(Tox *tox, uint32_t group_number, uint16_t peer_limit,
                              Tox_Err_Group_Set_Peer_Limit *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PEER_LIMIT_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    if (chat->connection_state == CS_DISCONNECTED) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PEER_LIMIT_DISCONNECTED);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_founder_set_max_peers(chat, peer_limit);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PEER_LIMIT_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PEER_LIMIT_PERMISSIONS);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PEER_LIMIT_FAIL_SET);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_PEER_LIMIT_FAIL_SEND);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_set_ignore(Tox *tox, uint32_t group_number, uint32_t peer_id, bool ignore,
                          Tox_Err_Group_Set_Ignore *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const GC_Chat *chat = gc_get_group(tox->m->group_handler, group_number);

    if (chat == nullptr) {
        SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_IGNORE_GROUP_NOT_FOUND);
        tox_unlock(tox);
        return false;
    }

    const int ret = gc_set_ignore(chat, gc_peer_id_from_int(peer_id), ignore);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_IGNORE_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_IGNORE_PEER_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_IGNORE_SELF);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_set_role(Tox *tox, uint32_t group_number, uint32_t peer_id, Tox_Group_Role role,
                        Tox_Err_Group_Set_Role *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_set_peer_role(tox->m, group_number, gc_peer_id_from_int(peer_id), (Group_Role) role);
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_PEER_NOT_FOUND);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_PERMISSIONS);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_ASSIGNMENT);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_FAIL_ACTION);
            return false;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_SET_ROLE_SELF);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

bool tox_group_kick_peer(const Tox *tox, uint32_t group_number, uint32_t peer_id,
                         Tox_Err_Group_Kick_Peer *error)
{
    assert(tox != nullptr);

    tox_lock(tox);
    const int ret = gc_kick_peer(tox->m, group_number, gc_peer_id_from_int(peer_id));
    tox_unlock(tox);

    switch (ret) {
        case 0: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_OK);
            return true;
        }

        case -1: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_GROUP_NOT_FOUND);
            return false;
        }

        case -2: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_PEER_NOT_FOUND);
            return false;
        }

        case -3: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_PERMISSIONS);
            return false;
        }

        case -4: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_FAIL_ACTION);
            return false;
        }

        case -5: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_FAIL_SEND);
            return false;
        }

        case -6: {
            SET_ERROR_PARAMETER(error, TOX_ERR_GROUP_KICK_PEER_SELF);
            return false;
        }
    }

    /* can't happen */
    LOGGER_FATAL(tox->m->log, "impossible return value: %d", ret);

    return false;
}

const Tox_System *tox_get_system(Tox *tox)
{
    assert(tox != nullptr);
    return &tox->sys;
}
