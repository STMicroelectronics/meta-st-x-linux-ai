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
#include <typeinfo>
#include <vector>
#include <array>
#include <stdexcept>

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
        "-i --install <pkg>      : Install   X-LINUX-AI  package\n"
        "-r --remove  <pkg>      : Remove    X-LINUX-AI  package\n"
        "-h --help               : Show    this help\n";
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
    std::string command = (install ? "apt-get install -y " : "apt-get autoremove -y ") + std::string(argv[2]);
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

std::string exec_command(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

/// Main function ///

int main(int argc, char *argv[])
{
    process_args(argc, argv);
    std::string command = "ls /var/lib/apt/lists/*_AI_*_main_*";
    std::string ostl_pkg_path = "/var/lib/dpkg/status";
    std::string x_pkg_path = exec_command(command.c_str());

    if (!x_pkg_path.empty() && x_pkg_path.back() == '\n') {
        x_pkg_path.pop_back();
    }

    if (list) {
        print_pkgs(ostl_pkg_path, x_pkg_path);
    }
    else if (to_install && argc == 3) {
        manage_pkgs(argc, argv,true);
    }
    else if (to_remove && argc == 3) {
        manage_pkgs(argc, argv,false);
    }
    else if (version) {
        std::cout << "\nX-LINUX-AI version: " << README_VERSION << std::endl;
    }
    else if (features) {
        std::cout << "\nFeatures:\n " << README_FEATURES << std::endl;
        std::cout << "\nFind more information on the wiki page: https://wiki.st.com/stm32mpu/wiki/X-LINUX-AI_OpenSTLinux_Expansion_Package" << std::endl;
        std::cout << "\nApplications:\n " << README_APPLI << std::endl;
    }
    else{
        print_help(argc, argv);
    }
    return 0;
}
