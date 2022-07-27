// Background.h

#pragma once

struct Background : public Component
{
    Background()
    {
        drawing = Drawable::createFromImageData(BinaryData::drawing_png, BinaryData::drawing_pngSize);
    }
    ~Background(){}

    void paint(Graphics& g) override
    {
        drawing->drawWithin(g, getLocalBounds().toFloat(), RectanglePlacement::centred, 1.f);
    }

private:
    std::unique_ptr<Drawable> drawing;
};
