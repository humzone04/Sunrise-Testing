#pragma once

namespace sunrise::server::ui::receipts {

/**
 * Draws the activity receipt registry inside the active Core UI frame.
 * The registry records every arriving activity message, so this is the only place a type that
 * arrives but is never fully framed becomes visible.
 */
void draw() noexcept;

/** Clears local filter state between UI lifecycles. */
void reset() noexcept;

} // namespace sunrise::server::ui::receipts
