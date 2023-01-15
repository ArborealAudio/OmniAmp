/* Menu.h */

#pragma once

class MenuComponent : public Component
{
    struct ListButton : TextButton
    {
        bool toggle = false;

        void paint(Graphics &g) override
        {
            if (isMouseOver())
                g.setColour(Colours::lightgrey);

            g.fillRect(getLocalBounds());

            auto text = getButtonText();

            auto bounds = getLocalBounds();
            bounds.reduce(bounds.getWidth() * 0.07, bounds.getHeight() * 0.2);
            auto textBounds = bounds;

            if (toggle)
            {
                textBounds = bounds.removeFromLeft((float)bounds.getWidth() * 0.5f);
                bounds.reduce(bounds.getWidth() * 0.2f, 0);
                if (getToggleState())
                {
                    g.setColour(Colours::cadetblue);
                    g.fillRoundedRectangle(bounds.toFloat(), bounds.getHeight() * 0.5f);
                    g.setColour(Colours::white);
                    g.fillEllipse(bounds.removeFromRight(bounds.getWidth() / 2).toFloat());
                }
                else
                {
                    g.setColour(Colours::darkgrey);
                    g.fillRoundedRectangle(bounds.toFloat(), bounds.getHeight() * 0.5f);
                    g.setColour(Colours::white);
                    g.fillEllipse(bounds.removeFromLeft(bounds.getWidth() / 2).toFloat());
                }
                g.setColour(Colours::white);
                g.drawFittedText(text, textBounds, Justification::centred, 1);
            }
            else
            {
                g.setColour(Colours::white);
                g.drawFittedText(text, textBounds, Justification::centred, 1);
            }
        }
    };

    AudioProcessorValueTreeState &vts;

    DrawableButton menuButton;
    std::unique_ptr<Drawable> icon;

    #if ! JUCE_MAC
    ListButton openGL;
    #endif
    ListButton HQ, renderHQ, windowSize, checkUpdate, showTooltips;

    bool openGLOn = false, showTooltipsOn = false;

    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hqAttach, renderHQAttach;

public:
    MenuComponent(AudioProcessorValueTreeState &a) : vts(a), menuButton("Menu", DrawableButton::ButtonStyle::ImageFitted)
    {
#if JUCE_WINDOWS || JUCE_LINUX
        openGL.setButtonText("OpenGL On/Off");
        openGL.toggle = true;
        openGL.setClickingTogglesState(true);
        openGL.setToggleState(readConfigFile("openGL"), NotificationType::sendNotification);
#endif

        HQ.setButtonText("HQ");
        HQ.toggle = true;
        HQ.setClickingTogglesState(true);
        HQ.setTooltip("Enable 4x oversampling with minimal latency");

        renderHQ.setButtonText("Render HQ");
        renderHQ.toggle = true;
        renderHQ.setClickingTogglesState(true);
        renderHQ.setTooltip("Enable 4x oversampling when rendering. Useful if you want to save some CPU while mixing.");

        windowSize.setButtonText("Default UI size");
        windowSize.setTooltip("Reset window size to default dimensions");
        windowSize.setClickingTogglesState(false);

        checkUpdate.setButtonText("Check update");
        checkUpdate.setTooltip("Check for new versions of Gamma");
        checkUpdate.setClickingTogglesState(false);

        showTooltips.setButtonText("Show tooltips");
        showTooltips.setTooltip("Set whether or not tooltips will show on hover-over");
        showTooltips.setClickingTogglesState(true);
        showTooltips.setToggleState(readConfigFile("tooltips"), NotificationType::dontSendNotification);
        showTooltips.toggle = true;

        addAndMakeVisible(menuButton);
        icon = Drawable::createFromImageData(BinaryData::Hamburger_icon_svg, BinaryData::Hamburger_icon_svgSize);
        menuButton.setImages(icon.get());
        menuButton.onClick = [&]
        {
            PopupMenu m;
#if !JUCE_MAC
            openGLOn = (bool)readConfigFile("openGL");
            openGL.setToggleState(openGLOn, NotificationType::dontSendNotification);
            m.addCustomItem(1, openGL, getWidth(), 35, false);
#endif
            m.addCustomItem(2, HQ, getWidth(), 35, false, nullptr, "HQ");
            m.addCustomItem(3, renderHQ, getWidth(), 35, false, nullptr, "Render HQ");
            showTooltipsOn = (bool)readConfigFile("tooltips");
            showTooltips.setToggleState(showTooltipsOn, NotificationType::sendNotificationSync);
            m.addCustomItem(4, showTooltips, getWidth(), 35, true, nullptr, "Show Tooltips");
            m.addCustomItem(5, windowSize, getWidth(), 35, true, nullptr, "Default window size");
            m.addCustomItem(6, checkUpdate, getWidth(), 35, true, nullptr, "Check update");
            m.showMenuAsync(PopupMenu::Options().withMinimumWidth(175).withStandardItemHeight(35),
                            [&](int result)
                            {
                                switch (result)
                                {
                                case 1:
                                    if (openGLCallback)
                                        openGLCallback(openGLOn);
                                    break;
                                case 2:
                                    HQ.setToggleState(!HQ.getToggleState(), NotificationType::sendNotificationAsync);
                                    break;
                                case 3:
                                    renderHQ.setToggleState(!renderHQ.getToggleState(), NotificationType::sendNotificationAsync);
                                    break;
                                case 4:
                                    if (showTooltipCallback)
                                        showTooltipCallback(!showTooltipsOn);
                                    break;
                                case 5:
                                    if (windowResizeCallback)
                                        windowResizeCallback();
                                    break;
                                case 6:
                                    if (checkUpdateCallback)
                                        checkUpdateCallback();
                                    break;
                                }
                            });
        };

        hqAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "hq", HQ);
        renderHQAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "renderHQ", renderHQ);
    }

    std::function<void()> windowResizeCallback;
    std::function<void()> checkUpdateCallback;
    std::function<void(bool)> openGLCallback;
    std::function<void(bool)> showTooltipCallback;

    void resized() override
    {
        auto w = getLocalBounds().getWidth();
        menuButton.setBounds(getLocalBounds().removeFromTop(20).removeFromRight(w / 2));
    }
};