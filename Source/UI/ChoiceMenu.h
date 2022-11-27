// ChoiceMenu.h

#pragma once

struct MenuLookAndFeel : LookAndFeel_V4
{
    MenuLookAndFeel(){}
    ~MenuLookAndFeel(){}

    void drawComboBox(Graphics& g, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, ComboBox& comboBox) override
    {
        if (comboBox.isMouseOver(true)) {
            g.setColour(Colours::grey);
            g.fillRoundedRectangle(0.f, 0.f, (float)width, (float)height, 10.f);
        }
        else {
            g.setColour(Colours::white);
            Rectangle<float> box(0.f, 0.f, (float)width, (float)height);
            g.drawRoundedRectangle(box.reduced(3), 10.f, 1.f);
        }
    }

    void positionComboBoxText(ComboBox& box, Label& label) override
    {
        label.setBounds(box.getLocalBounds());
    }

    void drawLabel(Graphics& g, Label& label) override
    {
        g.setColour(Colours::white);
        g.drawFittedText(label.getText(), label.getLocalBounds(), Justification::centred, 1);
    }
};

struct ChoiceMenu : ComboBox
{
    ChoiceMenu(StringArray itemList)
    {
        setLookAndFeel(&lnf);
        addItemList(itemList, 1);
    }
    ~ChoiceMenu()
    {
        setLookAndFeel(nullptr);
    }

private:
    MenuLookAndFeel lnf;
};