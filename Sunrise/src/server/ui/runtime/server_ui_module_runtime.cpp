#include "server_ui_module_runtime.h"

#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/ui/modules/registry/ui_module_registry.h"
#include "../../../core/ui/modules/ui_module_descriptor.h"
#include "../activity_override/activity_override_panel.h"
#include "../receipts/receipts_panel.h"

namespace sunrise::server::ui::runtime {
namespace {

/** A namespaced stable ID keeps Server modules from clashing with Client modules. */
constexpr std::string_view kOverrideStableId = "server.activity_override";
/** Short menu label for the activity override page. */
constexpr std::string_view kOverrideDisplayName = "Activity";
/** A namespaced stable ID for the activity receipt registry view. */
constexpr std::string_view kReceiptsStableId = "server.activity_receipts";
/** Short menu label for the receipt registry page. */
constexpr std::string_view kReceiptsDisplayName = "Receipts";

core::ui::modules::registry::PageRegistration g_overridePage;
core::ui::modules::registry::PageRegistration g_receiptsPage;

} // namespace

/** @return True when the activity override page owns its Core UI registry slot. */
bool initialize() noexcept {
    if (!g_overridePage.acquire(core::ui::modules::Owner::server,
                                kOverrideStableId,
                                kOverrideDisplayName,
                                &activity_override::draw)) {
        return false;
    }
    // The filter state is cleared under the slot lock, so a re-register never draws a stale one.
    if (!g_receiptsPage.acquire(core::ui::modules::Owner::server,
                                kReceiptsStableId,
                                kReceiptsDisplayName,
                                &receipts::draw,
                                &receipts::reset)) {
        // Reported and left alone. A Server UI failure fails the whole Server layer, and a
        // diagnostic page is never worth the session it would cost.
        core::log::write(
            core::log::Channel::server, core::log::Level::warn, "ev=ui stage=receipts result=fail");
    }
    return true;
}

/** Removes both Server modules from the Core UI registry, in reverse order. */
void shutdown() noexcept {
    g_receiptsPage.release(&receipts::reset);
    g_overridePage.release();
}

} // namespace sunrise::server::ui::runtime
