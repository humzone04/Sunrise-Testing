/**
 * The activity receipt registry's interface. Nothing here is saved, and nothing it draws changes
 * State. The rows are framing facts the router recorded, so this page is the protocol coverage
 * map: a type that arrives but never frames cleanly is visible here and nowhere else.
 */

#include "receipts_panel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <imgui.h>
#include <string_view>

#include "../../../core/ui/components/filter/ui_filter_component.h"
#include "../../../core/ui/components/label/ui_label_component.h"
#include "../../../core/ui/components/section/ui_section_component.h"
#include "../../../core/ui/components/toggle/ui_toggle_component.h"
#include "../../../core/ui/scaling/dpi/ui_dpi_scaling.h"
#include "../../../middleware/bap/activity_message/message_names.h"
#include "../../../state/activity/receipts/activity_receipts.h"

namespace sunrise::server::ui::receipts {
namespace {

namespace store = state::activity::receipts;
namespace names = middleware::bap::activity_message;
namespace filter = core::ui::components::filter;
namespace label = core::ui::components::label;
namespace section = core::ui::components::section;
namespace toggle = core::ui::components::toggle;

/** Eight bits per declared payload byte, used to compare framing against body width. */
constexpr std::uint32_t kBitsPerByte = 8;
/** 64 bytes hold the longest recovered name and a filter the user can type. */
constexpr std::size_t kFilterCapacity = 64;
/** Widest formatted type number, with room for a terminator. */
constexpr std::size_t kNumberCapacity = 16;
/** Columns: type, name, arrivals, verdict, coverage, last order. */
constexpr int kColumnCount = 6;
/** 320 authored pixels fit the name filter without crowding the toggle beside it. */
constexpr float kFilterWidth = 320.0F;
/** 240 authored pixels fit the unseen-row toggle and its label. */
constexpr float kToggleWidth = 240.0F;
/** Room for one formatted summary line. */
constexpr std::size_t kSummaryCapacity = 160;
/** Room for one formatted coverage label. */
constexpr std::size_t kCoverageCapacity = 32;
/** Zero size lets the table child take all the space left below the toolbar. */
constexpr ImVec2 kAutomaticChildSize{0.0F, 0.0F};
/** A full-width progress bar with the height its own text needs. */
constexpr ImVec2 kCoverageBarSize{-1.0F, 0.0F};
/** Rows scroll, alternate, and keep their column rules. */
constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                        | ImGuiTableFlags_ScrollY
                                        | ImGuiTableFlags_SizingStretchProp;

/** Fixed action label for the explicit dump. */
constexpr char kDumpLabel[] = "Dump to Log";
/** Names the button in the dump block, so a manual reading is distinct from the automatic one. */
constexpr std::string_view kManualReason = "manual";
/** Room for the one status line the dump action reports. */
constexpr std::size_t kStatusCapacity = 48;
/** Row count standing for "no dump has run this UI lifecycle". */
constexpr std::uint32_t kNoDump = 0xFFFFFFFFU;

/** User filter over the recovered name and the type number. */
std::array<char, kFilterCapacity> g_filter{};
/** User setting: leave out every type no message has arrived for. */
bool g_hideUnseen{true};
/** Rows the last explicit dump wrote, or kNoDump before one runs. */
std::uint32_t g_lastDumpRows{kNoDump};

/**
 * Colours a verdict so an incomplete row is findable without reading every cell.
 * @param verdict Row verdict.
 * @return The colour its text is drawn in.
 */
[[nodiscard]] ImVec4 verdict_colour(store::Verdict verdict) noexcept {
    switch (verdict) {
    case store::Verdict::framed:
        return ImVec4{0.45F, 0.85F, 0.50F, 1.0F};
    case store::Verdict::partial:
        return ImVec4{0.95F, 0.80F, 0.35F, 1.0F};
    case store::Verdict::malformed:
        return ImVec4{0.95F, 0.40F, 0.40F, 1.0F};
    case store::Verdict::quarantined:
        return ImVec4{0.55F, 0.75F, 0.95F, 1.0F};
    case store::Verdict::unowned:
        return ImVec4{0.85F, 0.55F, 0.95F, 1.0F};
    case store::Verdict::absent:
    default:
        return ImVec4{0.55F, 0.55F, 0.55F, 1.0F};
    }
}

/**
 * Answers whether one row survives the current text filter.
 * @param messageType Row's activity message type.
 * @param name Recovered name, or an empty view when the type has none.
 * @return True when the filter is empty or matches the name or the number.
 */
[[nodiscard]] bool matches_filter(std::uint32_t messageType, std::string_view name) noexcept {
    const std::string_view needle{g_filter.data()};
    if (needle.empty()) {
        return true;
    }
    if (name.find(needle) != std::string_view::npos) {
        return true;
    }
    std::array<char, kNumberCapacity> number{};
    const int written = std::snprintf(number.data(), number.size(), "%u", messageType);
    if (written <= 0) {
        return false;
    }
    const std::string_view text{number.data(), static_cast<std::size_t>(written)};
    return text.find(needle) != std::string_view::npos;
}

/**
 * Draws the coverage cell: how much of the last body the parser actually consumed.
 * A row below full width still has unrecovered grammar, which is the whole point of the page.
 * @param receipt Row copied from the registry.
 */
void draw_coverage(const store::ActivityReceipt& receipt) noexcept {
    const std::uint32_t declaredBits = receipt.lastPayloadBytes * kBitsPerByte;
    if (receipt.arrivals == 0 || declaredBits == 0) {
        ImGui::TextDisabled("--");
        return;
    }
    const float fraction =
        static_cast<float>(receipt.lastConsumedBits) / static_cast<float>(declaredBits);
    std::array<char, kCoverageCapacity> text{};
    const int written = std::snprintf(
        text.data(), text.size(), "%u of %u bits", receipt.lastConsumedBits, declaredBits);
    if (written <= 0) {
        ImGui::TextDisabled("--");
        return;
    }
    ImGui::ProgressBar(fraction, kCoverageBarSize, text.data());
}

/**
 * Draws one registry row.
 * @param messageType Activity message type the row is indexed by.
 * @param receipt Row copied from the registry.
 */
void draw_row(std::uint32_t messageType, const store::ActivityReceipt& receipt) noexcept {
    const std::string_view name = names::message_name(messageType);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%u", messageType);
    ImGui::TableNextColumn();
    if (name.empty()) {
        ImGui::TextDisabled("unrecovered");
    } else {
        ImGui::TextUnformatted(name.data(), name.data() + name.size());
    }
    ImGui::TableNextColumn();
    ImGui::Text("%u", receipt.arrivals);
    ImGui::TableNextColumn();
    ImGui::TextColored(
        verdict_colour(receipt.lastVerdict), "%s", store::verdict_name(receipt.lastVerdict));
    ImGui::TableNextColumn();
    draw_coverage(receipt);
    ImGui::TableNextColumn();
    if (receipt.arrivals == 0) {
        ImGui::TextDisabled("--");
    } else {
        ImGui::Text("%u", receipt.lastSequence);
    }
}

/** Draws the filter row above the table. */
void draw_toolbar() noexcept {
    label::align();
    ImGui::SetNextItemWidth(core::ui::scaling::dpi::pixels(kFilterWidth));
    (void)filter::input(
        "##sunrise_receipt_filter", "filter by name or type", g_filter.data(), g_filter.size());
    ImGui::SameLine();
    (void)toggle::control(
        "Hide types never seen", g_hideUnseen, core::ui::scaling::dpi::pixels(kToggleWidth));
}

/**
 * Draws the explicit dump action and the result of the last one.
 * The same block is written at shutdown, so this only matters mid-session, where it is the one
 * way to keep a reading taken before the run ends.
 */
void draw_dump_action() noexcept {
    label::align();
    if (ImGui::Button(kDumpLabel)) {
        g_lastDumpRows = store::report(kManualReason);
    }
    if (g_lastDumpRows == kNoDump) {
        return;
    }
    std::array<char, kStatusCapacity> status{};
    const int written =
        std::snprintf(status.data(), status.size(), "Wrote %u rows to the log.", g_lastDumpRows);
    if (written <= 0) {
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", status.data());
}

/**
 * Draws the one-line registry summary above the toolbar.
 * @param seenCount Types at least one message has arrived for.
 * @param arrivals Messages recorded across every type.
 */
void draw_summary(std::size_t seenCount, std::uint32_t arrivals) noexcept {
    std::array<char, kSummaryCapacity> summary{};
    const int written = std::snprintf(summary.data(),
                                      summary.size(),
                                      "%zu types seen   %u messages   %u unframed",
                                      seenCount,
                                      arrivals,
                                      store::unframed_total());
    if (written <= 0) {
        return;
    }
    label::align();
    ImGui::TextDisabled("%s", summary.data());
}

} // namespace

/** Draws the activity receipt registry inside the active Core UI frame. */
void draw() noexcept {
    section::header("Activity Receipts",
                    "One row per activity message type, as the router framed it.");

    std::array<store::ActivityReceipt, store::kReceiptCapacity> rows{};
    std::array<bool, store::kReceiptCapacity> seen{};
    std::uint32_t arrivals = 0;
    std::size_t seenCount = 0;
    for (std::size_t index = 0; index < store::kReceiptCapacity; ++index) {
        const auto messageType = static_cast<std::uint32_t>(index);
        seen[index] = store::snapshot(messageType, rows[index]);
        if (seen[index] && rows[index].arrivals != 0) {
            arrivals += rows[index].arrivals;
            ++seenCount;
        }
    }

    draw_summary(seenCount, arrivals);
    draw_toolbar();
    draw_dump_action();

    if (!ImGui::BeginChild(
            "##sunrise_receipt_child", kAutomaticChildSize, ImGuiChildFlags_Borders)) {
        ImGui::EndChild();
        return;
    }
    if (ImGui::BeginTable("##sunrise_receipt_table", kColumnCount, kTableFlags)) {
        ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("arrivals", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("verdict", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("coverage");
        ImGui::TableSetupColumn("last", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (std::size_t index = 0; index < store::kReceiptCapacity; ++index) {
            const auto messageType = static_cast<std::uint32_t>(index);
            const bool unseen = !seen[index] || rows[index].arrivals == 0;
            if (g_hideUnseen && unseen) {
                continue;
            }
            if (!matches_filter(messageType, names::message_name(messageType))) {
                continue;
            }
            draw_row(messageType, rows[index]);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

/** Clears local filter state between UI lifecycles. */
void reset() noexcept {
    g_filter = {};
    g_hideUnseen = true;
    g_lastDumpRows = kNoDump;
}

} // namespace sunrise::server::ui::receipts
