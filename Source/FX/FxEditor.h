#pragma once

#include "Common/Components.h"
#include "FxProcessor.h"

namespace spark
{
// Left column, top: the incoming track against Spark's output, Freeze and Capture.
class InputPanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit InputPanel (FxProcessor&);
    std::function<void()> onCapture;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }

    FxProcessor& processor;
    PillButton freeze { "Freeze", PillButton::Style::lockToggle, Icon::snowflake };
    PillButton capture { "Capture to sample", PillButton::Style::goldOutline };
    juce::ParameterAttachment freezeAttachment;
    std::vector<float> scopeIn, scopeOut;
};

// Left column, bottom: meters and bypass.
class OutputPanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit OutputPanel (FxProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    FxProcessor& processor;
    PillButton bypass { "BYPASS", PillButton::Style::lockToggle };
    juce::ParameterAttachment bypassAttachment;
    float inLevel = 0.0f, outLevel = 0.0f;
};

class FxEditor : public SparkEditorBase
{
public:
    explicit FxEditor (FxProcessor&);

protected:
    void addExtraMenuItems (juce::PopupMenu&) override;

private:
    void captureNow();

    FxProcessor& processor;
    InputPanel input;
    OutputPanel output;
};
} // namespace spark
