/*
 * tflite_benchmark.cc
 *
 * This a benchmark application allowing to test TesorflowLite model using EdgeTPU
 * AI hardware accelerator.
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


#include <iostream>
#include <getopt.h>     // needed to process arguments
#include <signal.h>     // needed for signal function
#include <sys/time.h>   // needed for gettimeofday function
#include <numeric>

#include "wrapper_tfl_edgetpu.hpp"

#define INPUT_MEAN  127.5f
#define INPUT_STD   127.5f

/* Application parameters */
std::string model_file_str;
unsigned int nb_loops = 1;


/* Tflite-EdgeTPU parameters */
std::shared_ptr<edgetpu::EdgeTpuContext>* edgetpu_context;
struct tflite::wrapper_tfl::Interpreter* interpreter;
struct tflite::wrapper_tfl::Config config;
float input_mean = INPUT_MEAN;
float input_std = INPUT_STD;

double get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }
double get_ms(struct timeval t) { return (t.tv_sec * 1000 + t.tv_usec / 1000); }

/**
 * This function display the help when -h or --help is passed as parameter.
 */
static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage: " << argv[0] << "./tflite-edgetpu-benchmark -m <model .tflite> -l <label .txt file>\n"
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
    const char* const short_opts = "m:l:h";
    const option long_opts[] = {
        {"model_file",   required_argument, nullptr, 'm'},
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
	exit(0);
}

int main(int argc, char** argv)
{
    /* Catch CTRL + C signal */
    signal(SIGINT, int_handler);
    /* Catch kill signal */
    signal(SIGTERM, int_handler);

    process_args(argc, argv);

    config.verbose = false;
    config.accel = false;
    config.input_floating = false;
    config.profiling = false;
    config.allow_fp16 = false;
    config.loop_count = 1;
    config.input_mean = input_mean;
    config.input_std = input_std;
    config.number_of_threads = 2;
    config.number_of_results = 5;
    config.model_name = model_file_str;
    config.labels_file_name = std::string();

    std::vector<double> m_inferenceTimes;

    std::shared_ptr<edgetpu::EdgeTpuContext> edgetpu_context_ptr = edgetpu::EdgeTpuManager::GetSingleton()->OpenDevice();
    edgetpu_context = &edgetpu_context_ptr;

    interpreter = tflite::wrapper_tfl::InitInterpreter(&config, edgetpu_context);
    int nn_input_width  = tflite::wrapper_tfl::GetInputWidth(interpreter);
    int nn_input_height = tflite::wrapper_tfl::GetInputHeight(interpreter);
    int nn_buffer_size  = nn_input_width * nn_input_height * 3;
    uint8_t *nn_img;

    // Allocate memory for tensor
    if(interpreter->impl->AllocateTensors() != kTfLiteOk){
            std::cout << "Failed to allocate tensors\n";
            exit(0);
    }

    std::cout << "\ninferences are running: " << std::flush;
    for(unsigned int i = 0; i <= nb_loops ; i++){
        nn_img = (uint8_t*)malloc(nn_buffer_size * sizeof(uint8_t));
        struct timeval start_time, stop_time;
        gettimeofday(&start_time, nullptr);
        tflite::wrapper_tfl::RunInference(&config, interpreter, edgetpu_context, nn_img);
        gettimeofday(&stop_time, nullptr);
        interpreter->inference_time = float(get_ms(stop_time) - get_ms(start_time));
        if (i != 0) m_inferenceTimes.push_back((get_us(stop_time) - get_us(start_time)));
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

