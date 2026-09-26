#include "FxEditor.h"

namespace spark
{
namespace
{
    juce::String dbText (float gain)
    {
        const float db = juce::Decibels::gainToDecibels (gain, -60.0f);
        return db <= -59.9f ? juce::String ("-inf") : juce::String (db, 1) + " dB";
    }

    float meterPosition (float gain)
    {
        // -60 dB .. 0 dB mapped to 0..1
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (gain, -60.0f) + 60.0f) / 60.0f);
    }
}

// =====================================================================================
InputPanel::InputPanel (FxProcessor& p)
    : processor (p),
      freezeAttachment (*p.apvts.getParameter ("freeze"), [this] (float v)
      {
          freeze.setToggleState (v > 0.5f, juce::dontSendNotification);
      })
{
    addAndMakeVisible (freeze);
    addAndMakeVisible (capture);
    freeze.setTooltip ("Freeze: stop listening and keep playing grains from what's already captured");
    capture.setTooltip ("Save the last four seconds of input as a WAV you can drag into Spark");
    freeze.onClick = [this] { freezeAttachment.setValueAsCompleteGesture (freeze.getToggleState() ? 0.0f : 1.0f); };
    capture.onClick = [this] { if (onCapture) onCapture(); };
    freezeAttachment.sendInitialUpdate();
    startTimerHz (30);
}

void InputPanel::resized()
{
    const int w = (getWidth() - 32 - 6) / 2;
    freeze.setBounds (16, 204, w - 20, 32);
    capture.setBounds (16 + w - 20 + 6, 204, w + 20, 32);
}

void InputPanel::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "INPUT", { 16.0f, 16.0f, 100.0f, 26.0f });

    const bool frozen = freeze.getToggleState();
    auto status = juce::Rectangle<float> ((float) getWidth() - 196.0f, 16.0f, 180.0f, 26.0f);
    const auto statusText = (frozen ? "FROZEN" : "LIVE") + juce::String::fromUTF8 (" \xc2\xb7 1/16 \xc2\xb7 ") + juce::String (juce::roundToInt (processor.getBpm())) + " BPM";
    const auto statusFont = fonts::mono (10.0f);
    const float statusWidth = juce::GlyphArrangement::getStringWidth (statusFont, statusText);
    g.setColour (frozen ? text : gold);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ status.getRight() - statusWidth - 9.0f, status.getCentreY() }));
    g.setColour (muted);
    g.setFont (statusFont);
    g.drawText (statusText, status, juce::Justification::centredRight, false);

    auto zone = juce::Rectangle<float> (16.0f, 54.0f, (float) getWidth() - 32.0f, 108.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (zone, 12.0f);
    g.setColour (line);
    g.drawRoundedRectangle (zone.reduced (0.5f), 12.0f, 1.0f);
    g.fillRect (zone.getX() + 1.0f, zone.getCentreY(), zone.getWidth() - 2.0f, 1.0f);

    processor.getScope (scopeIn, scopeOut);
    auto drawTrace = [&] (const std::vector<float>& d, juce::Colour c, float thickness)
    {
        juce::Path p;
        const int n = (int) d.size();
        const float inner = zone.getWidth() - 4.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = zone.getX() + 2.0f + (float) i / (float) (n - 1) * inner;
            const float y = zone.getCentreY() - juce::jlimit (-1.0f, 1.0f, d[(size_t) i]) * 46.0f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
    };
    g.saveState();
    g.reduceClipRegion (zone.toNearestInt());
    drawTrace (scopeIn, muted, 1.1f);
    drawTrace (scopeOut, gold, 1.6f);
    g.restoreState();

    auto legend = juce::Rectangle<float> (16.0f, 172.0f, 200.0f, 18.0f);
    g.setFont (fonts::body (11.0f));
    g.setColour (muted);
    g.fillRect (legend.getX(), legend.getCentreY() - 1.0f, 14.0f, 2.0f);
    g.drawText ("Track", legend.withTrimmedLeft (20.0f), juce::Justification::centredLeft, false);
    g.setColour (gold);
    g.fillRect (legend.getX() + 74.0f, legend.getCentreY() - 1.0f, 14.0f, 2.0f);
    g.setColour (muted);
    g.drawText ("Spark", legend.withTrimmedLeft (94.0f), juce::Justification::centredLeft, false);
}

// =====================================================================================
OutputPanel::OutputPanel (FxProcessor& p)
    : processor (p),
      bypassAttachment (*p.apvts.getParameter ("bypass"), [this] (float v)
      {
          bypass.setToggleState (v > 0.5f, juce::dontSendNotification);
      })
{
    addAndMakeVisible (bypass);
    bypass.setTooltip ("Hear the track without Spark FX");
    bypass.onClick = [this] { bypassAttachment.setValueAsCompleteGesture (bypass.getToggleState() ? 0.0f : 1.0f); };
    bypassAttachment.sendInitialUpdate();
    startTimerHz (30);
}

void OutputPanel::timerCallback()
{
    inLevel = juce::jmax (processor.getInputPeak(), inLevel * 0.85f);
    outLevel = juce::jmax (processor.getOutputPeak(), outLevel * 0.85f);
    repaint();
}

void OutputPanel::resized()
{
    bypass.setBounds (16, getHeight() - 16 - 32, 92, 32);
}

void OutputPanel::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "OUTPUT", { 16.0f, 14.0f, 100.0f, 20.0f });

    auto meter = [&] (float y, const juce::String& label, float level, juce::Colour c)
    {
        auto row = juce::Rectangle<float> (16.0f, y, (float) getWidth() - 32.0f, 20.0f);
        g.setColour (muted);
        g.setFont (fonts::mono (10.0f));
        g.drawText (label, row.removeFromLeft (34.0f), juce::Justification::centredLeft, false);
        g.setColour (text2);
        g.setFont (fonts::mono (11.0f));
        g.drawText (dbText (level), row.removeFromRight (56.0f), juce::Justification::centredRight, false);
        auto bar = row.reduced (4.0f, 0.0f).withSizeKeepingCentre (row.getWidth() - 8.0f, 4.0f);
        g.setColour (line);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * meterPosition (level)), 2.0f);
    };
    meter (48.0f, "IN", inLevel, text2);
    meter (78.0f, "OUT", outLevel, gold);

    g.setColour (muted);
    g.setFont (fonts::body (11.0f));
    g.drawText ("Wet amount is the Mix facet", juce::Rectangle<float> (116.0f, (float) getHeight() - 48.0f, (float) getWidth() - 132.0f, 32.0f),
                juce::Justification::centredRight, false);
}

// =====================================================================================
FxEditor::FxEditor (FxProcessor& p)
    : SparkEditorBase (p, true), processor (p), input (p), output (p)
{
    auto col = leftColumn();
    input.setBounds (col.removeFromTop (252));
    col.removeFromTop (14);
    output.setBounds (col);
    content().addAndMakeVisible (input);
    content().addAndMakeVisible (output);
    input.onCapture = [this] { captureNow(); };
}

void FxEditor::captureNow()
{
    juce::String error;
    auto file = processor.captureToFile (error);
    if (file == juce::File())
    {
        showMessage ("Nothing captured", error);
        return;
    }
    file.revealToUser();
    showMessage ("Captured", file.getFileName() + " is saved in Music > Spark > Captures.\n\nDrag it into Spark to play it, slice it, or turn it into a wavetable.");
}

void FxEditor::addExtraMenuItems (juce::PopupMenu& m)
{
    m.addItem ("Capture to sample", [this] { captureNow(); });
    m.addItem ("Open captures folder", []
    {
        auto folder = FxProcessor::getCaptureFolder();
        folder.createDirectory();
        folder.startAsProcess();
    });
}
} // namespace spark
