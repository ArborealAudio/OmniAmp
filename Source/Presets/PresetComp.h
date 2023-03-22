/*
  ==============================================================================

    PresetComp.h

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

// Custom ComboBox because this is a great codebase
struct PresetComboBoxLNF : LookAndFeel_V4
{
    void drawComboBox(Graphics& g, int width, int height, bool,
                                   int, int, int, int, ComboBox& box) override
    {
        auto cornerSize = (float)width * 0.1f;
        Rectangle<int> boxBounds (0, 0, width, height);

        g.setColour (box.findColour (ComboBox::backgroundColourId));
        g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

        g.setColour (box.findColour (ComboBox::outlineColourId));
        g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

        // chunk of L/R end used for arrow
        arrowSize = (float)box.getWidth() / 6.5f;
        const float padding = (float)box.getWidth() * 0.05f;
        const float strokeWidth = 3.f;

        auto drawArrow = [&](const Rectangle<float> &bounds, Path &path, bool leftArrow)
        {
            const float r = bounds.getRight();
            const float bot = bounds.getBottom();
            const float centerY = bounds.getY() + (bounds.getHeight() / 2);
            const float arrowWidth = bounds.getHeight() * 0.9f;
            if (leftArrow)
            {
                path.startNewSubPath(bounds.getX() + arrowWidth, bounds.getY());
                path.lineTo(bounds.getX(), centerY);
                path.lineTo(bounds.getX() + arrowWidth, bot);
            }
            else
            {
                path.startNewSubPath(r - arrowWidth, bounds.getY());
                path.lineTo(r, centerY);
                path.lineTo(r - arrowWidth, bot);
            }
            g.strokePath(path, PathStrokeType(strokeWidth, PathStrokeType::beveled, PathStrokeType::rounded));
        };
        Path leftArrow;
        drawArrow(boxBounds.toFloat().removeFromLeft(arrowSize).reduced(padding), leftArrow, true);
        Path rightArrow;
        drawArrow(boxBounds.toFloat().removeFromRight(arrowSize).reduced(padding), rightArrow, false);
    }
    
    void positionComboBoxText (ComboBox& box, Label& label) override
    {
        label.setJustificationType(Justification::centred);
        label.setBounds (1, 1,
                         box.getWidth() - 1,
                         box.getHeight() - 2);

        label.setFont (getComboBoxFont (box));
    }

    void drawComboBoxTextWhenNothingSelected (Graphics& g, ComboBox& box, Label& label) override
    {
        g.setColour (findColour (ComboBox::textColourId).withMultipliedAlpha (0.5f));

        auto font = label.getLookAndFeel().getLabelFont (label);

        g.setFont (font);

        auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());

        g.drawFittedText (box.getTextWhenNothingSelected(), textArea, Justification::centred,
                          jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());
    }

    float arrowSize;
};

struct PresetComp : Component, private Timer
{
    PresetComp(AudioProcessorValueTreeState& vts) : manager(vts)
    {
        addAndMakeVisible(box);
        box.setLookAndFeel(&boxLNF);
        box.setJustificationType(Justification::centredLeft);
        box.setTextWhenNothingSelected("Presets");

        loadPresets();

        addChildComponent(editor);
        editor.setJustification(Justification::centredLeft);

        startTimerHz(2);

        setInterceptsMouseClicks(true, false);

        addMouseListener(this, true);
    }

    ~PresetComp()
    {
        removeMouseListener(this);
        box.setLookAndFeel(nullptr);
        stopTimer();
    }

    void loadPresets()
    {
        PopupMenu::Item save{ "save" }, saveAs{ "save as" }, copy{ "copy preset" }, paste{"paste preset"}, presetDir{"open preset folder"};

        auto menu = box.getRootMenu();
        menu->clear();
        save.setID(1001);
        save.setTicked(false);
        save.action = [&] { savePreset(); };
        menu->addItem(save);

        saveAs.setID(1002);
        saveAs.setTicked(false);
        saveAs.action = [&] { savePresetAs(); };
        menu->addItem(saveAs);

        copy.setID(1003);
        copy.setTicked(false);
        copy.action = [&]
        {
            SystemClipboard::copyTextToClipboard(manager.getStateAsString());
        };
        menu->addItem(copy);

        paste.setID(1004);
        paste.setTicked(false);
        paste.action = [&]
        {
            manager.setStateFromString(SystemClipboard::getTextFromClipboard());
        };
        menu->addItem(paste);

        presetDir.setID(1005);
        presetDir.setTicked(false);
        presetDir.action = [&]
        {
            manager.userDir.getParentDirectory().startAsProcess();
        };
        menu->addItem(presetDir);

        menu->addSeparator();

        auto presets = manager.loadFactoryPresetList();
        box.addItemList(presets, 1);
        factoryPresetSize = presets.size();

        auto user = manager.loadUserPresetList();
        userPresets.clear();
        for (int i = 0; i < user.size(); ++i)
        {
            userPresets.addItem(factoryPresetSize + i + 1, user[i]);
        }

        menu->addSeparator();
        menu->addSubMenu("User Presets", userPresets);
    }

    String getCurrentPreset() noexcept
    {
        return currentPreset;
    }

    void setCurrentPreset(const String& newPreset) noexcept
    {
        currentPreset = newPreset;
        box.setText(currentPreset, NotificationType::dontSendNotification);
    }

    void setPresetWithChange(const String& newPreset) noexcept
    {
        currentPreset = newPreset;
        box.setText(currentPreset, NotificationType::sendNotificationSync);
    }

    void savePreset() noexcept
    {
        if (box.getText() == "" || currentPreset == "")
            return;
        manager.savePreset(currentPreset, manager.userDir);
        setCurrentPreset(currentPreset);
    }

    void savePresetAs() noexcept
    {
        editor.setVisible(true);
        editor.toFront(true);
        editor.setText("Preset Name", false);
        editor.grabKeyboardFocus();
        editor.setHighlightedRegion({ 0, 12 });

        editor.onFocusLost = [&]
        {
            editor.clear();
            editor.setVisible(false);
        };

        editor.onEscapeKey = [&] { editor.clear(); editor.setVisible(false); };

        editor.onReturnKey = [&]
        {
            auto name = editor.getText();
            if (name == "")
            {
                box.setText("Enter a name!");
                return;
            }
            editor.clear();
            editor.setVisible(false);

            if (manager.savePreset(name, manager.userDir)) {
                loadPresets();
                setPresetWithChange(name);
            }
            else
                box.setText("invalid name!", NotificationType::dontSendNotification);
        };
    }

    void valueChanged()
    {
        auto id = box.getSelectedId();
        auto idx = box.getSelectedItemIndex();
        auto preset = box.getItemText(idx);

        if (id < 1000 && id > 0) {

            if (id <= factoryPresetSize) {
                if (manager.loadPreset(preset, true))
                    box.setText(preset, NotificationType::sendNotificationSync);
                else
                    box.setText("preset not found", NotificationType::dontSendNotification);
            }
            else {
                if (manager.loadPreset(preset, false))
                    box.setText(preset, NotificationType::sendNotificationSync);
                else
                    box.setText("preset not found", NotificationType::dontSendNotification);
            }

            currentPreset = preset;
        }
        else
            box.setText(currentPreset, NotificationType::dontSendNotification);
    }

    void leftArrowClicked()
    {
        const auto currentIndex = box.getSelectedItemIndex();
        if (currentIndex != 0)
            box.setSelectedItemIndex(currentIndex - 1);
        else
            box.setSelectedItemIndex(currentIndex + 1);
    }
    
    void rightArrowClicked()
    {
        const auto currentIndex = box.getSelectedItemIndex();
        if (currentIndex < box.getNumItems())
            box.setSelectedItemIndex(currentIndex + 1);
        else
            box.setSelectedItemIndex(0);
    }

    void mouseDown(const MouseEvent &event) override
    {
        if (leftArrow.contains(event.position) && event.mouseWasClicked())
        {
            leftArrowClicked();
            return;
        }
        if (rightArrow.contains(event.position) && event.mouseWasClicked())
        {
            rightArrowClicked();
            return;
        }

        Component::mouseDown(event);
    }

    void timerCallback() override
    {
        if (manager.stateChanged && currentPreset != "")
            box.setText(currentPreset + "*", NotificationType::dontSendNotification);
    }

    void resized() override
    {
        const float w = getWidth();
        const float subChunk = w - (w / 8.f);
        auto b = getLocalBounds().toFloat();
        leftArrow = b.withTrimmedRight(subChunk);
        rightArrow = b.withTrimmedLeft(subChunk);
        box.setBounds(getLocalBounds());
        editor.setBounds(getLocalBounds());
    }

    ComboBox box;

private:
    PresetComboBoxLNF boxLNF;

    PopupMenu userPresets;

    int factoryPresetSize = 0;

    TextEditor editor;

    StringArray presetList;
    String currentPreset;

    Rectangle<float> leftArrow, rightArrow;

    PresetManager manager;
};