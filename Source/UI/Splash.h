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

    void paint(Graphics &g) override
    {
        auto b = getLocalBounds().reduced(1).toFloat();
        auto w = b.getWidth();
        auto h = b.getHeight();

        g.drawImage(img, b, RectanglePlacement::centred);
        g.setColour(Colour(BACKGROUND_COLOR).withAlpha(0.5f));
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(Colours::white);
        g.drawRoundedRectangle(b, 5.f, 2.f);

        auto logoBounds = b.removeFromTop(h * 0.66f).reduced(w * 0.2f);

        g.setColour(Colours::white);

        logo->drawWithin(g, logoBounds, RectanglePlacement::yTop, 1.f);

        g.setFont(16.f);
        Time time(Time::getCurrentTime());
        g.drawFittedText("v" + String(ProjectInfo::versionString) +
                             "\n" + currentWrapper +
                             "\n(c) Arboreal Audio " + String(time.getYear()),
                         b.toNearestInt(),
                         Justification::centred, 3, 1.f);
    }

    void mouseDown(const MouseEvent &event) override
    {
        auto b = getLocalBounds().reduced(1).toFloat();
        auto logoBounds = b.removeFromTop(b.getHeight() * 0.66f).reduced(b.getWidth() * 0.2f);

        if (logoBounds.contains(event.position))
        {
            if (isMouseButtonDown() && !linkClicked)
            {
                URL("https://arborealaudio.com").launchInDefaultBrowser();
                linkClicked = true;
                repaint();
                return;
            }
        }
        else
            Component::mouseDown(event);
    }

    void setImage(const Image &newImage)
    {
        img = newImage;

        Blur::blurImage<4, true>(img);

        repaint();
    }

    String currentWrapper;

    std::function<void()> onLogoClick;

private:
    bool linkClicked = false;

    Image img;

    std::unique_ptr<Drawable> logo;

};