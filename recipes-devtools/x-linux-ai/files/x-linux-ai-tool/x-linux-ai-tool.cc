/*
 * Copyright (c) 2020 STMicroelectronics. All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *
 *     http://www.opensource.org/licenses/BSD-3-Clause
 *
 * Author(s): Alexis BRISSON <alexis.brisson@st.com> for STMicroelectronics
 */

#include <iostream>
#include "readme.h"
#include <filesystem>
#include <stdio.h>
#include <getopt.h>
#include <numeric>
#include <sys/types.h>

/* Define global variables */
bool version = false;
bool features = false;
bool appli = false;
extern const std::string README_VERSION;
extern const std::string README_FEATURES;
extern const std::string README_APPLI;

static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage:" << argv[0] << "\n"
		"\n"
		"-v --version :  Show X-LINUX-AI current version if it is installed\n"
		"-f --supported-features : Print all supported frameworks in this X-LINUX-AI version\n"
		"-a --supported-appli :    Print all delivered applications in this X-LINUX-AI version\n"
		"-h --help :     Show this help\n";
	exit(1);
}

void process_args(int argc, char** argv)
{
	const char* const short_opts = ":-vfah:";
	const option long_opts[] = {
		{"version",  no_argument, nullptr, 'v'},
		{"supported-features", no_argument, nullptr, 'f'},
		{"supported-appli",    no_argument, nullptr, 'a'},
		{"help",     no_argument, nullptr, 'h'},
		{nullptr,    no_argument, nullptr,  0 }
	};

	while (true)
	{
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

		if (-1 == opt)
			break;

		switch (opt)
		{
		case 'v':
			version = true;
			break;
		case 'f':
			features = true;
			break;
		case 'a':
			appli = true;
			break;
		case 'h': // -h or --help
		case '?': // Unrecognized option
		default:
			print_help(argc, argv);
			break;
		}
	}
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
	/* Process the application parameters */
	process_args(argc, argv);

	std::stringstream version_sstr;
	std::stringstream features_sstr;
	std::stringstream appli_sstr;

	if(version){
		std::cout << "X-LINUX-AI version: " << README_VERSION << std::endl;
	}
	else if(features){
		std::cout << "Features:\n " << README_FEATURES << std::endl;
		std::cout << "Find more informations on the wiki page: https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package" << std::endl;
	}
	else if(appli){
		std::cout << "Applications:\n " << README_APPLI << std::endl;
			}
	else {
		std::cout << "Wrong argument, please refer to the help below" << std::endl;
		print_help(argc, argv);
	}
	return 0;
}
