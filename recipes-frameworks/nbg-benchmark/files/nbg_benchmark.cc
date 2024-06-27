/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <fstream>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#include <getopt.h>
#include <iostream>
#include <time.h>
#include <pthread.h>
#include "vnn_utils.h"
#include <sys/resource.h>

#define MAX_INPUT_COUNT  32
#define MAX_OUTPUT_COUNT 32
#define VIP_MAC     (768)
#define GPU_CLK_FD "/sys/kernel/debug/gc/clk"

std::string network_binary_file;
std::string input_file[MAX_INPUT_COUNT];
uint64_t case_mmac = 0;
unsigned int nb_loops = 1;

#define BILLION 1000000000
static uint64_t get_perf_count()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)((uint64_t)ts.tv_nsec + (uint64_t)ts.tv_sec * BILLION);
}

/**
 * This function display the help when -h or --help is passed as parameter.
 */
static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage: " << argv[0] << " -m <nbg_file .nb> -i <input_file .tensor/.txt> -c <int case_mmac> -l <int nb_loops>\n"
		"\n"
		"-m --nb_file <.nb file path>:               .nb network binary file to be benchmarked.\n"
		"-i --input_file <.tensor/.txt/ file path>:  Input file to be used for benchmark (maximum 32 input files).\n"
		"-c --case_mmac <int>:                        Theorical value of MMAC (Million Multiply Accu) of the model.\n"
		"-l --loops <int>:                           The number of loops of the inference (default loops=1)\n"
		"--help:                                     Show this help\n";
	exit(1);
}

/**
 * This function parse the parameters of the application -m and -l are
 * mandatory.
 */
void process_args(int argc, char** argv)
{
	const char* const short_opts = "m:i:c:l:p:h";
	const option long_opts[] = {
		{"nb_file", required_argument, nullptr, 'm'},
		{"input_file", optional_argument, nullptr, 'i'},
		{"case_mmac",   optional_argument, nullptr, 'c'},
		{"loops",      optional_argument, nullptr, 'l'},
		{"help",       no_argument,       nullptr, 'h'},
		{nullptr,      no_argument,       nullptr, 0}
	};

	unsigned int i = 0;

	while (true)
	{
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

		if (-1 == opt)
			break;

		switch (opt)
		{
		case 'm':
			network_binary_file = std::string(optarg);
			std::cout << "Info: Network binary file set to: " << network_binary_file << std::endl;
			break;
		case 'i':
			input_file[i] = std::string(optarg);
			std::cout << "Info: Using " << input_file[i] << " as an input " << i << " for benchmarking." << std::endl;
			i++;
			break;
		case 'c':
			case_mmac = std::stoi(optarg);
			std::cout << "Info: Using " << case_mmac << " as a case MMAC for " << network_binary_file << " model." << std::endl;
			break;
		case 'l':
			nb_loops = std::stoi(optarg);
			std::cout << "Info: Executing  " << nb_loops << " inference(s) during this benchmark." << std::endl;
			break;
		case 'h': // -h or --help
		case '?': // Unrecognized option
		default:
			print_help(argc, argv);
			break;
		}
	}

	if (network_binary_file.empty() || argc < 2)
		print_help(argc, argv);
}

/**
 * This functions checks if VIP_MAC and FREQUENCY env variables
 * are set and runs the graph nb_loops times, computes and prints
 * the average inference time and MAC utilization of the model.
 */
vx_status vnn_ProcessGraph(vx_graph   graph,
			   unsigned int loops,
			   uint64_t case_mmac)
{
	vx_status   status = VX_FAILURE;
	uint64_t tStart, tEnd;
	float msAvg, usAvg;
	float rUtil = 0, rtime = 0;
	uint64_t npu_freq;

	/* Get NPU frequency */
	std::ifstream gc_clk_fd(GPU_CLK_FD);
	std::string gc_clk_mc;
	if (std::getline(gc_clk_fd, gc_clk_mc)) {
		std::stringstream str_gc_clk_mc(gc_clk_mc);
		std::string gc_clk_mc_freq;
		for (unsigned int i = 1; i <= 4; i++) {
			if (str_gc_clk_mc >> gc_clk_mc_freq) {
				if (i == 4) {
					npu_freq = std::stoul(gc_clk_mc_freq);
					std::cout << "Info: NPU running at frequency: " << npu_freq << "Hz." << std::endl;
				}
			}
		}
	}

	printf("Info: Started running the graph [%d] loops ...\n", loops);
	if (case_mmac == 0){
		std::cout << "Info: No Case MAC has been specified for this model." << std::endl;
		std::cout << "Info: The MAC Utilization cannot be computed." << std::endl;
	}
	tStart = get_perf_count();
	for (unsigned int i = 0; i < loops; i++)
	{
		status = vxProcessGraph(graph);
		_CHECK_STATUS(status, exit);
	}
	tEnd = get_perf_count();
	msAvg = (float)(tEnd - tStart) / 1000000 / loops;
	usAvg = (float)(tEnd - tStart) / 1000 / loops;

	if (case_mmac != 0){
		rtime = (float)(tEnd - tStart) / 1000000000 / loops;
		rUtil = case_mmac * 1000000 / ((float)((VIP_MAC * npu_freq)) * rtime);
		printf("Info: MAC utilization is %.2f%% with caseMAC set to %ld Million of MAC\n", rUtil*100, case_mmac);
	}
	printf("Info: Loop:%d,Average: %.2f ms or %.2f us\n", loops, msAvg, usAvg);
exit:
	return status;
}

/* Get RAM Usage */
std::size_t GetPeakWorkingSetSize(){
	struct rusage rusage;
	getrusage(RUSAGE_SELF, &rusage);
	return static_cast<size_t>(rusage.ru_maxrss * 1024L);
}

/*-------------------------------------------
  Main Function
  -------------------------------------------*/
int main(int argc, char **argv){
	vx_context context      = NULL;
	vx_graph   graph        = NULL;
	vx_kernel  kernel       = NULL;
	vx_node    node         = NULL;
	vx_status  status       = VX_FAILURE;
	vx_tensor  input_tensor = NULL;
	std::size_t peak_memory = 0;

	inout_obj inputs[MAX_INPUT_COUNT];
	inout_obj outputs[MAX_OUTPUT_COUNT];
	inout_param inputs_param[MAX_INPUT_COUNT] ;
	inout_param outputs_param[MAX_OUTPUT_COUNT];

	process_args(argc, argv);

	vx_int32 input_count  = 0;
	vx_int32 output_count = 0;

	uint64_t tStart, tEnd, msVal, usVal;
	int ret;

	ZEROS(inputs_param);
	ZEROS(outputs_param);

	context = vxCreateContext();
	_CHECK_OBJ(context, exit);

	graph = vxCreateGraph(context);
	_CHECK_OBJ(graph, exit);

	kernel = vxImportKernelFromURL(context, VX_VIVANTE_IMPORT_KERNEL_FROM_FILE, network_binary_file.c_str());
	status = vxGetStatus((vx_reference)kernel);
	_CHECK_STATUS(status, exit);

	node = vxCreateGenericNode(graph, kernel);
	status = vxGetStatus((vx_reference)node);
	_CHECK_STATUS(status, exit);

	status = vnn_QueryInputsAndOutputsParam(kernel, inputs_param, &input_count, outputs_param, &output_count);
	_CHECK_STATUS(status, exit);

	for (int j = 0; j < input_count; j++) {
		status |= vnn_CreateObject(context, &inputs_param[j],&inputs[j]);
		_CHECK_STATUS(status, exit);
		status |= vxSetParameterByIndex(node, j, (vx_reference)inputs[j].u.ref);
		_CHECK_STATUS(status, exit);
	}
	for (int j = 0; j < output_count; j++) {
		status |= vnn_CreateObject(context,&outputs_param[j],&outputs[j]);
		_CHECK_STATUS(status, exit);
		status |= vxSetParameterByIndex(node, j + input_count, (vx_reference)outputs[j].u.ref);;
		_CHECK_STATUS(status, exit);
	}

	std::cout << "Info: Verifying graph..." << std::endl;
	tStart = get_perf_count();
	status = vxVerifyGraph(graph);
	_CHECK_STATUS(status, exit);
	tEnd = get_perf_count();
	msVal = (tEnd - tStart)/1000000;
	usVal = (tEnd - tStart)/1000;
	printf("Info: Verifying graph took: %ldms or %ldus\n", msVal, usVal);

	if (input_file[0].empty()) {
		inout_obj* obj = &inputs[0];
		input_tensor = obj->u.tensor;
		vnn_LoadTensorRandom(input_tensor);
		_CHECK_STATUS(status, exit);
	} else {
		for (int j = 0; j < input_count; j++){
			status = vnn_LoadDataFromFile(&inputs[j], &input_file[j][0]);
			_CHECK_STATUS(status, exit);
		}
	}

	ret = vnn_ProcessGraph(graph, nb_loops, case_mmac);

	for(int j = 0;j < output_count;j++) {
		char filename[128];
		sprintf(filename,"output_%d.txt",j);
		status = vnn_SaveDataToFile(&outputs[j],filename);
		_CHECK_STATUS(status, exit);
	}

	peak_memory = GetPeakWorkingSetSize();
	std::cout << "Info: Peak working set size: " << peak_memory << " bytes" << std::endl;

exit:
	for (int j = 0; j < input_count; j++)
		status = vnn_ReleaseObject(&inputs[j]);
	for (int j = 0; j < output_count; j++)
		status = vnn_ReleaseObject(&outputs[j]);
	if (kernel != NULL)
		vxReleaseKernel(&kernel);
	if (node != NULL)
		vxReleaseNode(&node);
	if (graph != NULL)
		vxReleaseGraph(&graph);
	if (context != NULL)
		vxReleaseContext(&context);

	return 0;
}