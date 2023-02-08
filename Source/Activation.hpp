// Activation.hpp
// A quick little MD5 hash checker for the beta version. Only checks the hash of a predetermined string which shall be given to beta testers. Should be replaced with the final version when ready.

#pragma once
#include <thread>

#if BETA_BUILD || DEV_BUILD

#define KEY "gamma-beta-1108"

struct HashChecker
{
    HashChecker() : reference(KEY)
    {
    }

    // expects UNhashed string, checks against expected hash
    bool checkString(const String &input)
    {
        auto inHash = MD5(input.toUTF8()).toHexString();
        auto refHash = MD5(reference).toHexString();
        return inHash == refHash;
    }

    // compare a hash input to expected hash
    bool compareHash(const String &input)
    {
        auto refHash = MD5(reference).toHexString();
        return refHash == input;
    }

    String getHash(const String &input)
    {
        return MD5(input.toUTF8()).toHexString();
    }

private:
    const CharPointer_UTF8 reference;
};
#endif

struct ActivationComponent : Component, Timer
{
    ActivationComponent()
    {
        addAndMakeVisible(editor);
        editor.onReturnKey = [&]
        {
            checkInput();
        };
        editor.setTextToShowWhenEmpty("License", Colours::lightgrey);

        addAndMakeVisible(submit);
        submit.onClick = [&]
        {
            checkInput();
        };
        startTimer(1000);
    }

    void timerCallback() override
    {
        checkSite();
        stopTimer();
    }

    /* called when UI submits a beta key & when site check is successful */
    std::function<void(bool)> onActivationCheck;
    /* called when siteCheck is complete */
    std::function<void(bool)> onSiteCheck;

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

    void paint(Graphics &g) override
    {
        g.setColour(Colours::darkslategrey.withAlpha(0.9f));
        g.fillRoundedRectangle(getLocalBounds().reduced(5).toFloat(), 10.f);

        if (!m_betaLive)
        {
            g.setFont(22.f);
            g.setColour(Colours::white);
            g.drawFittedText("The beta period has ended. Thank you for participating.", getLocalBounds().reduced(getWidth() * 0.1f), Justification::centred, 3);
            return;
        }

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

    var isBetaLive() { return m_betaLive; }

    bool readFile()
    {
        File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
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

    TextEditor editor;
    TextButton submit{"Submit"};
    var m_betaLive;

private:

    HashChecker check;

    void writeFile(const char *key)
    {
        File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
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

    void checkSite()
    {
        auto url = URL("https://arborealaudio.com/.netlify/functions/beta-check");
        bool checkResult = false;

        if (auto stream = url.createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inAddress).withExtraHeaders("name: Gamma\nkey: ArauGammBeta").withConnectionTimeoutMs(10000)))
        {
            auto response = stream->readEntireStreamAsString();

            checkResult = strcmp(response.toRawUTF8(), "true") == 0;
        }
        else
            checkResult = false;

        if (onSiteCheck) onSiteCheck(checkResult);
    }
};