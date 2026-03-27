#include <gtest/gtest.h>
#include "MidiChordImporter.h"

using TSE = MidiChordImporter::TimeSigEntry;

// ═══════════════════════════════════════════════════════════════════
// tickToBarPosition
// ═══════════════════════════════════════════════════════════════════

TEST(MidiChordImporter, TickToBar_4_4_TickZero)
{
    // Tick 0 in 4/4 at 480 PPQ → bar 1.0
    std::vector<TSE> tsMap = {{ 0.0, 4, 4 }};
    double bar = MidiChordImporter::tickToBarPosition(0.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 1.0, 0.001);
}

TEST(MidiChordImporter, TickToBar_4_4_OneBar)
{
    // One full bar in 4/4 = 4 quarter notes = 4*480 ticks → bar 2.0
    std::vector<TSE> tsMap = {{ 0.0, 4, 4 }};
    double bar = MidiChordImporter::tickToBarPosition(4 * 480.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 2.0, 0.001);
}

TEST(MidiChordImporter, TickToBar_4_4_HalfBar)
{
    // Half a bar in 4/4 = 2 quarter notes = 2*480 → bar 1.5
    std::vector<TSE> tsMap = {{ 0.0, 4, 4 }};
    double bar = MidiChordImporter::tickToBarPosition(2 * 480.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 1.5, 0.001);
}

TEST(MidiChordImporter, TickToBar_3_4_OneBar)
{
    // One full bar in 3/4 = 3 quarter notes = 3*480 → bar 2.0
    std::vector<TSE> tsMap = {{ 0.0, 3, 4 }};
    double bar = MidiChordImporter::tickToBarPosition(3 * 480.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 2.0, 0.001);
}

TEST(MidiChordImporter, TickToBar_6_8_OneBar)
{
    // 6/8 time: one bar = 6 × (4/8) = 3 quarter notes = 3*480 ticks
    std::vector<TSE> tsMap = {{ 0.0, 6, 8 }};
    double bar = MidiChordImporter::tickToBarPosition(3 * 480.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 2.0, 0.001);
}

TEST(MidiChordImporter, TickToBar_TimeSigChange)
{
    // 2 bars of 4/4 then switch to 3/4 at tick 3840
    // Bar 1 + Bar 2 = 2 bars of 4/4 = 2 × 1920 = 3840 ticks
    // Then 1 bar of 3/4 = 3 × 480 = 1440 ticks
    // Total tick at end of 3rd bar = 3840 + 1440 = 5280
    std::vector<TSE> tsMap = {{ 0.0, 4, 4 }, { 3840.0, 3, 4 }};
    double bar = MidiChordImporter::tickToBarPosition(5280.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 4.0, 0.001);
}

TEST(MidiChordImporter, TickToBar_MultipleBars)
{
    // 4 bars in 4/4 = 4 × 1920 = 7680 → bar 5.0
    std::vector<TSE> tsMap = {{ 0.0, 4, 4 }};
    double bar = MidiChordImporter::tickToBarPosition(7680.0, 480.0, tsMap);
    EXPECT_NEAR(bar, 5.0, 0.001);
}

// ═══════════════════════════════════════════════════════════════════
// importFromFile — error paths
// ═══════════════════════════════════════════════════════════════════

TEST(MidiChordImporter, ImportNonExistentFile)
{
    juce::File noFile("/tmp/jimmy_test_nonexistent_12345.mid");
    auto result = MidiChordImporter::importFromFile(noFile);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.isNotEmpty());
}
