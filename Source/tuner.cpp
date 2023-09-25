#include "tuner.h"
#include <assert.h>

void Tuner::init(double sample_rate, uint32_t num_samples) {
	buffer = (double *)&tuner_heap;
	buffer_len = ((TUNER_RMS_SIZE / 1000.0) * sample_rate) / num_samples;
	buffer_read_pos = 0;
	buffer_write_pos = 0;

	lp.init(1, sample_rate, 5000.0, 0.707);
}

void Tuner::createMonoBuffer(const double* const* in, int num_ch,
							 int num_samples) {
	assert(num_ch == 2);
	// if (buffer_loaded) return;
	
	const double *L = in[0];
	const double *R = in[1];
	double sum = 0;
	for (int i = 0; i < num_samples; ++i) {
		double out = (L[i] + R[i]) / 2.f;
		sum += out * out;
	}
	// Lowpass
	lp.process(&buffer, 1, num_samples);

	buffer[buffer_write_pos++] = std::sqrt(sum / (double)num_samples);
	buffer_write_pos %= buffer_len;
	buffer_loaded = true;
}

void Tuner::process(uint32_t num_samples) {
	
	// Process DC blocker
	dc_block.process(buffer, num_samples);

	// Process gate
	// gate.process(buffer, num_samples);

	// Process pitch detection
	for (int i = 0; i < num_samples; i++) {
		auto env = std::abs(buffer[i]);
	}
}

// TODO: Smooth out the visualization, mainly by taking RMS, but also we will
// need a windowing function, all of which will prob
// be implemented up top when we copy data over. Is there something we could do
// with a gradual falloff where we smooth out the decay of the wave?

void Tuner::visualize(uint8_t *pixels, int w, int h) {
	const double scale = 5.0;
	for (int x = 0; x < w; ++x) {
		double samp = (scale * buffer[buffer_read_pos++] + 1) * 0.5;
		for (int y = 0; y < h; ++y) {
			int pixel_amp = samp * (double)h; // round sample to pixels
			pixels[y*w+x] = pixel_amp == y ? 0xff : 0;
		}
		buffer_read_pos %= buffer_len;
	}
}
