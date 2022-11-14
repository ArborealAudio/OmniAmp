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
{ "https://arborealaudio.com/versions/Gamma-latest.json" };
#else
{ "https://arborealaudio.com/versions/test/Gamma-latest.json" };
#endif

const String downloadURLWin
{ "https://arborealaudio.com/downloads/Gamma-windows.exe" };

const String downloadURLMac
{ "https://arborealaudio.com/downloads/Gamma-mac.dmg" };

struct DownloadManager : Component
{
    DownloadManager(bool hasUpdated) : updated(hasUpdated)
    {
        addAndMakeVisible(yes);
        addAndMakeVisible(no);
        addChildComponent(retry);

        if (checkForUpdate() && !updated) {
            setVisible(true);
        }
        else {
            setVisible(false);
        }

        no.onClick = [&]
        {
            setVisible(false);
            updated = true;
            if (onUpdateChange)
                onUpdateChange(updated);
        };
        yes.onClick = [&] { downloadFinished.store(false); downloadUpdate(); };
    }

    ~DownloadManager() override
    {
        yes.setLookAndFeel(nullptr);
        no.setLookAndFeel(nullptr);
        retry.setLookAndFeel(nullptr);
    }

    bool checkForUpdate()
    {
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
            else {
                changes = changesObj;
            }
            
            auto latestVersion = data.getProperty("version", var());
            
            DBG("Current: " << String(ProjectInfo::versionString));
            DBG("Latest: " << latestVersion.toString());
            
#if PRODUCTION_BUILD
            return String(ProjectInfo::versionString).removeCharacters(".") < latestVersion.toString().removeCharacters(".");
#else
            DBG("Update result: " << int(String(ProjectInfo::versionString).removeCharacters(".")
                < latestVersion.toString().removeCharacters(".")));
            return true;
#endif
        }
        else
            return false;
    }

    void downloadUpdate()
    {
        download.setDownloadBlockSize(64000);
        download.setProgressInterval(60);
        download.setRetryDelay(2);
        download.setRetryLimit(2);
#if JUCE_WINDOWS
        download.startAsyncDownload(URL(downloadURLWin), result, progress);
#elif JUCE_MAC
        download.startAsyncDownload(URL(downloadURLMac), result, progress);
#endif
        isDownloading.store(true);
        isInstalling.store(false);
    }

    void paint(Graphics& g) override
    {
        if (isVisible()) {
            g.setColour(Colours::grey.darker());
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 15.f);
            
            g.setColour(Colours::white);

            auto textbounds = Rectangle<int>{ getLocalBounds().reduced(10).withTrimmedBottom(70) };

            if (!downloadFinished.load()) {
                if (!isDownloading.load())
                    g.drawFittedText("A new update is available! Would you like to download?\n\nChanges:\n" + changes, textbounds, Justification::centredTop, 10, 0.f);
                else
                    g.drawFittedText("Downloading... " + String(downloadProgress.load(), 2) + "%",
                        textbounds, Justification::centred,
                        1, 1.f);
            }
            else {
                if (downloadStatus.load()) {
                    g.drawFittedText("Download complete.\nThe installer is in your Downloads folder. You must close your DAW to run the installation.",
                        textbounds, Justification::centred,
                        7, 1.f);
                    yes.setVisible(false);
                    no.setButtonText("Close");
                }
                else {
                    g.drawFittedText("Download failed. Please try again.",
                        textbounds, Justification::centred,
                        7, 1.f);
                    retry.setVisible(true);
                    retry.onClick = [this] {yes.onClick(); };
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

        Rectangle<int> yesBounds{ bounds.withTrimmedTop(halfHeight).withTrimmedRight(halfWidth).reduced(20, 30)};
        Rectangle<int> noBounds{ bounds.withTrimmedTop(halfHeight).withTrimmedLeft(halfWidth).reduced(20, 30) };
        Rectangle<int> retryBounds{ bounds.withTrimmedTop(halfHeight).reduced(20, 30) };

        yes.setBounds(yesBounds);
        no.setBounds(noBounds);
        retry.setBounds(retryBounds);
    }

    std::function<void(bool)> onUpdateChange;

private:
    gin::DownloadManager download;

    TextButton yes{ "Yes" }, no{ "No" }, retry{ "Retry" };

    bool updated = false;

    std::atomic<bool> downloadStatus = false;
    std::atomic<bool> isDownloading = false;
    std::atomic<bool> isInstalling = false;
    std::atomic<float> downloadProgress;
    std::atomic<bool> downloadFinished = false;

    String changes;

    std::function<void(gin::DownloadManager::DownloadResult)> result =
        [this](gin::DownloadManager::DownloadResult downloadResult)
    {
        repaint();

        downloadStatus.store(downloadResult.ok);

        if (!downloadResult.ok)
            return;

#if JUCE_WINDOWS
        auto exe = File("~/Downloads/Gamma-windows.exe");
        
        if (!downloadResult.saveToFile(exe))
            downloadStatus.store(false);
        else
            downloadFinished.store(true);
#elif JUCE_MAC
        auto dmg = File("~/Downloads/Gamma-mac.dmg");

        if (!downloadResult.saveToFile(dmg))
            downloadStatus.store(false);
        else
            downloadFinished.store(true);
#endif
        
        updated = true;
        if (onUpdateChange)
            onUpdateChange(updated);
        repaint();
    };

    std::function<void(int64, int64, int64)> progress = [this](int64 downloaded, int64 total, int64)
    {
        downloadProgress = 100.f * ((float)downloaded / (float)total);
        repaint();
    };

};
