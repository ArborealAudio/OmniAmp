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
        struct ListButton : TextButton
        {
            void paint(Graphics& g) override
            {
                if (isMouseOver()) {
                    g.setColour(Colours::lightslategrey);
                }

                g.fillRect(getLocalBounds());

                auto text = getButtonText();
                g.setColour(Colours::white);
                if (getToggleState()) {
                    AttributedString checked;
                    checked.setText(String::fromUTF8("✅ ") + text);
                    checked.setJustification(Justification::centred);
                    checked.setColour(Colours::white);
                    checked.draw(g, getLocalBounds().toFloat());
                }
                else
                    g.drawFittedText(text, getLocalBounds(), Justification::centred, 1);
            }
        };

        ButtonList()
        {
            for (auto& b : getButtons())
                addAndMakeVisible(b);

#if JUCE_WINDOWS || JUCE_LINUX
            openGL.setButtonText("OpenGL On/Off");
            openGL.setClickingTogglesState(true);
            openGL.onClick = [&]
            {
                if (onItemClick)
                    onItemClick(CommandID::OpenGL, openGL.getToggleState());
            };
#endif

            HQ.setButtonText("HQ");
            HQ.setClickingTogglesState(true);

            renderHQ.setButtonText("Render HQ");
            renderHQ.setClickingTogglesState(true);

            compLink.setButtonText("Comp Stereo Link");
            compLink.setClickingTogglesState(true);

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

        ListButton HQ, renderHQ, compLink;

    private:
    #if JUCE_WINDOWS || JUCE_LINUX
        ListButton openGL;
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

    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hqAttach, renderHQAttach, compLinkAttach;

public:
    MenuComponent(AudioProcessorValueTreeState& a, int width) : vts(a), panel("Options", width, false), menuButton("Menu", DrawableButton::ButtonStyle::ImageFitted)
    {
        addAndMakeVisible(menuButton);
        icon = Drawable::createFromImageData(BinaryData::Hamburger_icon_svg, BinaryData::Hamburger_icon_svgSize);
        menuButton.setImages(icon.get());
        menuButton.onClick = [&]
        { panel.showOrHide(!panel.isPanelShowing()); };

        addAndMakeVisible(panel);
        panel.setColour(SidePanel::ColourIds::backgroundColour, Colours::white);
        panel.setContent(&list, false);
        panel.setShadowWidth(0);

        list.onItemClick = [&](CommandID id, bool state)
        {
            switch (id)
            {
            case CommandID::OpenGL:
                if (openGLCallback)
                    openGLCallback(state);
                break;
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