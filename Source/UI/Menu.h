/* Menu.h */

#pragma once

class MenuComponent : public Component
{
    enum CommandID
    {
    #if JUCE_WINDOWS || JUCE_LINUX
        OpenGL,
    #endif
        WindowSize,
        CheckUpdate
    };

    struct ButtonList : Component
    {
        enum ButtonType
        {
            Toggle,
            PrePost,
            Default
        };

        struct ListButton : TextButton
        {
            ListButton()
            {
                type = ButtonType::Default;
            }

            ListButton(ButtonType newType) : type(newType)
            {
            }

            void paint(Graphics& g) override
            {
                if (isMouseOver())
                    g.setColour(Colours::lightslategrey);

                g.fillRect(getLocalBounds());

                auto text = getButtonText();

                auto bounds = getLocalBounds();
                bounds.reduce(bounds.getWidth() * 0.07, bounds.getHeight() * 0.2);
                auto textBounds = bounds;

                switch (type)
                {
                case ButtonType::Toggle:
                    textBounds = bounds.removeFromLeft((float)bounds.getWidth() * 0.5f);
                    bounds.reduce(bounds.getWidth() * 0.2, 0);
                    if (getToggleState()) {
                        g.setColour(Colours::cadetblue);
                        g.fillRoundedRectangle(bounds.toFloat(), bounds.getHeight() * 0.5);
                        g.setColour(Colours::white);
                        g.fillEllipse(bounds.removeFromRight(bounds.getWidth() / 2).toFloat());
                    }
                    else {
                        g.setColour(Colours::darkgrey);
                        g.fillRoundedRectangle(bounds.toFloat(), bounds.getHeight() * 0.5);
                        g.setColour(Colours::white);
                        g.fillEllipse(bounds.removeFromLeft(bounds.getWidth() / 2).toFloat());
                    }
                    g.setColour(Colours::white);
                    g.drawFittedText(text, textBounds, Justification::centred, 1);
                    break;
                case ButtonType::PrePost:
                    textBounds = bounds.removeFromLeft((float)bounds.getWidth() * 0.5f);
                    bounds.reduce(bounds.getWidth() * 0.2, 0);
                    g.setColour(Colours::white);
                    g.fillRoundedRectangle(bounds.toFloat(), bounds.getHeight() * 0.5);
                    g.setColour(Colours::black);
                    if (getToggleState())
                        g.drawFittedText("Post", bounds, Justification::centred, 1);
                    else
                        g.drawFittedText("Pre", bounds, Justification::centred, 1);
                    g.setColour(Colours::white);
                    g.drawFittedText(text, textBounds, Justification::centred, 1);
                    break;
                case ButtonType::Default:
                    g.setColour(Colours::white);
                    g.drawFittedText(text, textBounds, Justification::centred, 1);
                    break;
                }
            }
        private:
            ButtonType type;
        };

        ButtonList()
        {
            for (auto& b : getButtons())
                addAndMakeVisible(b);

#if JUCE_WINDOWS || JUCE_LINUX
            openGL.setButtonText("OpenGL On/Off");
            openGL.setClickingTogglesState(true);
            openGL.setToggleState(readConfigFile("openGL"), NotificationType::sendNotification);
            openGL.onClick = [&]
            {
                if (onItemClick)
                    onItemClick(CommandID::OpenGL, openGL.getToggleState());
            };
#endif

            HQ.setButtonText("HQ");
            HQ.setClickingTogglesState(true);
            HQ.setTooltip("Enable 4x oversampling with minimal latency.");

            renderHQ.setButtonText("Render HQ");
            renderHQ.setClickingTogglesState(true);
            renderHQ.setTooltip("Enable 4x oversampling when rendering. Useful if you want to save some CPU while mixing.");

            compLink.setButtonText("Comp Stereo Link");
            compLink.setClickingTogglesState(true);
            compLink.setTooltip("Stereo linking for Opto compressor. When linked, the maximum amplitude of either channel will be used for the compression. When unlinked, each channel will be compressed independently.");

            compPos.setButtonText("Comp Pos: ");
            compPos.setClickingTogglesState(true);
            compPos.setTooltip("Position in the signal chain for the compressor. Pre will place it at the very beginning, and Post will be after the amp, and before the enhancers, cab and reverb.");

            windowSize.setButtonText("Default UI size");
            windowSize.setClickingTogglesState(false);
            windowSize.onClick = [&]
            {
                if (onItemClick)
                    onItemClick(CommandID::WindowSize, true);
            };

            checkUpdate.setButtonText("Check update");
            checkUpdate.setClickingTogglesState(false);
            checkUpdate.onClick = [&]
            {
                if (onItemClick)
                    onItemClick(CommandID::CheckUpdate, true);
            };
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            int chunk = bounds.getHeight() / getButtons().size();
            for (auto &b : getButtons())
                b->setBounds(bounds.removeFromTop(chunk));
        }

        std::function<void(CommandID, bool)> onItemClick;

        ListButton HQ{ButtonType::Toggle}, renderHQ{ButtonType::Toggle}, compLink{ButtonType::Toggle}, compPos{ButtonType::PrePost};

    private:
    #if JUCE_WINDOWS || JUCE_LINUX
        ListButton openGL{ButtonType::Toggle};
    #endif
        ListButton windowSize, checkUpdate;

        std::vector<Component*> getButtons()
        {
            return {
            #if JUCE_WINDOWS || JUCE_LINUX
                &openGL,
            #endif
                &HQ,
                &renderHQ,
                &compLink,
                &compPos,
                &windowSize,
                &checkUpdate
            };
        }
    };

    AudioProcessorValueTreeState& vts;

    ButtonList list;
    DrawableButton menuButton;
    std::unique_ptr<Drawable> icon;

    SidePanel panel;

    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hqAttach, renderHQAttach, compLinkAttach, compPosAttach;

public:
    MenuComponent(AudioProcessorValueTreeState& a, int width) : vts(a), panel("Options", width, false), menuButton("Menu", DrawableButton::ButtonStyle::ImageFitted)
    {
        addAndMakeVisible(menuButton);
        icon = Drawable::createFromImageData(BinaryData::Hamburger_icon_svg, BinaryData::Hamburger_icon_svgSize);
        menuButton.setImages(icon.get());
        menuButton.onClick = [&]
        {
            panel.showOrHide(!panel.isPanelShowing());
        };
        panel.onPanelShowHide = [&] (bool state)
        {
            state ? toFront(false) : toBack();
        };

        addAndMakeVisible(panel);
        panel.setContent(&list, false);
        panel.setShadowWidth(10);

        list.onItemClick = [&](CommandID id, bool state)
        {
            switch (id)
            {
        #if JUCE_WINDOWS || JUCE_LINUX
            case CommandID::OpenGL:
                if (openGLCallback)
                    openGLCallback(state);
                break;
        #endif
            case CommandID::WindowSize:
                if (windowResizeCallback)
                    windowResizeCallback();
                break;
            case CommandID::CheckUpdate:
                if (checkUpdateCallback)
                    checkUpdateCallback();
                break;
            default:
                break;
            };
        };

        hqAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "hq", list.HQ);
        renderHQAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "renderHQ", list.renderHQ);
        compLinkAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "compLink", list.compLink);
        compPosAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "compPos", list.compPos);
    }

    ~MenuComponent() override
    {
        panel.setContent(nullptr);
    }

    std::function<void()> windowResizeCallback;
    std::function<void()> checkUpdateCallback;
    std::function<void(bool)> openGLCallback;

    void resized() override
    {
        auto w = getLocalBounds().getWidth();

        menuButton.setBounds(getLocalBounds().removeFromTop(20).removeFromRight(w / 2));
        panel.setBounds(getLocalBounds());
    }
};