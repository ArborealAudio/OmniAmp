/*
  ==============================================================================

    DownloadManager.h
    Created: 3 Nov 2021 4:43:12pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

const String versionURL
#if PRODUCTION_BUILD
    {"https://arborealaudio.com/versions/Gamma-latest.json"};
#else
    {"https://arborealaudio.com/versions/test/Gamma-latest.json"};
#endif

/* these can function as a sort of "dummy" endpoint which we can redirect to AWS */
const String downloadURL
#if JUCE_WINDOWS
    {"https://arborealaudio.com/downloads/Gamma-windows.exe"};
#elif JUCE_MAC
    {"https://arborealaudio.com/downloads/Gamma-mac.dmg"};
#elif JUCE_LINUX
    {"https://arborealaudio.com/downloads/Gamma-linux.tar.gz"};
#endif

const String downloadPath
{
    "~/Downloads/"
#if JUCE_WINDOWS
    "Gamma-windows.exe"
#elif JUCE_MAC
    "Gamma-mac.dmg"
#elif JUCE_LINUX
    "Gamma-linux.tar.gz"
#endif
};

struct DownloadManager : Component
{
    DownloadManager()
    {
        addAndMakeVisible(yes);
        addAndMakeVisible(no);

        no.onClick = [&]
        {
            if (!isDownloading)
            {
                onUpdateCheck(false);
            }
            else
            {
                download.cancelAllDownloads();
                isDownloading = false;
                downloadFinished = false;
                no.setButtonText("No");
                yes.setVisible(true);
                yes.setButtonText("Yes");
                repaint();
            }
        };
        yes.onClick = [&]
        { downloadFinished = false; downloadUpdate(); };
    }

    ~DownloadManager() override
    {
        yes.setLookAndFeel(nullptr);
        no.setLookAndFeel(nullptr);
    }

    /** @param force whether to force the check even if checked < 24hrs ago */
    void checkForUpdate(bool force = false)
    {
        if (!force)
        {
            auto lastCheck = readConfigFileString("updateCheck").getLargeIntValue();
            auto dayAgo = Time::getCurrentTime() - RelativeTime::hours(24);
            MessageManager::callAsync([&]
                                      { onUpdateCheck(false); });
            if (lastCheck > dayAgo.toMilliseconds())
                return;
        }

        if (auto stream = URL(versionURL).createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inAddress)))
        {
            auto data = JSON::parse(stream->readEntireStreamAsString());

            auto changesObj = data.getProperty("changes", var());
            if (changesObj.isArray())
            {
                std::vector<String> chVec;
                for (size_t i = 0; i < changesObj.size(); ++i)
                {
                    chVec.emplace_back(changesObj[i].toString());
                }

                StringArray changesList{chVec.data(), (int)chVec.size()};
                changes = changesList.joinIntoString(", ");
            }
            else
            {
                changes = changesObj;
            }

            auto latestVersion = data.getProperty("version", var());

            DBG("Current: " << String(ProjectInfo::versionString));
            DBG("Latest: " << latestVersion.toString());

#if PRODUCTION_BUILD
            updateAvailable = String(ProjectInfo::versionString).removeCharacters(".") < latestVersion.toString().removeCharacters(".");
#else
            DBG("Update result: " << int(String(ProjectInfo::versionString).removeCharacters(".") < latestVersion.toString().removeCharacters(".")));
            updateAvailable = true;
#endif
        }
        else
            updateAvailable = false;

        writeConfigFileString("updateCheck", String(Time::currentTimeMillis()));

        MessageManager::callAsync([&]
                                  { onUpdateCheck(updateAvailable); });
    }

    void downloadUpdate()
    {
        download.setDownloadBlockSize(64 * 128);
        download.setProgressInterval(100);
        download.setRetryDelay(1);
        download.setRetryLimit(2);
        download.startAsyncDownload(URL(downloadURL), result, progress);
        isDownloading = true;
        no.setButtonText("Cancel");
        yes.setVisible(false);
    }

    void paint(Graphics &g) override
    {
        if (isVisible())
        {
            g.setColour(Colours::grey.darker());
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 15.f);

            g.setColour(Colours::white);

            auto textbounds = Rectangle<int>{getLocalBounds().reduced(10).withTrimmedBottom(70)};

            if (!downloadFinished.load())
            {
                if (!isDownloading.load())
                    g.drawFittedText("A new update is available! Would you like to download?\n\nChanges:\n" + changes, textbounds, Justification::centredTop, 10, 0.f);
                else
                    g.drawFittedText("Downloading... " + String(downloadProgress.load()) + "%",
                                     textbounds, Justification::centred,
                                     1, 1.f);
            }
            else
            {
                if (downloadStatus.load())
                {
                    g.drawFittedText("Download complete.\nThe installer is in your Downloads folder. You must close your DAW to run the installation.",
                                     textbounds, Justification::centred,
                                     7, 1.f);
                    yes.setVisible(false);
                    no.setButtonText("Close");
                }
                else
                {
                    g.drawFittedText("Download failed. Please try again.",
                                     textbounds, Justification::centred,
                                     7, 1.f);
                    yes.setVisible(true);
                    yes.setButtonText("Retry");
                    yes.onClick = [this]
                    { downloadProgress = 0;
                    downloadFinished = false;
                    isDownloading = true;
                    downloadUpdate(); };
                }
            }
        }
        else
            return;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.reduce(10, 10);
        auto halfWidth = bounds.getWidth() / 2;
        auto halfHeight = bounds.getHeight() / 2;

        Rectangle<int> yesBounds{bounds.withTrimmedTop(halfHeight).withTrimmedRight(halfWidth).reduced(20, 30)};
        Rectangle<int> noBounds{bounds.withTrimmedTop(halfHeight).withTrimmedLeft(halfWidth).reduced(20, 30)};

        yes.setBounds(yesBounds);
        no.setBounds(noBounds);
    }

    bool updateAvailable = false;

private:

    void onUpdateCheck(bool checkResult)
    {
        if (isVisible() != checkResult)
            setVisible(checkResult);
    }

    gin::DownloadManager download;

    TextButton yes{"Yes"}, no{"No"};

    std::atomic<bool> downloadStatus = false;
    std::atomic<bool> isDownloading = false;
    std::atomic<int> downloadProgress = 0;
    std::atomic<bool> downloadFinished = false;

    String changes;

    std::function<void(gin::DownloadManager::DownloadResult)> result =
        [&](gin::DownloadManager::DownloadResult downloadResult)
    {
        downloadStatus = downloadResult.ok;
        isDownloading = false;

        if (!downloadResult.ok || downloadResult.httpCode != 200)
        {
            downloadFinished = true;
            repaint();
            return;
        }

        auto bin = File(downloadPath);
        if (!downloadResult.saveToFile(bin))
            downloadStatus = false;
        else
            downloadFinished = true;

        repaint();
    };

    std::function<void(int64, int64, int64)> progress = [&](int64 downloaded, int64 total, int64)
    {
        downloadProgress = 100 * ((float)downloaded / (float)total);
        repaint();
    };
};
