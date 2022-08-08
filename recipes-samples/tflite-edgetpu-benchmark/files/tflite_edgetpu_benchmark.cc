/*
 * tflite_benchmark.cc
 *
 * This a benchmark application allowing to test TesorflowLite model using EdgeTPU
 * AI hardware accelerator.
 *
 * Author: Vincent Abriou <vincent.abriou@st.com> for STMicroelectronics.
 *
 * Copyright (c) 2020 STMicroelectronics. All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *
 *     http://www.opensource.org/licenses/BSD-3-Clause
 */


#include <iostream>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <numeric>

#include "wrapper_tfl_edgetpu.hpp"

/* Application parameters */
std::string model_file_str;
unsigned int nb_loops = 1;

/* Tflite-EdgeTPU parameters */
wrapper_tfl::Tfl_WrapperEdgeTPU tfl_wrapper_tpu;
struct wrapper_tfl::Config config;

/**
 * This function display the help when -h or --help is passed as parameter.
 */
static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage: " << argv[0] << "./tflite-edgetpu-benchmark -m <model .tflite> \n"
		"\n"
		"-m --model_file <.tflite file path>:  .tflite model to be executed\n"
		"-l --loops <int>:                     provide the number of time the inference will be executed (by default nb_loops=1)\n"
		"--help:                               show this help\n";
	exit(1);
}

/**
 * This function parse the parameters of the application -m and -l ar
 * mandatory.
 */
void process_args(int argc, char** argv)
{
	const char* const short_opts = "m:l:p:h";
	const option long_opts[] = {
		{"model_file", required_argument, nullptr, 'm'},
		{"loops",      required_argument, nullptr, 'l'},
		{"help",       no_argument,       nullptr, 'h'},
		{nullptr,      no_argument,       nullptr, 0}
	};

	while (true)
	{
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

		if (-1 == opt)
			break;

		switch (opt)
		{
		case 'm':
			model_file_str = std::string(optarg);
			std::cout << "model file set to: " << model_file_str << std::endl;
			break;
		case 'l':
			nb_loops = std::stoi(optarg);
			std::cout << "This benchmark will execute " << nb_loops << " inference(s)" << std::endl;
			break;
		case 'h': // -h or --help
		case '?': // Unrecognized option
		default:
			print_help(argc, argv);
			break;
		}
	}
	if (model_file_str.empty())
		print_help(argc, argv);
}

/**
 * Function called when CTRL + C or Kill signal is destected
 */
static void int_handler(int dummy)
{
	exit(-1);
}

int main(int argc, char** argv)
{
	std::vector<double> m_inferenceTimes;

	/* Catch CTRL + C signal */
	signal(SIGINT, int_handler);
	/* Catch kill signal */
	signal(SIGTERM, int_handler);

	process_args(argc, argv);

	/*  Check if the Edge TPU is connected */
	int status = system("lsusb -d 1a6e:");
	status &= system("lsusb -d 18d1:");
	if (status) {
		std::cout << "ERROR: Edge TPU not connected.\n";
		return -1;
	}

	config.verbose = false;
	config.model_name = model_file_str;

	tfl_wrapper_tpu.Initialize(&config);

	if (!tfl_wrapper_tpu.IsModelQuantized()) {
		std::cout << "The model is not quantized! Please quantized it for egde tpu usage...\n";
		return -1;
	}

	int nn_input_width  = tfl_wrapper_tpu.GetInputWidth();
	int nn_input_height = tfl_wrapper_tpu.GetInputHeight();
	int nn_input_channels = tfl_wrapper_tpu.GetInputChannels();
	int nn_buffer_size  = nn_input_width * nn_input_height * nn_input_channels;
	uint8_t *nn_img;

	std::cout << "\ninferences are running: " << std::flush;
	for(unsigned int i = 0; i <= nb_loops ; i++){
		double inference_time;
		nn_img = (uint8_t*)malloc(nn_buffer_size * sizeof(uint8_t));
		inference_time = tfl_wrapper_tpu.RunInference(nn_img);
		if (i != 0)
			m_inferenceTimes.push_back(inference_time);
		std::cout << "# " << std::flush;
	}
	auto maxInfTime = *std::max_element(m_inferenceTimes.begin(), m_inferenceTimes.end());
	auto minInfTime = *std::min_element(m_inferenceTimes.begin(), m_inferenceTimes.end());
	auto avgInfTime = std::accumulate(m_inferenceTimes.begin(), m_inferenceTimes.end(), 0.0) / m_inferenceTimes.size();
	std::cout << "\n\ninference time: ";
	std::cout << "min=" << minInfTime << "us  ";
	std::cout << "max=" << maxInfTime << "us  ";
	std::cout << "avg=" << avgInfTime << "us" << std::endl;
	free(nn_img);

	return 0;
}

