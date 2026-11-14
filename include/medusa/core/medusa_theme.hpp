#pragma once
#include <medusa/medusa_types.hpp>

namespace medusa {

// Shared native DCC theme tokens. Refined deep obsidian palette with crisp,
// high-contrast visual hierarchy and modern typography.
struct Theme {
    static constexpr Color kChassisBg       = Color::from_hex(0x111214); // Deep neutral obsidian
    static constexpr Color kPanelBg         = Color::from_hex(0x17181B); // Sleek dark graphite panel surface
    static constexpr Color kPanelHeaderBg   = Color::from_hex(0x1E2024); // Defined distinct header & topbar row
    static constexpr Color kControlBg       = Color::from_hex(0x26282E); // Clean readable input container
    static constexpr Color kControlHover    = Color::from_hex(0x32353D);
    static constexpr Color kControlActive   = Color::from_hex(0x3B3F49);
    static constexpr Color kSelectionBg     = Color::from_hex(0x274972);

    static constexpr Color kBorderSubtle   = Color::from_hex(0x101113); // Structural panel borders
    static constexpr Color kBorderBright   = Color::from_hex(0x383B44); // Crisp control borders
    static constexpr Color kDockDivider   = Color::from_hex(0x34373F); // Visible seams between editor docks
    static constexpr Color kDockEdge      = Color::from_hex(0x0B0C0E); // Recessed edge beside dock dividers
    static constexpr Color kSplitterHover  = Color::from_hex(0x5682B8);

    static constexpr Color kTextPrimary   = Color::from_hex(0xF2F3F5); // Crisp high contrast text
    static constexpr Color kTextSecondary = Color::from_hex(0xA8ACB4); // Refined readable secondary text
    static constexpr Color kTextMuted     = Color::from_hex(0x6E727A); // Clean muted labels

    static constexpr Color kAccent        = Color::from_hex(0x5294E2); // Modern studio sapphire accent
    static constexpr Color kAccentBg      = Color::from_hex(0x285586);
    static constexpr Color kAccentHover   = Color::from_hex(0x336AA6);
    static constexpr Color kAccentBorder  = Color::from_hex(0x5A9EEA);
    static constexpr Color kAccentText    = Color::from_hex(0xF6F8FA);
    static constexpr Color kFocusRing     = Color::from_hex(0x62A0EC);

    static constexpr Color kViewportBg       = Color::from_hex(0x111214);
    static constexpr Color kViewportGrid     = Color::from_hex(0x222429);
    static constexpr Color kViewportGridMajor = Color::from_hex(0x353841);
    static constexpr Color kKeyframeGold   = Color::from_hex(0xE5BA5C);
    static constexpr Color kPlayheadBlue   = Color::from_hex(0xE88C56);
    static constexpr Color kChannelBg      = Color::from_hex(0x212327);
    static constexpr Color kTrackLaneEven  = Color::from_hex(0x1A1B1E);
    static constexpr Color kTrackLaneOdd   = Color::from_hex(0x1E2024);
    static constexpr Color kTrackGrid      = Color::from_hex(0x2C2F36);
    static constexpr Color kScrollbarTrack = Color::from_hex(0x141517);
    static constexpr Color kScrollbarThumb = Color::from_hex(0x4A4D56);

    // Axis colors remain semantic; they are not used for selection or focus.
    static constexpr Color kAxisX          = Color(0.820f, 0.320f, 0.320f, 1.0f);
    static constexpr Color kAxisY          = Color(0.320f, 0.740f, 0.380f, 1.0f);
    static constexpr Color kAxisZ          = Color(0.320f, 0.580f, 0.860f, 1.0f);
    static constexpr Color kSafeFrameMask  = Color(0.012f, 0.012f, 0.014f, 0.80f);

    // Compact application menu and workspace rows, followed by editor headers.
    static constexpr float kTopbarHeight = 28.0f;
    static constexpr float kAreaHeaderHeight = 25.0f;
    static constexpr float kTimelineHeaderHeight = 26.0f;
    static constexpr float kControlHeight = 22.0f;
    static constexpr float kOutlinerRowHeight = 24.0f;
    static constexpr float kUiFont = 12.0f;
    static constexpr float kPanelTitleFont = 12.5f;
    static constexpr float kSmallFont = 11.0f;
    static constexpr float kCornerRadius = 2.0f;
    static constexpr float kControlInset = 1.0f;
    static constexpr const char* kUiFontFamily = "Inter";
    static constexpr const char* kNumericFontFamily = "Cascadia Code";
};

} // namespace medusa
