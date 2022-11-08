/*
  ==============================================================================

    Splash.h

  ==============================================================================
*/

#pragma once

struct Splash : Component
{
    Splash()
    {
        logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);
    }

    void paint(Graphics& g) override
    {
        auto b = getLocalBounds().reduced(1).toFloat();
        auto w = b.getWidth();
        auto h = b.getHeight();

        g.drawImage(img, b, RectanglePlacement::centred);
        g.setColour(Colour(TOP_TRIM).withAlpha(0.5f));
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(Colours::white);
        g.drawRoundedRectangle(b, 5.f, 2.f);

        auto logoBounds = b.removeFromTop(h * 0.66f).reduced(w * 0.2f);

        if (logoBounds.contains(getMouseXYRelative().toFloat()))
        {
            setMouseCursor(MouseCursor::PointingHandCursor);
            if (isMouseButtonDown() && !linkClicked) {
                URL("https://arborealaudio.com").launchInDefaultBrowser();
                linkClicked = true;
                repaint();
                return;
            }
        }
        else
            setMouseCursor(MouseCursor::NormalCursor);

        g.setColour(Colours::white);

        logo->drawWithin(g, logoBounds, RectanglePlacement::yTop, 1.f);

        auto wrapperStr = AudioProcessor::getWrapperTypeDescription(currentWrapper);
        if (currentWrapper == AudioProcessor::wrapperType_Undefined)
            wrapperStr = "CLAP";

        g.setFont(16.f);
        Time time(Time::getCurrentTime());
        g.drawFittedText("v" + String(ProjectInfo::versionString) + 
            "\n" + wrapperStr +
            "\n(c) Arboreal Audio " + String(time.getYear()),
            b.toNearestInt(),
            Justification::centred, 3, 1.f);
    }

    void setImage(const Image& newImage)
    {
        img = newImage;

        Blur::blurImage<4, true>(img);

        repaint();
    }

    void setPluginWrapperType(AudioProcessor::WrapperType type)
    {
        currentWrapper = type;
    }

    std::function<void()> onLogoClick;

private:
        
    bool linkClicked = false;

    Image img;

    std::unique_ptr<Drawable> logo;

    AudioProcessor::WrapperType currentWrapper;
};