// ChoiceMenu.h

#pragma once

struct MenuLookAndFeel : LookAndFeel_V4
{
    void drawComboBox(Graphics &g, int width, int height, bool, int, int, int,
                      int, ComboBox &comboBox) override
    {
        Rectangle<float> box(0.f, 0.f, (float)width, (float)height);
        box.reduce(3, 3);
        auto labelBounds = box.withSizeKeepingCentre(width * 0.75f, height);
        g.setColour(backgroundColor);
        g.fillRoundedRectangle(box, box.getHeight() * 0.5f);
        g.setColour(outlineColor);
        g.drawRoundedRectangle(box, box.getHeight() * 0.5f, 1.f);

        // chunk of L/R end used for arrow
        const float arrowSize = (float)box.getWidth() / 6.5f;
        const float padding = (float)box.getWidth() * 0.05f;
        const float strokeWidth = 3.f;

        auto drawArrow = [&](const Rectangle<float> &bounds, Path &path,
                             bool leftArrow) {
            const float r = bounds.getRight();
            const float bot = bounds.getBottom();
            const float centerY = bounds.getY() + (bounds.getHeight() / 2);
            const float arrowWidth = bounds.getWidth() * 0.9;
            if (leftArrow) {
                path.startNewSubPath(bounds.getX() + arrowWidth, bounds.getY());
                path.lineTo(bounds.getX(), centerY);
                path.lineTo(bounds.getX() + arrowWidth, bot);
            } else {
                path.startNewSubPath(r - arrowWidth, bounds.getY());
                path.lineTo(r, centerY);
                path.lineTo(r - arrowWidth, bot);
            }
            g.strokePath(path,
                         PathStrokeType(strokeWidth, PathStrokeType::beveled,
                                        PathStrokeType::rounded));
        };
        Path leftArrow;
        drawArrow(box.removeFromLeft(arrowSize).reduced(padding), leftArrow,
                  true);
        Path rightArrow;
        drawArrow(box.removeFromRight(arrowSize).reduced(padding), rightArrow,
                  false);

        g.setFont(Font(jlimit(10.f, 18.f, comboBox.getWidth() * 0.15f)));
        g.drawText(comboBox.getItemText(comboBox.getSelectedItemIndex()),
                   labelBounds, Justification::centred);
    }

    void drawLabel(Graphics &g, Label &label) override {}

    void drawPopupMenuItem(Graphics &g, const Rectangle<int> &area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const String &text,
                           const String &shortcutKeyText, const Drawable *icon,
                           const Colour *textColour) override
    {
        if (isHighlighted) {
            g.setColour(Colours::grey);
            g.fillRoundedRectangle(area.toFloat(), 10.f);
        }

        if (isTicked) {
            g.setColour(Colours::white);
            g.fillEllipse(
                area.withTrimmedRight(area.getWidth() - area.getHeight())
                    .reduced(area.getHeight() * 0.33f)
                    .toFloat());
        }

        g.setColour(Colours::white);
        g.setFont(jlimit(10.f, 18.f, area.getHeight() * 0.75f));
        g.drawFittedText(text, area, Justification::centred, 1);
    }

    Colour outlineColor = Colours::white;
    Colour backgroundColor = Colour(0x00000000);
};

struct ChoiceMenu : ComboBox
{
    ChoiceMenu(StringArray itemList)
    {
        lnf.setColour(PopupMenu::ColourIds::backgroundColourId,
                      Colour(BACKGROUND_COLOR));
        setLookAndFeel(&lnf);
        addItemList(itemList, 1);
    }
    ~ChoiceMenu() override { setLookAndFeel(nullptr); }

    void leftArrowClicked()
    {
        const auto currentIndex = getSelectedItemIndex();
        if (currentIndex != 0)
            setSelectedItemIndex(currentIndex - 1);
        else
            setSelectedItemIndex(getNumItems() - 1);
    }

    void rightArrowClicked()
    {
        const auto currentIndex = getSelectedItemIndex();
        if (currentIndex < getNumItems())
            setSelectedItemIndex(currentIndex + 1);
        else
            setSelectedItemIndex(0);
    }

    void mouseDown(const MouseEvent &event) override
    {
        if (leftArrow.contains(event.position) && event.mouseWasClicked()) {
            leftArrowClicked();
            return;
        }
        if (rightArrow.contains(event.position) && event.mouseWasClicked()) {
            rightArrowClicked();
            return;
        }

        ComboBox::mouseDown(event);
    }

    void resized() override
    {
        const float w = getWidth();
        const float subChunk = w - (w / 8.f);
        auto b = getLocalBounds().toFloat();
        leftArrow = b.withTrimmedRight(subChunk);
        rightArrow = b.withTrimmedLeft(subChunk);
        ComboBox::resized();
    }

    MenuLookAndFeel lnf;

  private:
    Rectangle<float> leftArrow, rightArrow;
};
