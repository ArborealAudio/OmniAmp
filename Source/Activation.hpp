// Activation.hpp

#pragma once
#include <JuceHeader.h>

struct ActivationComponent : Component
{
    enum CheckResult
    {
        None,
        Success,
        InvalidLicense,
        ActivationsMaxed,
        WrongProduct,
        ConnectionFailed
    };

    enum CheckStatus
    {
        NotSubmitted,
        Finished
    };

    ActivationComponent(int64 _trialRemaining) : trialRemaining(_trialRemaining)
    {
        addAndMakeVisible(editor);
        editor.setFont(Font(18.f));
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

        addAndMakeVisible(close);
        close.onClick = [&]
        {
            setVisible(false);
        };

        addAndMakeVisible(buy);
        buy.onClick = []{
            URL("https://arborealaudio.com/plugins/omniamp").launchInDefaultBrowser();
        };
    }

    /* called when UI submits a license key & when site check is successful.
    Basically you set the visibility of this ActivationComponent and set the
    AudioProcessor's `isUnlocked` variable */
    std::function<void(bool)> onActivationCheck;

    void checkInput()
    {
        auto text = editor.getText();
        if (text.isEmpty())
        {
            editor.clear();
            editor.setTextToShowWhenEmpty("Actually enter a license...", Colour::fromHSL((float)color / 360.f, 1.f, 0.75f, 1.f));
            color += 45;
            if (color % 360 == 0)
                color = 0;
            if (onActivationCheck)
                onActivationCheck(false);
            repaint();
            return;
        }
        m_result = checkSite(text, false);
        if (m_result == CheckResult::Success)
        {
            if (checkSite(text, true) == CheckResult::Success)
            {
                writeFile(text.toRawUTF8());
                if (onActivationCheck)
                    onActivationCheck(true);
                editor.setVisible(false);
                close.setVisible(true);
                close.setEnabled(true);
                submit.setVisible(false);
            }
        }
        else
        {
            if (onActivationCheck)
                onActivationCheck(false);

            editor.setVisible(true);
            editor.clear();
            editor.giveAwayKeyboardFocus();
            editor.setTextToShowWhenEmpty("Invalid license...", Colours::red);
        }
        m_status = Finished;
        repaint();
    }

    void paint(Graphics &g) override
    {
        g.setColour(Colours::black.withAlpha(0.9f));
        g.fillRoundedRectangle(getLocalBounds().reduced(5).toFloat(), 10.f);

        g.setFont(18.f);
        g.setColour(Colours::white);
        String message;
        String trialDesc = "";
        if (m_status == NotSubmitted)
        {
            message = "Enter your license:";
        }
        else if (m_status == Finished)
        {
            g.setColour(Colours::red);
            switch (m_result)
            {
                case Success:
                    message = "License activated! Thank you!";
                    g.setColour(Colours::white);
                    break;
                case InvalidLicense:
                    message = "Invalid license";
                    break;
                case ActivationsMaxed:
                    message = "Activations maxed";
                    break;
                case WrongProduct:
                    message = "Wrong product";
                    break;
                case ConnectionFailed:
                    message = "Connection failed. Try again.";
                    break;
                case None:
                    message = "Activation...not run? Error!";
                    break;
            }
        }
        if (trialRemaining > 0 && m_result != Success)
            trialDesc = RelativeTime::milliseconds(trialRemaining).getDescription() + " remaining in free trial";
        else if (trialRemaining <= 0 && m_result != Success)
            trialDesc = "Trial expired.";
        g.drawFittedText(message + "\n" + trialDesc, getLocalBounds().removeFromTop(getHeight() * 0.3f), Justification::centred, 3);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto editorPadding = BorderSize<int>(h * 0.4, 20, h * 0.4, 20);
        editor.setBoundsInset(editorPadding);

        auto buttons = b.removeFromBottom(h * 0.3f);
        submit.setBounds(buttons.removeFromLeft(w / 3).reduced(10));
        close.setBounds(buttons.removeFromLeft(w / 3).reduced(10));
        buy.setBounds(buttons.removeFromLeft(w / 3).reduced(10));
    }

    CheckResult checkSite(const String &input, bool activate)
    {
        auto url = URL("https://3pvj52nx17.execute-api.us-east-1.amazonaws.com");
        if (activate)
            url = url.withNewSubPath("/default/licenses/activate/" + input);
        else
            url = url.withNewSubPath("/default/licenses/" + input);

        CheckResult result = CheckResult::ConnectionFailed;

        if (auto stream = url.createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                                                .withExtraHeaders("x-api-key: Fb5mXNfHiNaSKABQEl0PiFmYBthvv457bOCA1ou2")
                                                .withConnectionTimeoutMs(10000)))
        {
            if (activate)
                return CheckResult::Success;
            
            auto response = stream->readEntireStreamAsString();
            auto json = JSON::parse(response);

            if ((bool)json.getProperty("success", var(false)) != true)
                return CheckResult::InvalidLicense;
            
            auto item = json.getProperty("Item", var());
            if (item.isObject())
            {
                // it should be a JSON object
                auto product = item.getProperty("product", var(false));
                auto numActivations = item.getProperty("activationCount", var());
                auto maxActivations = item.getProperty("maxActivations", var());

                if (product.toString() != "omniamp")
                    return CheckResult::WrongProduct;
                if (numActivations >= maxActivations)
                    return CheckResult::ActivationsMaxed;

                return CheckResult::Success;
            }
            else
                return CheckResult::InvalidLicense;
        }

        return result;
    }

    TextEditor editor;
    TextButton submit{"Submit"}, close{"Close"}, buy{"Buy"};

private:
    std::atomic<CheckResult> m_result = None;
    std::atomic<CheckStatus> m_status = NotSubmitted;

    int64 trialRemaining = 0;

    uint16 color;

    void writeFile(const char *key)
    {
        File license {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/OmniAmp/License/license"};
        if (!license.existsAsFile())
            license.create();

        license.appendText(String(key));
    }
};