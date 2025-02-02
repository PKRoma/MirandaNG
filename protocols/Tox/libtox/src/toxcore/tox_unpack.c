/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright © 2022 The TokTok team.
 */

#include "tox_unpack.h"

#include <stdint.h>

#include "attributes.h"
#include "bin_unpack.h"
#include "tox.h"

non_null()
static bool tox_conference_type_from_int(uint32_t value, Tox_Conference_Type *out_enum)
{
    switch (value) {
        case TOX_CONFERENCE_TYPE_TEXT: {
            *out_enum = TOX_CONFERENCE_TYPE_TEXT;
            return true;
        }

        case TOX_CONFERENCE_TYPE_AV: {
            *out_enum = TOX_CONFERENCE_TYPE_AV;
            return true;
        }

        default: {
            *out_enum = TOX_CONFERENCE_TYPE_TEXT;
            return false;
        }
    }
}
bool tox_conference_type_unpack(Tox_Conference_Type *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_conference_type_from_int(u32, val);
}

non_null()
static bool tox_connection_from_int(uint32_t value, Tox_Connection *out_enum)
{
    switch (value) {
        case TOX_CONNECTION_NONE: {
            *out_enum = TOX_CONNECTION_NONE;
            return true;
        }

        case TOX_CONNECTION_TCP: {
            *out_enum = TOX_CONNECTION_TCP;
            return true;
        }

        case TOX_CONNECTION_UDP: {
            *out_enum = TOX_CONNECTION_UDP;
            return true;
        }

        default: {
            *out_enum = TOX_CONNECTION_NONE;
            return false;
        }
    }
}

bool tox_connection_unpack(Tox_Connection *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_connection_from_int(u32, val);
}

non_null()
static bool tox_file_control_from_int(uint32_t value, Tox_File_Control *out_enum)
{
    switch (value) {
        case TOX_FILE_CONTROL_RESUME: {
            *out_enum = TOX_FILE_CONTROL_RESUME;
            return true;
        }

        case TOX_FILE_CONTROL_PAUSE: {
            *out_enum = TOX_FILE_CONTROL_PAUSE;
            return true;
        }

        case TOX_FILE_CONTROL_CANCEL: {
            *out_enum = TOX_FILE_CONTROL_CANCEL;
            return true;
        }

        default: {
            *out_enum = TOX_FILE_CONTROL_RESUME;
            return false;
        }
    }
}

bool tox_file_control_unpack(Tox_File_Control *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_file_control_from_int(u32, val);
}

non_null()
static bool tox_message_type_from_int(uint32_t value, Tox_Message_Type *out_enum)
{
    switch (value) {
        case TOX_MESSAGE_TYPE_NORMAL: {
            *out_enum = TOX_MESSAGE_TYPE_NORMAL;
            return true;
        }

        case TOX_MESSAGE_TYPE_ACTION: {
            *out_enum = TOX_MESSAGE_TYPE_ACTION;
            return true;
        }

        default: {
            *out_enum = TOX_MESSAGE_TYPE_NORMAL;
            return false;
        }
    }
}

bool tox_message_type_unpack(Tox_Message_Type *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_message_type_from_int(u32, val);
}

non_null()
static bool tox_user_status_from_int(uint32_t value, Tox_User_Status *out_enum)
{
    switch (value) {
        case TOX_USER_STATUS_NONE: {
            *out_enum = TOX_USER_STATUS_NONE;
            return true;
        }

        case TOX_USER_STATUS_AWAY: {
            *out_enum = TOX_USER_STATUS_AWAY;
            return true;
        }

        case TOX_USER_STATUS_BUSY: {
            *out_enum = TOX_USER_STATUS_BUSY;
            return true;
        }

        default: {
            *out_enum = TOX_USER_STATUS_NONE;
            return false;
        }
    }
}

bool tox_user_status_unpack(Tox_User_Status *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_user_status_from_int(u32, val);
}

non_null()
static bool tox_group_privacy_state_from_int(uint32_t value, Tox_Group_Privacy_State *out_enum)
{
    switch (value) {
        case TOX_GROUP_PRIVACY_STATE_PUBLIC: {
            *out_enum = TOX_GROUP_PRIVACY_STATE_PUBLIC;
            return true;
        }
        case TOX_GROUP_PRIVACY_STATE_PRIVATE: {
            *out_enum = TOX_GROUP_PRIVACY_STATE_PRIVATE;
            return true;
        }
        default: {
            *out_enum = TOX_GROUP_PRIVACY_STATE_PUBLIC;
            return false;
        }
    }
}
bool tox_group_privacy_state_unpack(Tox_Group_Privacy_State *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_group_privacy_state_from_int(u32, val);
}
non_null()
static bool tox_group_voice_state_from_int(uint32_t value, Tox_Group_Voice_State *out_enum)
{
    switch (value) {
        case TOX_GROUP_VOICE_STATE_ALL: {
            *out_enum = TOX_GROUP_VOICE_STATE_ALL;
            return true;
        }
        case TOX_GROUP_VOICE_STATE_MODERATOR: {
            *out_enum = TOX_GROUP_VOICE_STATE_MODERATOR;
            return true;
        }
        case TOX_GROUP_VOICE_STATE_FOUNDER: {
            *out_enum = TOX_GROUP_VOICE_STATE_FOUNDER;
            return true;
        }
        default: {
            *out_enum = TOX_GROUP_VOICE_STATE_ALL;
            return false;
        }
    }
}
bool tox_group_voice_state_unpack(Tox_Group_Voice_State *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_group_voice_state_from_int(u32, val);
}

non_null()
static bool tox_group_topic_lock_from_int(uint32_t value, Tox_Group_Topic_Lock *out_enum)
{
    switch (value) {
        case TOX_GROUP_TOPIC_LOCK_ENABLED: {
            *out_enum = TOX_GROUP_TOPIC_LOCK_ENABLED;
            return true;
        }
        case TOX_GROUP_TOPIC_LOCK_DISABLED: {
            *out_enum = TOX_GROUP_TOPIC_LOCK_DISABLED;
            return true;
        }
        default: {
            *out_enum = TOX_GROUP_TOPIC_LOCK_ENABLED;
            return false;
        }
    }
}
bool tox_group_topic_lock_unpack(Tox_Group_Topic_Lock *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_group_topic_lock_from_int(u32, val);
}

non_null()
static bool tox_group_join_fail_from_int(uint32_t value, Tox_Group_Join_Fail *out_enum)
{
    switch (value) {
        case TOX_GROUP_JOIN_FAIL_PEER_LIMIT: {
            *out_enum = TOX_GROUP_JOIN_FAIL_PEER_LIMIT;
            return true;
        }
        case TOX_GROUP_JOIN_FAIL_INVALID_PASSWORD: {
            *out_enum = TOX_GROUP_JOIN_FAIL_INVALID_PASSWORD;
            return true;
        }
        case TOX_GROUP_JOIN_FAIL_UNKNOWN: {
            *out_enum = TOX_GROUP_JOIN_FAIL_UNKNOWN;
            return true;
        }
        default: {
            *out_enum = TOX_GROUP_JOIN_FAIL_PEER_LIMIT;
            return false;
        }
    }
}
bool tox_group_join_fail_unpack(Tox_Group_Join_Fail *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_group_join_fail_from_int(u32, val);
}

non_null()
static bool tox_group_mod_event_from_int(uint32_t value, Tox_Group_Mod_Event *out_enum)
{
    switch (value) {
        case TOX_GROUP_MOD_EVENT_KICK: {
            *out_enum = TOX_GROUP_MOD_EVENT_KICK;
            return true;
        }
        case TOX_GROUP_MOD_EVENT_OBSERVER: {
            *out_enum = TOX_GROUP_MOD_EVENT_OBSERVER;
            return true;
        }
        case TOX_GROUP_MOD_EVENT_USER: {
            *out_enum = TOX_GROUP_MOD_EVENT_USER;
            return true;
        }
        case TOX_GROUP_MOD_EVENT_MODERATOR: {
            *out_enum = TOX_GROUP_MOD_EVENT_MODERATOR;
            return true;
        }
        default: {
            *out_enum = TOX_GROUP_MOD_EVENT_KICK;
            return false;
        }
    }
}
bool tox_group_mod_event_unpack(Tox_Group_Mod_Event *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_group_mod_event_from_int(u32, val);
}

non_null()
static bool tox_group_exit_type_from_int(uint32_t value, Tox_Group_Exit_Type *out_enum)
{
    switch (value) {
        case TOX_GROUP_EXIT_TYPE_QUIT: {
            *out_enum = TOX_GROUP_EXIT_TYPE_QUIT;
            return true;
        }
        case TOX_GROUP_EXIT_TYPE_TIMEOUT: {
            *out_enum = TOX_GROUP_EXIT_TYPE_TIMEOUT;
            return true;
        }
        case TOX_GROUP_EXIT_TYPE_DISCONNECTED: {
            *out_enum = TOX_GROUP_EXIT_TYPE_DISCONNECTED;
            return true;
        }
        case TOX_GROUP_EXIT_TYPE_SELF_DISCONNECTED: {
            *out_enum = TOX_GROUP_EXIT_TYPE_SELF_DISCONNECTED;
            return true;
        }
        case TOX_GROUP_EXIT_TYPE_KICK: {
            *out_enum = TOX_GROUP_EXIT_TYPE_KICK;
            return true;
        }
        case TOX_GROUP_EXIT_TYPE_SYNC_ERROR: {
            *out_enum = TOX_GROUP_EXIT_TYPE_SYNC_ERROR;
            return true;
        }
        default: {
            *out_enum = TOX_GROUP_EXIT_TYPE_QUIT;
            return false;
        }
    }
}
bool tox_group_exit_type_unpack(Tox_Group_Exit_Type *val, Bin_Unpack *bu)
{
    uint32_t u32;
    return bin_unpack_u32(bu, &u32)
           && tox_group_exit_type_from_int(u32, val);
}
