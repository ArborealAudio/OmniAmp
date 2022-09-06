/* Menu.h */

#pragma once

class MenuComponent : public Component
{
    enum CommandID
    {
    #if JUCE_WINDOWS || JUCE_LINUX
        OpenGL,
    #endif
        HQ,
        RenderHQ,
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
                    checked.setText(L"✔︎ ");
                    checked.setJustification(Justification::centred);
                    checked.setColour(Colours::white);
                    checked.append(text);
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
            HQ.onClick = [&]
            {
                if (onItemClick)
                    onItemClick(CommandID::HQ, HQ.getToggleState());
            };

            renderHQ.setButtonText("Render HQ");
            renderHQ.setClickingTogglesState(true);
            renderHQ.onClick = [&]
            {
                if (onItemClick)
                    onItemClick(CommandID::RenderHQ, renderHQ.getToggleState());
            };

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

    private:
    #if JUCE_WINDOWS || JUCE_LINUX
        ListButton openGL;
    #endif
        ListButton HQ, renderHQ, windowSize, checkUpdate;

        std::vector<Component*> getButtons()
        {
            return {
            #if JUCE_WINDOWS || JUCE_LINUX
                &openGL,
            #endif
                &HQ,
                &renderHQ,
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
        panel.setShadowWidth(10);

        list.onItemClick = [&](CommandID id, bool state)
        {
            switch (id)
            {
            case CommandID::HQ:
                vts.getParameterAsValue("hq") = state;
                break;
            case CommandID::RenderHQ:
                vts.getParameterAsValue("renderHQ") = state;
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
    }

    ~MenuComponent() override
    {
        panel.setContent(nullptr);
    }

    std::function<void()> windowResizeCallback;
    std::function<void()> checkUpdateCallback;

    void resized() override
    {
        auto w = getLocalBounds().getWidth();

        menuButton.setBounds(getLocalBounds().removeFromTop(20).removeFromRight(w / 2));
        panel.setBounds(getLocalBounds());
    }
};