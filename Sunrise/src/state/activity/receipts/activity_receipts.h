#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "definition.h"

namespace sunrise::state::activity::receipts {

/** One arriving activity message, as the router framed it. */
struct Arrival {
    std::uint64_t sessionId{};
    std::uint32_t messageType{};
    std::uint32_t payloadBytes{};
    std::uint32_t peerHeardMask{};
    /** Bits the parser consumed. Zero when the type has no parser of its own. */
    std::uint32_t consumedBits{};
    Verdict verdict{Verdict::framed};
};

/**
 * Records one arriving activity message.
 * Every routed message calls this, including the ones that change nothing, so a type that stops
 * arriving is visible instead of silent.
 * @param arrival Framing facts for the message that just arrived.
 * @return The registry-wide arrival order stamped into the row.
 */
std::uint32_t record(const Arrival& arrival) noexcept;

/**
 * Copies one message type's arrival record.
 * @param messageType Activity message type.
 * @param receipt Cleared, then filled from the row.
 * @return True when a message of that type has arrived.
 */
[[nodiscard]] bool snapshot(std::uint32_t messageType, ActivityReceipt& receipt) noexcept;

/** @param verdict Recorded verdict. @return Its short name, for a log line or a table cell. */
[[nodiscard]] const char* verdict_name(Verdict verdict) noexcept;

/**
 * Writes every arrived row to the log as one block of structured lines.
 * The registry is process-local and is erased with the rest of State, so a run's coverage is
 * kept only by writing it out. Types no message arrived for are left out, which keeps the block
 * to the handful of types one session actually exercises. Emitted on the State channel at info.
 *
 * One `stage=dump` line opens the block and names what asked for it. Two blocks otherwise look
 * alike, so without it a reader cannot tell an automatic dump from one a person asked for, and
 * cannot tell that an automatic one never ran.
 *
 * @param reason Short lowercase name of what asked for the dump, for that opening line.
 * @return Rows written, not counting the opening line.
 */
std::uint32_t report(std::string_view reason) noexcept;

/** @return Messages recorded with a verdict other than framed, across every type. */
[[nodiscard]] std::uint32_t unframed_total() noexcept;

} // namespace sunrise::state::activity::receipts
