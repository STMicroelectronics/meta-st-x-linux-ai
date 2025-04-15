/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

#include <iostream>
#include "readme.h"
#include <filesystem>
#include <stdio.h>
#include <getopt.h>
#include <numeric>
#include <cstdlib>
#include <sys/types.h>
#include <fstream>
#include <algorithm>
#include <set>
#include <string>
#include <cstring>
#include <typeinfo>
#include <vector>
#include <regex>
#include <array>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

/* Define global variables */
bool list = false;
bool to_remove = false;
bool to_install = false;
bool version = false;
bool features = false;

extern const std::string WIKI_LINK;
extern const std::string README_APPLI;
extern const std::string README_VERSION;
extern const std::string README_FEATURES;

static void print_help(int argc, char** argv)
{
    std::cout <<
        "\nUsage:\t'" << argv[0] << " -[option]'\n"
        "\n"
        "-v --version            : Show X-LINUX-AI current version if it is installed\n"
        "-f --supported-features : Print all supported frameworks in this X-LINUX-AI version\n"
        "-l --list               : Print installed and ready-to-install packages\n"
        "-i --install <pkg>      : Install X-LINUX-AI package\n"
        "-r --remove  <pkg>      : Remove X-LINUX-AI package\n"
        "-h --help               : Show this help\n";
    exit(0);
}

void process_args(int argc, char** argv)
{
    const char* const short_opts = "i:r:vfhl";
    const option long_opts[] = {
        {"remove",   required_argument, nullptr, 'r'},
        {"install",  required_argument, nullptr, 'i'},
        {"supported-features",  no_argument, nullptr, 'f'},
        {"version",  no_argument, nullptr, 'v'},
        {"list",     no_argument, nullptr, 'l'},
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
        case 'l':
            list = true;
            break;
        case 'r':
            to_remove = true;
            break;
        case 'i':
            to_install = true;
            break;
        case 'h': // -h or --help
        case '?': // Unrecognized option
        default:
            print_help(argc, argv);
            break;
        }
    }
}

/* Get x-linux-ai pkgs list */
std::set<std::string> _get_pkg(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cout << "\nTo install x-linux-ai packages, please follow the instructions provided on the wiki page: \n" + WIKI_LINK << std::endl;
        exit(1);
    }

    std::set<std::string> pkgs;
    std::string line;
    while (getline(file, line)) {
        if (line.find("Package: ") == 0 && line.find("x-linux-ai-tool") == std::string::npos) {
            pkgs.insert(line.substr(9));
        }
    }
    return pkgs;
}

// Function to check if the output contains any of the patterns
int _contains_pattern(const std::string &output, const std::vector<std::string> &patterns) {
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (output.find(patterns[i]) != std::string::npos) {
            return i; // Return the index of the matching pattern
        }
    }
    return -1; // No pattern matched
}

std::string _exCommand(const char *cmd)
{
    // A fixed-size array to temporarily hold segments of the command's output
    std::array<char, 128> buffer;
    std::string result;
    // Open a pipe to run the command and read its output
    // The unique_ptr ensures that the pipe is closed with pclose when it goes out of scope
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    // Check if the pipe was successfully opened
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    // Read the command's output in segments and append each segment to the result
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Remove the trailing newline character from the result, if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string _get_platform() {
    std::string command = "cat /proc/device-tree/compatible";

    // Execute the command and capture the output
    std::string output = _exCommand(command.c_str());

    // Patterns to be checked
    const char *patterns[] = {"stm32mp257", "stm32mp235", "stm32mp255"};
    size_t num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    // Convert patterns to std::vector<std::string>
    std::vector<std::string> pattern_vector(patterns, patterns + num_patterns);

    // Check if the output contains any of the patterns
    int pattern_index = _contains_pattern(output, pattern_vector);

    std::string platform;

    // Return platform used
    if (pattern_index != -1) {
        platform = "STM32MP_NPU";
    } else {
        platform = "STM32MP_CPU";
    }

    return platform;
}

std::set<std::string> _set_common_pkgs(auto ostl_pkg, auto x_pkg) {
    std::set<std::string> common_pkg;
    std::set_intersection(ostl_pkg.begin(), ostl_pkg.end(),
                          x_pkg.begin(), x_pkg.end(),
                          std::inserter(common_pkg, common_pkg.begin()));
    return common_pkg;
}

std::set<std::string> _get_x_pkg(auto ostl_pkg, auto x_pkg) {
    std::set<std::string> av_x_pkg;
    for (const auto& pkg : x_pkg) {
        if (!ostl_pkg.count(pkg)) {
            av_x_pkg.insert(pkg);
        }
    }
    return av_x_pkg;
}

void print_pkgs(const std::string& ostl_pkg_path, const std::string& x_pkg_path) {

    auto ostl_pkg = _get_pkg(ostl_pkg_path);
    auto x_pkg = _get_pkg(x_pkg_path);
    std::set<std::string> common_pkg = _set_common_pkgs(ostl_pkg, x_pkg);

    if(common_pkg.empty()){
        std::cout << "\n X-LINUX-AI packages are not currently installed. Please find below a list of all the available packages.\n" << std::endl;
    }
    else{
        std::cout << "\n";
        for (const auto& pkg : common_pkg) {
            std::cout << " "
                      << "[installed]\t    "
                      << pkg
                      << std::endl;
        }
    }
    x_pkg = _get_x_pkg(ostl_pkg, x_pkg);
    if (x_pkg.empty()) {
        std::cout << "\n All X-LINUX-AI packages are already installed and up to date.\n" << std::endl;
    }
    else{
        /* Ready-to-Install pkgs */
        std::cout << "\n";
        for (const auto& pkg : x_pkg) {
            std::cout << " "
                      << "[not installed]"
                      << "    "
                      << pkg
                      << std::endl;
        }
    }
    std::cout << "\n";
}

/* This function is used to install and uninstall pkgs */
void manage_pkgs(int argc, char** argv, bool install = true) {
    std::string command = (install ? "apt-get update && apt-get install -y " : "apt-get autoremove -y ") + std::string(argv[2]);
    int result = system(command.c_str());
    if (result == 0) {
        std::cout << std::string(argv[2])
                  << " has been "
                  << (install ? "installed" : "removed")
                  << " successfully."
                  << std::endl;
    } else {
        std::cout << "E: "
                  << "Failed to "
                  << (install ? "install" : "remove")
                  << " package "
                  << std::string(argv[2])
                  << std::endl;
        exit(1);
    }
}

// Function to check if the output contains any of the patterns
int contains_pattern(const char *output, const char *patterns[], size_t num_patterns) {
    for (size_t i = 0; i < num_patterns; ++i) {
        if (strstr(output, patterns[i]) != NULL) {
            return i; // Return the index of the matching pattern
        }
    }
    return -1; // No pattern matched
}

int install_pkg_repo(){

    //check if AI repo is installed
    std::string check_package = "dpkg -l | grep apt-openstlinux-x-linux-ai- > /dev/null";
    int repo_set = system(check_package.c_str());
    if (repo_set == 0) {
        return 0;
    } else {
        printf("X-LINUX-AI repo is not set\n");
        printf("Prepare X-LINUX-AI repo \n");
        std::string platform;
        platform = _get_platform();
        if (platform=="STM32MP_NPU"){
            // Execute the specific apt-get install command
            printf("Updating APT database ...\n");
            if (system("sudo -E apt-get update  > /dev/null") == -1) {
                perror("sudo -E apt-get update failed");
                return 1;
            }
            printf("Setting up X-LINUX-AI package repo ...\n");
            if (system("sudo -E apt-get install -y apt-openstlinux-x-linux-ai-npu  > /dev/null") == -1) {
                perror("setting AI package repo failed");
                return 1;
            }
        } else {
            printf("Updating APT database ...\n");
            if (system("sudo -E apt-get update") == -1) {
                perror("sudo -E apt-get update failed");
                return 1;
            }
            printf("Setting up X-LINUX-AI package repo ...\n");
            // Execute the alternative apt-get install command
            if (system("sudo -E apt-get install -y apt-openstlinux-x-linux-ai-cpu  > /dev/null") == -1) {
                perror("setting AI package repo failed");
                return 1;
            }
        }

        //check if old AI repo is installed
        printf("Cleaning the repo ...\n");
        std::string check_old_repo = "dpkg -l | grep apt-openstlinux-x-linux-ai  > /dev/null";
        int result = system(check_old_repo.c_str());
        if (result == 0) {
            //old AI repo is already installed
            if (system("sudo -E apt-get update > /dev/null ") == -1) {
            perror("sudo -E apt-get update failed");
            return 1;
            }
            // clean old packages with autoremove
            if (system("sudo -E apt-get autoremove > /dev/null") == -1) {
                perror("sudo -E apt-get autoremove failed");
                return 1;
            }
        }
        return 0;
    }
}

std::string _get_x_pkg_path(const std::string& pattern, const std::vector<std::string>& directories) {
    std::regex regexPattern(pattern);

    for (const auto& dir : directories) {
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (std::regex_search(filename, regexPattern)) {
                        return entry.path().string();
                    }
                }
            }
        }
    }
    return "";
}

/// Main function ///
int main(int argc, char *argv[])
{
    process_args(argc, argv);

    int repo_set;
    repo_set = install_pkg_repo();
    if (repo_set == 0){
        std::cout << "X-LINUX-AI package repo is already set." << std::endl;
    } else {
        std::cout << "X-LINUX-AI package repo is not set properly." << std::endl;
    }

    if (list) {
        std::vector<std::string> directories = {
            "/var/lib/apt/lists/",
            "/var/lib/apt/lists/auxfiles/"
        };

        std::string platform;
        std::string pattern;
        platform = _get_platform();
        if (platform=="STM32MP_NPU"){
            pattern= ".*_AINPU_.*_main_.*";
        } else {
            pattern= ".*_AICPU_.*_main_.*";
        }
        std::string x_pkg_path = _get_x_pkg_path(pattern, directories);
        if (x_pkg_path.empty()) {
            std::cout << "list of AI packages not found." << std::endl;
            return -1;
        }
        /* Get ostl installed packages */
        std::string ostl_pkg_path = "/var/lib/dpkg/status";
        print_pkgs(ostl_pkg_path, x_pkg_path);
    }
    else if (to_install && argc == 3) {
        manage_pkgs(argc, argv,true);
    }
    else if (to_remove && argc == 3) {
        manage_pkgs(argc, argv,false);
    }
    else if (version) {
        std::cout << "\nX-LINUX-AI version: " << README_VERSION << "\n" << std::endl;
    }
    else if (features) {
        std::cout << "\nFeatures:\n " << README_FEATURES << std::endl;
        std::cout << "\nFind more information on the wiki page: https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package" << std::endl;
        std::cout << "\nApplications:\n " << README_APPLI << "\n" << std::endl;
    }
    else{
        print_help(argc, argv);
    }
    return 0;
}