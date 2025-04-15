/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */


#if !defined(_X_LINUX_AI_APPLICATION_H_)
#define _X_LINUX_AI_APPLICATION_H_

#include <sys/stat.h>
#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <stdlib.h>
#include <dirent.h>
#include <cstdlib>
#include <getopt.h>
#include <sstream>
#include <numeric>
#include <array>
#include <algorithm>
#include <string>
#include <cstring>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>
#include <regex>
#include <unordered_set>

/**
 * @brief A class designed to enhance the intelligence of AI applications and select
 * the most optimal applications based on the user's board.
 */
class Application{

public:

    /**
     * @brief Constructor for the X-LINUX-AI Application class without arguments.
     */
    Application();

    /**
     * @brief Destructor for the X-LINUX-AI Application class.
     */
    ~Application();

    /**
     * @brief Recover x-linux-ai package list.
     */
    static std::string get_x_pkg_path(const std::string& pattern, const std::vector<std::string>& directories);

    /**
     * @brief Recover platform used.
     */
    static std::string get_platform();

    /**
     * @brief Recover model type if not defined.
     */
    static std::map<std::string, std::string> set_modelType();

    /**
     * @brief Executes a shell command and captures its output.
     *
     * This function uses `popen` to execute a shell command and reads the command's
     * output into a string. The function ensures that the pipe is properly closed
     * using a unique pointer with a custom deleter.
     *
     * @param cmd The shell command to execute.
     * @return The output of the command as a string.
     * @throws std::runtime_error if `popen` fails to open the pipe.
     */
    static std::string exCommand(const char* cmd);

    /**
     * @brief Get use cases path list from a directory path.
     *
     * @param dirPath Path to x-linux-ai applications.
     *
     * @return a set of use cases path
     */
    std::set<std::string> getUseCasePathList(const std::string& dirPath);

    /**
     * @brief Get launch file path list from a list of use cases path.
     *
     * @param useCasePathList List of all the available use cases.
     *
     * @return set of launch files paths
     */
    std::set<std::string> getLaunchFilePathList(const std::set<std::string>& useCasePathList);

    /**
     * @brief Get use case list.
     *
     * @param machine machine name.
     *
     * @return useCaseList
     */
    std::set<std::string> getUseCaseList();

    /**
     * @brief Format file path to get only the fileName.
     *
     * @param path file path or directory path.
     *
     * @return fileName
     */
    static std::string formatPath(const std::filesystem::path& path);

    /**
     * @brief Print warning message and guide users to install the application.
     *
     * @param xApp x-linux-ai application.
     */
    static void warningMsg(const std::string& xApp);

    /**
     * @brief Print warning message and guide users to install the application.
     *
     * @param xApp x-linux-ai application.
     */
    static void printApplicationInfo(const std::string& modelType, const std::string& useCase, const std::string& fileLanguage, std::string& filePath);

    /**
     * @brief Extract File language from a file name.
     *
     * @param fileName.
     *
     * @return extractedFileLanguage.
     */
    std::string extractFileLanguageFromFileName(const std::string& fileName);

    /**
     * @brief Extract use case from a file path.
     *
     * @param filePath.
     *
     * @return extractedUseCase.
     */
    std::string extractUseCaseFromFilePath(const std::string& filePath);

    /**
     * @brief Extract model type from#include <map> a file path.
     *
     * @param filePath.
     *
     * @return extractedUseCase.
     */
    std::string extractModelTypeFromFilePath(const std::string& filePath);

    /**
     * @brief Get a list of all AI Applications.
     *
     * @param filePath Contain a list of all AI Applications.
     *
     * @return appList
     */
    std::set<std::string> getAiApplicationList(const std::string& filePath);

    /**
     * @brief Get a list of all the installed AI Applications.
     *
     * @param xAppList Contain x-linux-ai Applications.
     *
     * @param installedAppList Contain openSTLinux Applications.
     *
     * @return installedAppList
     */
    std::set<std::string> getInstalledAiApplicationList(const std::set<std::string>& xAppList, const std::set<std::string>& ostlAppList);

    /**
     * @brief Get a list of not installed AI Applications.
     *
     * @param xAppList Contain x-linux-ai Applications.
     *
     * @param ostlAppList Contain openSTLinux Applications.
     *
     * @return notInstalledAppList
     */
    std::set<std::string> getNotInstalledAiApplicationList(const std::set<std::string>& xAppList, const std::set<std::string>& ostlAppList);

    /**
     * @brief Display a list of all the available use cases.
     *
     * @param uCaseList Contain x-linux-ai use cases.
     *
     * @param inApp Contain installed Applications.
     */
    void displayUseCaseList(std::set<std::string> uCaseList, std::set<std::string> inAppList);

    /**
     * @brief This function select the best application for the specific board.
     *
     * @param launchFilePathList This set contain a list of all the application launch files.
     *
     * @param inputUscase This set contain installed Applications.
     *
     * @param inputModelType This set contain installed Applications.
     *
     * @param inputFileLanguage This set contain installed Applications.
     *
     * @param notInAppList This set contain the not installed Applications.
     *
     * @param inAppList This set contain installed Applications.
     *
     * @return launchFileApp.
     */
    std::string getBestPerf(const std::set<std::string>& launchFilePathList, const std::string& inputUscase, const std::string& inputModelType, const std::string& inputFileLanguage, const std::set<std::string>& notInAppList, const std::set<std::string>& inAppList);

    /**
     * @brief This function select the best application for the specific board.
     *
     * @param launchFilePathList This set contain a list of all the application launch files.
     *
     * @param inputUscase This set contain installed Applications.
     *
     * @param inputFileLanguage This set contain installed Applications.
     *
     * @param notInAppList This set contain the not installed Applications.
     *
     * @param inAppList This set contain installed Applications.
     *
     * @return launchFileApp.
     */
    std::string getBestPerf(const std::set<std::string>& launchFilePathList, const std::string& inputUscase, const std::string& inputFileLanguage, const std::set<std::string>& notInAppList, const std::set<std::string>& inAppList);

    /**
     * @brief This function select the best application for the specific board.
     *
     * @param launchFilePathList This set contain a list of all the application launch files.
     *
     * @param inputUscase This set contain installed Applications.
     *
     * @param notInAppList This set contain the not installed Applications.
     *
     * @param inAppList This set contain installed Applications.
     *
     * @return launchFileApp.
     */
    std::string getBestPerf(const std::set<std::string>& launchFilePathList, const std::string& inputUscase, const std::set<std::string>& notInAppList, const std::set<std::string> &inAppList);

    /**
     * @brief This function is responsible for calling the getBestPerf function,
     * this function has two overloads: one in the case where the user wants to
     * launch an application using only a useCase, and an overload in the case
     * where the user has entered all the parameters.
     */
    void runApplication(const std::string& inputUscase,
                        const std::string& inputModelType,
                        const std::string& inputFileLanguage,
                        const std::set<std::string>& launchFilePathList,
                        const std::set<std::string>& notInAppList,
                        const std::set<std::string> &inAppList);

private:
    std::string _fileLanguage;
    std::set<std::string> _useCasePathList;
    std::set<std::string> _launchFilePathList;
    std::set<std::string> _installedAppList;
    std::set<std::string> _notInstalledAppList;
    std::set<std::string> _useCaseList;

public:
    static std::string dirPath;
    static std::string ostlAppFilePath;
    static std::map<std::string, std::string> modelTypeList;
    static std::set<std::string> fileLanguageList;
};

#endif // _X_LINUX_AI_APPLICATION_H_
