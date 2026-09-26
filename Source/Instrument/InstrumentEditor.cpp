#include "InstrumentEditor.h"

namespace spark
{
// =====================================================================================
SourcePanel::SourcePanel (InstrumentProcessor& p)
    : processor (p),
      modeAttachment (*p.apvts.getParameter ("mode"), [this] (float v)
      {
          const bool table = v > 0.5f;
          grainTab.setToggleState (! table, juce::dontSendNotification);
          tableTab.setToggleState (table, juce::dontSendNotification);
          repaint();
      })
{
    for (auto* b : { &grainTab, &tableTab, &importButton, &makeTableButton, &exportButton })
        addAndMakeVisible (b);
    for (auto* b : { &grainTab, &tableTab })
    {
        b->setFontHeight (10.0f);
        b->setLetterSpacing (0.14f);
    }
    grainTab.setTooltip ("Grain: play the sound as a cloud of tiny slices");
    tableTab.setTooltip ("Table: play the wavetable made from the sound");
    importButton.setTooltip ("Load a sample or wavetable (you can also drag one onto Spark)");
    makeTableButton.setTooltip ("Slice the current sound into a 64-frame wavetable");
    exportButton.setTooltip ("Save the wavetable, with Drive and Tone baked in, for Serum or Vital");

    grainTab.onClick = [this] { modeAttachment.setValueAsCompleteGesture (0.0f); };
    tableTab.onClick = [this] { modeAttachment.setValueAsCompleteGesture (1.0f); };
    importButton.onClick = [this] { if (onImport) onImport(); };
    makeTableButton.onClick = [this] { if (onMakeTable) onMakeTable(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };

    modeAttachment.sendInitialUpdate();
    processor.addChangeListener (this);
    startTimerHz (24);
}

SourcePanel::~SourcePanel() { processor.removeChangeListener (this); }

void SourcePanel::resized()
{
    tableTab.setBounds (getWidth() - 16 - 2 - 62, 16, 62, 26);
    grainTab.setBounds (tableTab.getX() - 64, 16, 64, 26);
    const int w = (getWidth() - 32 - 12) / 3;
    importButton.setBounds (16, 204, w, 32);
    makeTableButton.setBounds (16 + w + 6, 204, w, 32);
    exportButton.setBounds (16 + 2 * (w + 6), 204, w, 32);
}

void SourcePanel::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "SOURCE", { 16.0f, 16.0f, 100.0f, 26.0f });

    // segmented control frame
    auto seg = grainTab.getBounds().getUnion (tableTab.getBounds()).toFloat().expanded (2.0f);
    g.setColour (line2);
    g.drawRoundedRectangle (seg, seg.getHeight() * 0.5f, 1.0f);

    // drop zone
    auto zone = juce::Rectangle<float> (16.0f, 54.0f, (float) getWidth() - 32.0f, 108.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (zone, 12.0f);
    {
        juce::Path outline;
        outline.addRoundedRectangle (zone.reduced (0.5f), 12.0f);
        juce::Path dashPath;
        const float dashLengths[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashPath, outline, dashLengths, 2);
        g.setColour (dragHover ? gold : colours::dashed);
        g.fillPath (dashPath);
    }

    auto s = processor.getSource();
    auto wave = zone.withHeight (78.0f).reduced (1.0f, 0.0f);
    auto info = zone.withTrimmedTop (78.0f);
    g.setColour (line);
    g.fillRect (info.getX() + 1.0f, info.getY(), info.getWidth() - 2.0f, 1.0f);

    const bool tableMode = processor.getMode() == InstrumentProcessor::tableMode;
    if (s != nullptr)
    {
        if (tableMode && s->table != nullptr)
        {
            const float morph = processor.facetParam (InstrumentProcessor::morph).getValue();
            const int frames = s->table->getNumFrames();
            s->table->getFrameShape (morph * (float) (frames - 1), frameShape, 128);
            juce::Path p;
            for (int i = 0; i < 128; ++i)
            {
                const float x = wave.getX() + 8.0f + (float) i / 127.0f * (wave.getWidth() - 16.0f);
                const float y = wave.getCentreY() - frameShape[(size_t) i] * 30.0f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (gold);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
            g.setColour (muted);
            g.setFont (fonts::mono (10.0f));
            g.drawText ("FRAME " + juce::String (juce::roundToInt (morph * (float) (frames - 1)) + 1) + " / " + juce::String (frames),
                        wave.reduced (8.0f, 6.0f), juce::Justification::topRight, false);
        }
        else if (! s->peaks.empty())
        {
            const float pos = processor.facetParam (InstrumentProcessor::position).getValue();
            const float grainSec = fmt::grainSeconds (processor.facetParam (InstrumentProcessor::grain).getValue());
            const float duration = (float) (s->audio.getNumSamples() / s->sampleRate);
            const float grainW = juce::jlimit (3.0f, wave.getWidth(), grainSec / juce::jmax (0.01f, duration) * wave.getWidth());
            const float px = wave.getX() + pos * wave.getWidth();

            g.setColour (gold.withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (px - grainW * 0.5f, wave.getY(), grainW, wave.getHeight()).getIntersection (wave));

            const int bars = 86;
            juce::Path bp;
            for (int i = 0; i < bars; ++i)
            {
                const int bin = (int) ((float) i / (float) bars * (float) s->peaks.size());
                const float h = juce::jmax (1.5f, s->peaks[(size_t) bin] * 34.0f);
                const float x = wave.getX() + 3.0f + (float) i * (wave.getWidth() - 6.0f) / (float) (bars - 1);
                bp.startNewSubPath (x, wave.getCentreY() - h);
                bp.lineTo (x, wave.getCentreY() + h);
            }
            g.setColour (gold);
            g.strokePath (bp, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

            g.setColour (text);
            g.fillRect (px - 0.6f, wave.getY(), 1.2f, wave.getHeight());
        }

        g.setColour (text);
        g.setFont (fonts::body (12.0f));
        g.drawText (s->name, info.reduced (10.0f, 0.0f).withTrimmedRight (50.0f), juce::Justification::centredLeft, true);
        g.setColour (muted);
        g.setFont (fonts::mono (10.0f));
        const auto secs = s->audio.getNumSamples() / s->sampleRate;
        g.drawText (s->loadedAsWavetable ? juce::String (s->table->getNumFrames()) + " fr" : juce::String (secs, 2) + " s",
                    info.reduced (10.0f, 0.0f), juce::Justification::centredRight, false);
    }

    g.setColour (muted);
    g.setFont (fonts::body (12.0f));
    g.drawText ("Drop audio or a wavetable (WAV, AIFF, FLAC)", juce::Rectangle<float> (16.0f, 170.0f, (float) getWidth() - 32.0f, 22.0f),
                juce::Justification::centredLeft, false);
}

// =====================================================================================
ShapePanel::ShapePanel (InstrumentProcessor& p)
    : processor (p),
      attack (p.apvts.getParameter ("attack")),
      decay (p.apvts.getParameter ("decay")),
      sustain (p.apvts.getParameter ("sustain")),
      release (p.apvts.getParameter ("release"))
{
    setTooltip ("Drag the points to shape how each note starts, holds and fades");
    startTimerHz (20);
}

ShapePanel::Geometry ShapePanel::geometry() const
{
    Geometry geo;
    geo.left = graph.getX() + 4.0f;
    geo.right = graph.getRight() - 4.0f;
    geo.top = graph.getY() + 8.0f;
    geo.bottom = graph.getBottom() - 4.0f;
    const float h = geo.bottom - geo.top;
    const float ax = geo.left + attack->getValue() * 60.0f;
    const float dx = ax + 10.0f + decay->getValue() * 60.0f;
    const float sy = geo.bottom - sustain->getValue() * h;
    const float rx = geo.right - 10.0f - release->getValue() * 60.0f;
    geo.attack = { ax, geo.top };
    geo.decay = { dx, sy };
    geo.release = { rx, sy };
    return geo;
}

void ShapePanel::paint (juce::Graphics& g)
{
    using namespace colours;
    drawPanel (g, getLocalBounds().toFloat());
    drawSectionLabel (g, "SHAPE", { 16.0f, 14.0f, 100.0f, 20.0f });

    const auto geo = geometry();
    juce::Path line;
    line.startNewSubPath (geo.left, geo.bottom);
    line.lineTo (geo.attack);
    line.quadraticTo (geo.attack.x + 6.0f, geo.decay.y, geo.decay.x, geo.decay.y);
    line.lineTo (geo.release);
    line.quadraticTo (geo.release.x + 6.0f, geo.bottom, geo.right, geo.bottom);

    juce::Path fill (line);
    fill.closeSubPath();
    g.setColour (gold.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (gold);
    g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const juce::Point<float> pts[] = { geo.attack, geo.decay, geo.release };
    for (int i = 0; i < 3; ++i)
    {
        const float r = (i == hovered || i == dragging) ? 5.5f : 4.0f;
        g.setColour (i == dragging ? gold : text);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pts[i]));
    }

    const juce::String labels[] = { "A", "D", "S", "R" };
    juce::RangedAudioParameter* ps[] = { attack, decay, sustain, release };
    const float colW = graph.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto col = juce::Rectangle<float> (graph.getX() + colW * (float) i, graph.getBottom() + 6.0f, colW, 30.0f);
        g.setColour (muted);
        g.setFont (fonts::body (10.0f, true).withExtraKerningFactor (0.14f));
        g.drawText (labels[i], col.removeFromTop (13.0f), juce::Justification::centred, false);
        g.setColour (text);
        g.setFont (fonts::mono (11.0f));
        g.drawText (ps[i]->getCurrentValueAsText(), col, juce::Justification::centred, false);
    }
}

int ShapePanel::pointAt (juce::Point<float> p) const
{
    const auto geo = geometry();
    const juce::Point<float> pts[] = { geo.attack, geo.decay, geo.release };
    int best = -1;
    float bestD = 12.0f;
    for (int i = 0; i < 3; ++i)
        if (auto d = pts[i].getDistanceFrom (p); d < bestD) { bestD = d; best = i; }
    return best;
}

void ShapePanel::mouseMove (const juce::MouseEvent& e)
{
    const int h = pointAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ShapePanel::mouseDown (const juce::MouseEvent& e)
{
    dragging = pointAt (e.position);
    for (auto* p : { attack, decay, sustain, release })
        if (dragging >= 0) p->beginChangeGesture();
}

void ShapePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto geo = geometry();
    const float h = geo.bottom - geo.top;
    auto setP = [] (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v)); };

    if (dragging == 0)
    {
        setP (attack, (e.position.x - geo.left) / 60.0f);
    }
    else if (dragging == 1)
    {
        setP (decay, (e.position.x - geo.attack.x - 10.0f) / 60.0f);
        setP (sustain, (geo.bottom - e.position.y) / h);
    }
    else
    {
        setP (release, (geo.right - 10.0f - e.position.x) / 60.0f);
        setP (sustain, (geo.bottom - e.position.y) / h);
    }
    repaint();
}

void ShapePanel::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* p : { attack, decay, sustain, release })
            p->endChangeGesture();
    dragging = -1;
    repaint();
}

// =====================================================================================
InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : SparkEditorBase (p, false), processor (p), source (p), shape (p)
{
    auto col = leftColumn();
    source.setBounds (col.removeFromTop (252));
    col.removeFromTop (14);
    shape.setBounds (col);
    content().addAndMakeVisible (source);
    content().addAndMakeVisible (shape);

    source.onImport = [this] { chooseFileToImport(); };
    source.onMakeTable = [this] { processor.makeTableFromSample(); };
    source.onExport = [this] { exportWavetable(); };
}

bool InstrumentEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (processor.getFormatManager().findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;
    return false;
}

void InstrumentEditor::filesDropped (const juce::StringArray& files, int, int)
{
    source.setDragHover (false);
    for (auto& f : files)
    {
        const juce::File file (f);
        if (processor.getFormatManager().findFormatForFileExtension (file.getFileExtension()) != nullptr)
        {
            loadFile (file);
            return;
        }
    }
}

void InstrumentEditor::loadFile (const juce::File& file)
{
    juce::String error;
    if (! processor.loadFile (file, error))
        showMessage ("Couldn't load that sound", error);
}

void InstrumentEditor::chooseFileToImport()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a sample or wavetable",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   processor.getSupportedExtensions());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (auto f = fc.getResult(); f.existsAsFile())
                                  loadFile (f);
                          });
}

void InstrumentEditor::exportWavetable()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Spark").getChildFile ("Wavetables");
    folder.createDirectory();
    auto s = processor.getSource();
    const auto base = (s != nullptr ? juce::File::createLegalFileName (s->name).upToLastOccurrenceOf (".", false, false) : juce::String ("Spark"));
    const auto suggested = folder.getNonexistentChildFile ((base.isEmpty() ? juce::String ("Spark") : base) + " Spark", ".wav", false);

    chooser = std::make_unique<juce::FileChooser> ("Export wavetable", suggested, "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;
                              f = f.withFileExtension (".wav");
                              juce::String error;
                              if (processor.exportTable (f, error))
                                  showMessage ("Wavetable saved", f.getFileName() + " is ready for Serum, Vital or Spark.\n\n" + f.getParentDirectory().getFullPathName());
                              else
                                  showMessage ("Couldn't export", error);
                          });
}

void InstrumentEditor::addExtraMenuItems (juce::PopupMenu& m)
{
    m.addItem ("Load sample or wavetable...", [this] { chooseFileToImport(); });
    m.addItem ("Make wavetable from sample", [this] { processor.makeTableFromSample(); });
    m.addItem ("Export wavetable...", [this] { exportWavetable(); });
}
} // namespace spark
