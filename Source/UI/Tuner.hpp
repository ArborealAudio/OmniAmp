#include "../PluginProcessor.h"
#include "../tuner.h"
#include "UI.h"
#include <JuceHeader.h>

#include <stdint.h>

struct TunerComponent : DocumentWindow, Timer
{
    TunerComponent(Tuner &t)
        : tuner(t), DocumentWindow("Tuner", Colours::black, 0),
		  view(Image::PixelFormat::SingleChannel, 400, 400, true) {
        setContentComponentSize(400, 400);

		startTimer(30);
    }

	~TunerComponent() { stopTimer(); }

    void paint(Graphics &g) override {
		const auto bounds = view.getBounds();
		Image::BitmapData bmp = Image::BitmapData(view, Image::BitmapData::readWrite);

		uint8_t *pixels = bmp.data;

		tuner.visualize(pixels, bounds.getWidth(), bounds.getHeight());

		g.drawImage(view, bounds.toFloat(), RectanglePlacement::fillDestination);
	}

	void timerCallback() override {
		if (tuner.buffer_loaded) {
			repaint();
			tuner.buffer_loaded = false;
		}
	}

private:
    Tuner &tuner;

	Image view;
};
