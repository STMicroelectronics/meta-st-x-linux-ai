/*
 * armnn_tfl_benchmark.cc
 *
 * This a benchmark application allowing to test TesorflowLite model using armnn
 * framework.
 *
 * Copyright (C) 2020, STMicroelectronics - All Rights Reserved
 * Author: Vincent Abriou <vincent.abriou@st.com> for STMicroelectronics.
 *
 * License type: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <getopt.h>
#include <signal.h>

#include "wrapper_armnn_tfl.hpp"

/* Application parameters */
std::vector<armnn::BackendId> preferred_backends_order = {armnn::Compute::CpuAcc, armnn::Compute::CpuRef};
std::string model_file_str;
std::string preferred_backend_str;
unsigned int nb_loops = 1;

/* armNN variables */
wrapper_armnn_tfl::Tfl_Wrapper tfl_wrapper;
wrapper_armnn_tfl::Config config;

/**
 * This function display the help when -h or --help is passed as parameter.
 */
static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage: " << argv[0] << " -m <model .tflite> -l <label .txt file>\n"
		"\n"
		"-m --model_file <.tflite file path>:  .tflite model to be executed\n"
		"-b --backend <device>:                preferred backend device to run layers on by default. Possible choices: " << armnn::BackendRegistryInstance().GetBackendIdsAsString() << "\n"
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
	const char* const short_opts = "m:b:l:h";
	const option long_opts[] = {
		{"model_file",   required_argument, nullptr, 'm'},
		{"backend",      required_argument, nullptr, 'b'},
		{"loops",        required_argument, nullptr, 'l'},
		{"help",         no_argument,       nullptr, 'h'},
		{nullptr,        no_argument,       nullptr, 0}
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
		case 'b':
			preferred_backend_str = std::string(optarg);
			/* Overwrite the prefered backend order */
			if (preferred_backend_str == "CpuAcc")
				preferred_backends_order = {armnn::Compute::CpuAcc, armnn::Compute::CpuRef};
			else if (preferred_backend_str == "CpuRef")
				preferred_backends_order = {armnn::Compute::CpuRef, armnn::Compute::CpuAcc};

			std::cout << "preferred backend device set to:";
			for (unsigned int i = 0; i < preferred_backends_order.size(); i++) {
				std::cout << " " << preferred_backends_order.at(i);
			}
			std::cout << std::endl;
			break;
		case 'l':
			nb_loops = std::stoi(optarg);
			std::cout << "benchmark will execute " << nb_loops << " inference(s)" << std::endl;
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
	exit(0);
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
	/* Catch CTRL + C signal */
	signal(SIGINT, int_handler);
	/* Catch kill signal */
	signal(SIGTERM, int_handler);

	process_args(argc, argv);

	/* TensorFlow Lite wrapper initialization */
	config.model_name = model_file_str;
	config.compute_devices = preferred_backends_order;

	tfl_wrapper.Initialize(&config);

	tfl_wrapper.RunInference(nb_loops);

	return 0;
}
