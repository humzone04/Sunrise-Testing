#include "activity_receipts.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../middleware/bap/activity_message/message_names.h"
#include "../../runtime/storage/internal.h"

namespace sunrise::state::activity::receipts {
namespace {

/** Eight bits per declared payload byte, so a line compares framing against body width. */
constexpr std::uint32_t kBitsPerByte = 8;
/** Stands in for a type whose name the route has not recovered. */
constexpr std::string_view kUnnamed = "unrecovered";

/** @return The row one message type is counted in. Types above the table share the overflow row. */
[[nodiscard]] std::size_t row_for(std::uint32_t messageType) noexcept {
    return messageType < kOverflowRow ? static_cast<std::size_t>(messageType) : kOverflowRow;
}

/**
 * Writes one row as a structured event.
 * @param messageType Activity message type the row is indexed by.
 * @param row Row already copied out from under the State lock.
 * @return True when the line was formatted and emitted.
 */
[[nodiscard]] bool report_row(std::uint32_t messageType, const ActivityReceipt& row) noexcept {
    std::string_view name = middleware::bap::activity_message::message_name(messageType);
    if (name.empty()) {
        name = kUnnamed;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=receipt type=%u name=%.*s arrivals=%u verdict=%s "
                                      "consumed=%u declared=%u unframed=%u",
                                      messageType,
                                      static_cast<int>(name.size()),
                                      name.data(),
                                      row.arrivals,
                                      verdict_name(row.lastVerdict),
                                      row.lastConsumedBits,
                                      row.lastPayloadBytes * kBitsPerByte,
                                      row.unframed);
    if (written <= 0) {
        return false;
    }
    core::log::write(core::log::Channel::state,
                     core::log::Level::info,
                     {line.data(), static_cast<std::size_t>(written)});
    return true;
}

} // namespace

/** Records one arriving activity message. */
std::uint32_t record(const Arrival& arrival) noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ReceiptRegistry& registry = runtime::storage::g_state.activity.receipts;
    // The counter orders arrivals and nothing reads it as a clock, so wrapping loses only order
    // across a wrap and never invalidates a row.
    ++registry.sequence;
    const std::uint32_t sequence = registry.sequence;
    ActivityReceipt& row = registry.rows[row_for(arrival.messageType)];
    ++row.arrivals;
    row.lastSequence = sequence;
    row.lastSessionId = arrival.sessionId;
    row.lastPayloadBytes = arrival.payloadBytes;
    row.lastPeerHeardMask = arrival.peerHeardMask;
    row.lastConsumedBits = arrival.consumedBits;
    row.lastVerdict = arrival.verdict;
    if (arrival.verdict != Verdict::framed) {
        ++row.unframed;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return sequence;
}

/** Copies one message type's arrival record. */
bool snapshot(std::uint32_t messageType, ActivityReceipt& receipt) noexcept {
    receipt = {};
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityReceipt& row =
        runtime::storage::g_state.activity.receipts.rows[row_for(messageType)];
    const bool seen = row.arrivals != 0;
    if (seen) {
        receipt = row;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return seen;
}

/** @return One verdict's short name, for a log line or a table cell. */
const char* verdict_name(Verdict verdict) noexcept {
    switch (verdict) {
    case Verdict::framed:
        return "framed";
    case Verdict::partial:
        return "partial";
    case Verdict::malformed:
        return "malformed";
    case Verdict::quarantined:
        return "quarantined";
    case Verdict::unowned:
        return "unowned";
    case Verdict::absent:
    default:
        return "absent";
    }
}

/** Writes every arrived row to the log as one block of structured lines. */
std::uint32_t report(std::string_view reason) noexcept {
    std::array<char, core::log::kLineCapacity> opening{};
    const int openingLength = std::snprintf(opening.data(),
                                            opening.size(),
                                            "ev=receipt stage=dump reason=%.*s",
                                            static_cast<int>(reason.size()),
                                            reason.data());
    if (openingLength > 0) {
        core::log::write(core::log::Channel::state,
                         core::log::Level::info,
                         {opening.data(), static_cast<std::size_t>(openingLength)});
    }

    std::uint32_t written = 0;
    for (std::size_t index = 0; index < kReceiptCapacity; ++index) {
        const auto messageType = static_cast<std::uint32_t>(index);
        ActivityReceipt row{};
        // One row is copied at a time, so the State lock is never held across a sink write.
        if (snapshot(messageType, row) && report_row(messageType, row)) {
            ++written;
        }
    }
    return written;
}

/** Counts messages recorded with a verdict other than framed. */
std::uint32_t unframed_total() noexcept {
    std::uint32_t total = 0;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    for (const ActivityReceipt& row : runtime::storage::g_state.activity.receipts.rows) {
        total += row.unframed;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return total;
}

} // namespace sunrise::state::activity::receipts
