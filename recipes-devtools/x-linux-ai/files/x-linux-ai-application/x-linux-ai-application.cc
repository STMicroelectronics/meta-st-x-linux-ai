/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#include "x-linux-ai-application.h"

/* Global variables */
bool list = false;
bool run = false;
bool found = false;

std::string inputUseCase = "";
std::string inputModelType = "";
std::string inputFileLanguage = "";

static void print_help(int argc, char** argv)
{
    std::cout <<
        "\nUsage:\t'" << argv[0] << " -r <use-case> --model-type <model-type> --language <language>'\n"
        "\n"
        "-l --list            : list all the available applications\n"
        "-r --run <str>       : run application with a specific use case\n"
        "--model-type <str>   : run application with a specific model type\n"
        "--language <str>     : run application with a specific program language\n"
        "-h --help            : Show this help\n\n";
    exit(0);
}

void process_args(int argc, char** argv)
{
    const char* const short_opts = "r:m:s:lh";
    const option long_opts[] = {
        {"run",        required_argument, nullptr, 'r'},
        {"model-type", required_argument, nullptr, 'm'},
        {"language",   required_argument, nullptr, 's'},
        {"list",             no_argument, nullptr, 'l'},
        {"help",             no_argument, nullptr, 'h'},
        {nullptr,            no_argument, nullptr,  0 }
    };

    while (true)
    {
        const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

        if (-1 == opt)
            break;
        switch (opt)
        {
        case 'l':
            list = true;
            break;
        case 'r':
            inputUseCase = std::string(optarg);
            run = true;
            break;
        case 'm':
            inputModelType = std::string(optarg);
            break;
        case 's':
            inputFileLanguage = std::string(optarg);
            break;
        case 'h': // -h or --help
        case '?': // Unrecognized option
        default:
            print_help(argc, argv);
            break;
        }
    }
}

/// Main function ///

int main(int argc, char* argv[]) {

    process_args(argc, argv);

    if (!list && !run){
        print_help(argc, argv);
        exit(1);
    }

    Application XLinuxAI;

    // recover x-linux-ai package list depending on the directory used
    std::vector<std::string> directories = {
        "/var/lib/apt/lists/",
        "/var/lib/apt/lists/auxfiles/"
    };

    std::string platform;
    std::string pattern;
    platform = XLinuxAI.get_platform();
    if (platform=="STM32MP_NPU"){
        pattern= ".*_AINPU_.*_main_.*";
    } else {
        pattern= ".*_AICPU_.*_main_.*";
    }

    std::string x_pkg_path = XLinuxAI.get_x_pkg_path(pattern, directories);
    if (x_pkg_path.empty()) {
        std::cout << "list of AI packages not found." << std::endl;
        return -1;
    }
    std::set<std::string> xAppList = XLinuxAI.getAiApplicationList(x_pkg_path);
    std::set<std::string> ostlAppList = XLinuxAI.getAiApplicationList(Application::ostlAppFilePath);
    std::set<std::string> installedAppList = XLinuxAI.getInstalledAiApplicationList(xAppList, ostlAppList);
    std::set<std::string> notInstalledAppList = XLinuxAI.getNotInstalledAiApplicationList(xAppList, ostlAppList);
    std::set<std::string> useCasesPathList = XLinuxAI.getUseCasePathList(Application::dirPath);
    std::set<std::string> launchFilePathList = XLinuxAI.getLaunchFilePathList(useCasesPathList);
    std::set<std::string> useCasesList = XLinuxAI.getUseCaseList();

    if (list) {
        XLinuxAI.displayUseCaseList(useCasesList, installedAppList);
    }

    if(run){
        XLinuxAI.runApplication(inputUseCase, inputModelType, inputFileLanguage, launchFilePathList, notInstalledAppList, installedAppList);
    }

    return 0;
}