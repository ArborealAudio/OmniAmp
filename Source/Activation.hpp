// Activation.hpp
// A quick little MD5 hash checker for the beta version. Only checks the hash of a predetermined string which shall be given to beta testers. Should be replaced with the final version when ready.

#pragma once

#if BETA_BUILD

#define KEY "gamma-beta-1108"

struct HashChecker
{
    HashChecker() : reference(KEY)
    {
    }

    // expects UNhashed string, checks against expected hash
    bool checkString(const String& input)
    {
        auto inHash = MD5(input.toUTF8()).toHexString();
        auto refHash = MD5(reference.toUTF8()).toHexString();
        return inHash == refHash;
    }

    // compare a hash input to expected hash
    bool compareHash(const String &input)
    {
        auto refHash = MD5(reference.toUTF8()).toHexString();
        return refHash == input;
    }

    String getHash(const String& input)
    {
        return MD5(input.toUTF8()).toHexString();
    }

private:
    String reference;
};
#endif

struct ActivationComponent : Component
{
    ActivationComponent()
    {
        addAndMakeVisible(editor);
        editor.grabKeyboardFocus();
        editor.onReturnKey = [&] {
            checkInput();
        };
        editor.setTextToShowWhenEmpty("License", Colours::lightgrey);

        addAndMakeVisible(submit);
        submit.onClick = [&] {
            checkInput();
        };
    }

    std::function<void(bool)> onActivationCheck;

    void checkInput()
    {
        auto text = editor.getText();
        if (check.checkString(text))
        {
            writeFile(check.getHash(text).toRawUTF8());
            if (onActivationCheck)
                onActivationCheck(true);
        }
        else
        {
            if (onActivationCheck)
                onActivationCheck(false);

            editor.clear();
            editor.giveAwayKeyboardFocus();
            editor.setTextToShowWhenEmpty("Invalid license...", Colours::red);
            repaint(getLocalBounds());
        }
    }

    void paint(Graphics& g) override
    {
        g.setColour(Colours::darkslategrey);
        g.fillRoundedRectangle(getLocalBounds().reduced(5).toFloat(), 10.f);

        g.setFont(22.f);
        g.setColour(Colours::white);
        g.drawFittedText("Please enter your license key:", getLocalBounds().removeFromTop(getHeight() * 0.3f), Justification::centred, 3);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto editorPadding = BorderSize<int>(h * 0.4, 20, h * 0.4, 20);
        editor.setBoundsInset(editorPadding);

        submit.setBounds(b.removeFromBottom(h * 0.3f).reduced(50, 10));
    }

    bool readFile()
    {
        File config {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
        if (!config.existsAsFile())
            return false;
        
        auto xml = parseXML(config);
        if (xml == nullptr)
            return false;

        auto key = xml->getStringAttribute("Key");
        auto result = check.compareHash(key);
        if (onActivationCheck)
            onActivationCheck(result);
        return result;
    }

private:
    HashChecker check;

    TextEditor editor;

    TextButton submit{"Submit"};

    std::atomic<bool> lastResult;

    void writeFile(const char* key)
    {
        File config {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
        if (!config.existsAsFile())
            config.create();

        auto xml = parseXML(config);
        if (xml == nullptr)
        {
            xml.reset(new XmlElement("Config"));
        }

        xml->setAttribute("Key", key);
        xml->writeTo(config);
    }
};