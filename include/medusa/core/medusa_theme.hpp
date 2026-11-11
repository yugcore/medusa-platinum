#pragma once
#include <medusa/medusa_types.hpp>

namespace medusa {

// Shared native DCC theme tokens. The surfaces stay graphite; selection and
// animation retain the clear, semantic colors expected in a dense editor.
struct Theme {
    static constexpr Color kChassisBg       = Color::from_hex(0x191A1B);
    static constexpr Color kPanelBg         = Color::from_hex(0x222324);
    static constexpr Color kPanelHeaderBg   = Color::from_hex(0x2B2C2E);
    static constexpr Color kControlBg       = Color::from_hex(0x353638);
    static constexpr Color kControlHover    = Color::from_hex(0x414346);
    static constexpr Color kControlActive   = Color::from_hex(0x484B50);
    static constexpr Color kSelectionBg     = Color::from_hex(0x354D6A);

    static constexpr Color kBorderSubtle   = Color::from_hex(0x18191A);
    static constexpr Color kBorderBright   = Color::from_hex(0x4B4D50);
    static constexpr Color kSplitterHover  = Color::from_hex(0x6E91BA);

    static constexpr Color kTextPrimary   = Color::from_hex(0xE5E6E8);
    static constexpr Color kTextSecondary = Color::from_hex(0xB5B7BA);
    static constexpr Color kTextMuted     = Color::from_hex(0x828589);

    static constexpr Color kAccent        = Color::from_hex(0x6F9BCB);
    static constexpr Color kAccentBg      = Color::from_hex(0x405F83);
    static constexpr Color kAccentHover   = Color::from_hex(0x4B6F97);
    static constexpr Color kAccentBorder  = Color::from_hex(0x6E91BA);
    static constexpr Color kAccentText    = Color::from_hex(0xF1F3F5);
    static constexpr Color kFocusRing     = Color::from_hex(0x79A9DE);

    static constexpr Color kViewportBg       = Color::from_hex(0x1A1B1C);
    static constexpr Color kViewportGrid     = Color::from_hex(0x303234);
    static constexpr Color kViewportGridMajor = Color::from_hex(0x484A4D);
    static constexpr Color kKeyframeGold   = Color::from_hex(0xE1B85A);
    static constexpr Color kPlayheadBlue   = Color::from_hex(0xE58A54);
    static constexpr Color kChannelBg      = Color::from_hex(0x292A2C);
    static constexpr Color kTrackLaneEven  = Color::from_hex(0x232426);
    static constexpr Color kTrackLaneOdd   = Color::from_hex(0x27282A);
    static constexpr Color kTrackGrid      = Color::from_hex(0x343638);
    static constexpr Color kScrollbarTrack = Color::from_hex(0x1B1C1D);
    static constexpr Color kScrollbarThumb = Color::from_hex(0x55585C);

    // Axis colors remain semantic; they are not used for selection or focus.
    static constexpr Color kAxisX          = Color(0.780f, 0.350f, 0.350f, 1.0f);
    static constexpr Color kAxisY          = Color(0.350f, 0.700f, 0.400f, 1.0f);
    static constexpr Color kAxisZ          = Color(0.350f, 0.550f, 0.800f, 1.0f);
    static constexpr Color kSafeFrameMask  = Color(0.018f, 0.018f, 0.018f, 0.76f);

    // Compact application menu and workspace rows, followed by editor headers.
    static constexpr float kTopbarHeight = 60.0f;
    static constexpr float kAreaHeaderHeight = 25.0f;
    static constexpr float kTimelineHeaderHeight = 26.0f;
    static constexpr float kControlHeight = 22.0f;
    static constexpr float kOutlinerRowHeight = 20.0f;
    static constexpr float kUiFont = 12.0f;
    static constexpr float kPanelTitleFont = 12.5f;
    static constexpr float kSmallFont = 11.0f;
    static constexpr float kCornerRadius = 2.0f;
    static constexpr float kControlInset = 1.0f;
    static constexpr const char* kUiFontFamily = "Segoe UI Variable Text";
    static constexpr const char* kNumericFontFamily = "Consolas";
};

} // namespace medusa
