#ifndef TUNER_H
#define TUNER_H

#include <stdint.h>
#include <math.h>
#include <cstring>
#include <atomic>
#include <JuceHeader.h>
#include <Arbor_modules.h>

#define TUNER_MAX_BUFFER_SIZE 4096 * 8 // Max expected input buffer * 4B/sample

static uint8_t tuner_heap[TUNER_MAX_BUFFER_SIZE] = {0};

#define TUNER_RMS_SIZE 100 // ms

struct DCBlock {
	float x1 = 0, y1 = 0, r = 0.995;
	void process(double *in, uint32_t num_samples) {
		for (int i = 0; i < num_samples; ++i) {
			auto y = in[i] - x1 + r * y1;
			x1 = in[i];
			y1 = y;
			in[i] = y;
		}
	}
};

struct Gate {
	const float threshold_dB = -48.f;
	const float threshold = pow(10, threshold_dB * 0.05);

	void process(double*, uint32_t) {}
};

struct Tuner {
	DCBlock dc_block;
	Gate gate;
	double *buffer;
	uint32_t buffer_len;
	uint32_t buffer_read_pos;
	uint32_t buffer_write_pos;

	strix::Filter<double> lp{strix::FilterType::lowpass};

	std::atomic<bool> buffer_loaded = false;
	// TODO: Need a buffer of pitch info

	void init(double, uint32_t);
	void createMonoBuffer(const double* const*, int, int);
	void process(uint32_t);
	void visualize(uint8_t*, int, int);
};

#endif
