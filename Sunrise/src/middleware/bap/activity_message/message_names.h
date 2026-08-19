#pragma once

#include <cstdint>
#include <string_view>

namespace sunrise::middleware::bap::activity_message {

/**
 * Names one activity message type.
 * The names are the binary's own where the route recovered them, so a row reads as the client
 * sees it rather than as a bare number. A type with no recovered name is drawn by number alone.
 * @param messageType Activity message type from the envelope.
 * @return The recovered name, or an empty view when the type has none.
 */
[[nodiscard]] constexpr std::string_view message_name(std::uint32_t messageType) noexcept {
    switch (messageType) {
    case 0:
        return "entity_slot_notification";
    case 1:
        return "global_state";
    case 3:
        return "join_request";
    case 5:
        return "sensor_auth";
    case 6:
        return "sensor_sense_update";
    case 8:
        return "request_activity_host";
    case 11:
        return "start_new_activity";
    case 12:
        return "replicate_membership";
    case 13:
        return "request_peer_reservation";
    case 14:
        return "release_peer_reservation";
    case 15:
        return "peer_leave_request";
    case 16:
        return "client_keepalive";
    case 18:
        return "state_refresh";
    case 19:
        return "incident";
    case 20:
        return "entity_slot_request_value";
    case 21:
        return "entity_slot_request_mask";
    case 22:
        return "client_authoritative_data";
    case 23:
        return "client_identity";
    case 26:
        return "authority_abandon";
    case 27:
        return "authority_request_purge";
    case 29:
        return "authority_reset_ack";
    case 31:
        return "authority_query_per_bubble";
    case 32:
        return "authority_query_response";
    case 33:
        return "authority_abdicate";
    case 34:
        return "process_debug_command";
    case 37:
        return "connectivity_failure";
    case 38:
        return "membership_ack";
    case 39:
        return "send_client_heartbeat";
    case 43:
        return "bug_claw";
    case 46:
        return "report_lag_switch";
    case 47:
        return "connection_quality_report";
    case 48:
        return "speculative_migration";
    case 49:
        return "high_water";
    case 50:
        return "refresh_inspirations";
    case 52:
        return "patch_epoch";
    default:
        return {};
    }
}

} // namespace sunrise::middleware::bap::activity_message
