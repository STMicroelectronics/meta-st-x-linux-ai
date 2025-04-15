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

/* Constructor */
Application::Application(){
    _fileLanguage = "";
    _useCasePathList = {};
    _launchFilePathList = {};
    _installedAppList = {};
    _notInstalledAppList = {};
}

/* Destructor */
Application::~Application(){}

/* Global variables */
std::string Application::dirPath = "/usr/local/x-linux-ai";
std::string Application::ostlAppFilePath = "/var/lib/dpkg/status";
std::map<std::string, std::string> Application::modelTypeList = set_modelType();
std::set<std::string> Application::fileLanguageList = {"cpp", "python"};

// Function to check if the output contains any of the patterns
int contains_pattern(const std::string &output, const std::vector<std::string> &patterns) {
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (output.find(patterns[i]) != std::string::npos) {
            return i; // Return the index of the matching pattern
        }
    }
    return -1; // No pattern matched
}

std::string Application::get_platform() {
    std::string command = "cat /proc/device-tree/compatible";

    // Execute the command and capture the output
    std::string output = exCommand(command.c_str());

    // Patterns to be checked
    const char *patterns[] = {"stm32mp257", "stm32mp235", "stm32mp255"};
    size_t num_patterns = sizeof(patterns) / sizeof(patterns[0]);

    // Convert patterns to std::vector<std::string>
    std::vector<std::string> pattern_vector(patterns, patterns + num_patterns);

    // Check if the output contains any of the patterns
    int pattern_index = contains_pattern(output, pattern_vector);

    std::string platform;

    // Return platform used
    if (pattern_index != -1) {
        platform = "STM32MP_NPU";
    } else {
        platform = "STM32MP_CPU";
    }

    return platform;
}

// Function to set model type if not defined by user
std::map<std::string, std::string> Application::set_modelType(){
    std::string platform;
    platform = get_platform();
    if (platform=="STM32MP_NPU"){
        std::map<std::string, std::string> modelTypeList = {
            {"nbg", "ovx"},
            {"tflite", "tfl"},
            {"onnx", "ort"},
        };
        return modelTypeList;
    } else {
        std::map<std::string, std::string> modelTypeListCpu = {
            {"tflite", "tfl"},
            {"onnx", "ort"},
        };
        return modelTypeListCpu;
    }
}

std::string Application::get_x_pkg_path(const std::string& pattern, const std::vector<std::string>& directories) {
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

std::string Application::exCommand(const char *cmd)
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

std::string Application::formatPath(const std::filesystem::path &path)
{
    return path.filename();
}

void Application::warningMsg(const std::string &xApp)
{
    std::cout << "\n[WARNING]: " << xApp << ":" << " is not yet installed\n";
    std::cout << "[INFO]: Can be installed using: `x-linux-ai --install " << xApp << "`\n\n";
    exit(1);
}

void Application::printApplicationInfo(const std::string &modelType, const std::string &useCase, const std::string &fileLanguage, std::string &filePath)
{
    std::stringstream ss;

    ss << "\n";
    ss << "\nRunning ...\n";
    ss << "Application: ";
    ss << "stai-mpu-";
    ss << useCase;
    ss << "-";
    ss << fileLanguage;
    ss << "-";
    ss << modelType;
    ss << "\nFile path: ";
    ss << filePath;
    ss << "\n\n";

    std::cout << ss.str() << std::endl;
}

std::set<std::string> Application::getUseCaseList()
{
    // list all the stai-mpu package to find installed applications
    std::string command = "x-linux-ai -l | grep ' stai-mpu'";
    std::string list_of_applications = exCommand(command.c_str());
    // filter the list based on installed applications
    std::regex pattern(R"(\[installed\].*)");
    std::vector<std::string> vect_list_of_applications;

    auto list_begin = std::sregex_iterator(list_of_applications.begin(), list_of_applications.end(), pattern);
    auto list_end = std::sregex_iterator();

    //iterate on each results and remove all the caracters before "stai-mpu" pattern
    for (std::sregex_iterator i = list_begin; i != list_end; ++i) {
        std::smatch match = *i;
        std::string match_str = match.str();
        size_t pos = match_str.find("stai");
        if (pos != std::string::npos) {
            vect_list_of_applications.push_back(match_str.substr(pos));
        }
    }

    //extract unique applications from the list of packages
    std::unordered_set<std::string> unique_applications;
    std::vector<std::string> result;
    for (const auto& str : vect_list_of_applications) {
    size_t pos = str.find_last_of('-');
        if (pos != std::string::npos) {
            pos = str.find_last_of('-', pos - 1);
            if (pos != std::string::npos) {
                pos = str.find_last_of('-', pos - 1);
                if (pos != std::string::npos) {
                    std::string prefix = str.substr(0, pos);
                    if (unique_applications.find(prefix) == unique_applications.end()) {
                        unique_applications.insert(prefix);
                        result.push_back(prefix);
                    }
                }
            }
        }
    }

    // Remove redundancies of substring to keep only the full package name
    std::vector<bool> toRemove(result.size(), false);
    for (size_t i = 0; i < result.size(); ++i) {
        for (size_t j = 0; j < result.size(); ++j) {
            if (i != j && (result[i].find(result[j]) != std::string::npos)) {
                toRemove[j] = true;
                break;
            }
        }
    }

    auto it = std::remove_if(result.begin(), result.end(), [&toRemove, &result](const std::string& str) {
        return toRemove[&str - &result[0]];
    });

    result.erase(it, result.end());

    std::set<std::string> final_list_of_applications;
    // Remove "stai-mpu-" prefix pattern to keep only application name
    std::string prefix = "stai-mpu-";
    size_t prefixLength = prefix.length();

    for (const auto& str : result) {
        if (str.substr(0, prefixLength) == prefix) {
            final_list_of_applications.insert(str.substr(prefixLength));
        }
    }

    return final_list_of_applications;
}

std::string Application::extractFileLanguageFromFileName(const std::string &fileName)
{
    this->_fileLanguage = "python";
    if (formatPath(fileName).find("bin") != std::string::npos)
        _fileLanguage = "cpp";
    return _fileLanguage;
}

std::string Application::extractUseCaseFromFilePath(const std::string &filePath)
{
    for (const std::string& uCase: getUseCaseList()){
        if (filePath.find(uCase) != std::string::npos){
            return uCase;
        }
    }
    return std::string();
}

std::string Application::extractModelTypeFromFilePath(const std::string &filePath)
{
    for (const auto& mdType: modelTypeList){
        if (filePath.find(mdType.first) != std::string::npos){
            return mdType.second;
        }
    }
    return std::string();
}

std::set<std::string> Application::getUseCasePathList(const std::string &dirPath)
{
    for (const auto& useCase : std::filesystem::directory_iterator(dirPath)){
        if (useCase.is_directory()) {
            for (const auto& file : std::filesystem::directory_iterator(useCase.path())){
                /* check if this application contain launch files */
                if (file.path().filename().string().find("launch_") != std::string::npos){
                    this->_useCasePathList.insert(useCase.path());
                    break;
                }
            }
        }
    }

    return _useCasePathList;
}

std::set<std::string> Application::getLaunchFilePathList(const std::set<std::string> &useCasePathList)
{
    for (const auto& useCase : useCasePathList){
        for (const auto& file : std::filesystem::directory_iterator(useCase)){
            if (file.is_regular_file()){
                std::string filename = file.path().filename().string();
                if (filename.rfind("launch_", 0) == 0 && filename.find("_testdata") == std::string::npos){
                    this->_launchFilePathList.insert(file.path());
                }
            }
        }
    }

    return _launchFilePathList;
}

std::set<std::string> Application::getAiApplicationList(const std::string &filePath)
{
    std::set<std::string> appList;
    try {
        std::ifstream file(filePath);
        std::string platform = get_platform();

        if (!file) {
            throw std::runtime_error("Cannot open package list file");
        }

        std::set<std::string> packageList;
        for (std::string line; std::getline(file, line); ) {
            if (line.find("Package: ") != std::string::npos) {
                packageList.insert(line.substr(9));
            }
        }

        std::set<std::string> useCaseList = Application::getUseCaseList();
        for (const auto& mdType : modelTypeList) {
            for (const std::string& uCase : useCaseList) {
                for (const std::string& fLang : fileLanguageList) {
                    if (platform=="STM32MP_NPU"){
                        std::string app = "stai-mpu-" + uCase + "-" + fLang + "-" + mdType.second + "-npu";
                        for (const std::string package: packageList) {
                            if (package.find(app) != std::string::npos) {
                                appList.insert(package);
                                break;
                            }
                        }
                    } else {
                        std::string appcpu = "stai-mpu-" + uCase + "-" + fLang + "-" + mdType.second + "-cpu";
                        for (const std::string package: packageList) {
                            if (package.find(appcpu) != std::string::npos) {
                                appList.insert(package);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    catch(const std::exception& e) {
        std::cerr << "\n[ERROR]: Ensure your board is updated by typing the following command: apt-get update.\n" << e.what() << '\n';
        exit(EXIT_FAILURE);
    }

    return appList;
}

std::set<std::string> Application::getInstalledAiApplicationList(const std::set<std::string>& xAppList, const std::set<std::string>& ostlAppList)
{
    for (const std::string& xApp: xAppList) {
        for (const std::string ostlApp : ostlAppList){
            if (ostlApp.find(xApp) != std::string::npos){
                this->_installedAppList.insert(xApp);
                break;
            }
        }
    }

    return _installedAppList;
}

std::set<std::string> Application::getNotInstalledAiApplicationList(const std::set<std::string>& xAppList, const std::set<std::string>& ostlAppList)
{
    if (ostlAppList.empty())
        return xAppList;

    for (const std::string& xApp: xAppList) {
        for (const std::string ostlApp : ostlAppList){
            if (!ostlApp.find(xApp) != std::string::npos){
                this->_notInstalledAppList.insert(xApp);
            }
        }
    }

    return _notInstalledAppList;
}

void Application::displayUseCaseList(std::set<std::string> uCaseList, std::set<std::string> inAppList)
{
    std::vector<std::string> niUscaseList = {};
    std::cout << "\n";
    for (const std::string& uCase: uCaseList){
        bool found = false;
        for (const std::string inApp: inAppList){
            if (inApp.find(uCase) != std::string::npos) {
                std::cout << "[installed]\t" << uCase << std::endl;
                found = true;
                break;
            }
        }
        if (!found)
            niUscaseList.push_back(uCase);
    }

    for (const std::string& uCase: niUscaseList){
        std::cout << "[not installed]\t" << uCase << std::endl;
    }
    std::cout << "\n";
    exit(0);
}

// All parameters
std::string Application::getBestPerf(const std::set<std::string> &launchFilePathList, const std::string &inputUscase, const std::string &inputModelType, const std::string &inputFileLanguage, const std::set<std::string> &notInAppList, const std::set<std::string> &inAppList)
{
    for (const std::string& filePath: launchFilePathList){
        std::string useCase = extractUseCaseFromFilePath(filePath);
        std::string fileLanguage = extractFileLanguageFromFileName(filePath);
        if(inputUscase == useCase && inputFileLanguage == fileLanguage){
            for (const std::string& app: inAppList){
                if (app.find("stai-mpu-" + inputUscase + "-" + _fileLanguage + "-" + modelTypeList[inputModelType]) != std::string::npos)
                    return filePath + " " + inputModelType;
            }
        }
    }

    for (const std::string app: notInAppList){
        if (app.find(inputUscase) != std::string::npos){
            if(app.find(modelTypeList[inputModelType]) != std::string::npos){
                if (app.find(inputFileLanguage) != std::string::npos)
                    warningMsg(app);
            }
        }
    }
    return "";
}


// File Language
std::string Application::getBestPerf(const std::set<std::string> &launchFilePathList, const std::string &inputUscase, const std::string &inputFileLanguage, const std::set<std::string> &notInAppList, const std::set<std::string> &inAppList)
{

    std::vector<std::string> orderedKeys = {"nbg", "tflite", "onnx"};

    for (const auto& key : orderedKeys) {
        auto it = modelTypeList.find(key);
        if (it != modelTypeList.end()) {
            for (const auto& filePath : launchFilePathList) {
                std::string useCase = extractUseCaseFromFilePath(filePath);
                std::string fileLanguage = extractFileLanguageFromFileName(filePath);
                if (inputUscase == useCase && inputFileLanguage == fileLanguage) {
                    for (const auto& app : inAppList) {
                        if (app.find("stai-mpu-" + inputUscase + "-" + inputFileLanguage + "-" + it->second) != std::string::npos) {
                            return filePath + " " + it->first;
                        }
                    }
                }
            }
        }
    }

    for (const auto& key : orderedKeys) {
        auto it = modelTypeList.find(key);
        if (it != modelTypeList.end()) {
            for (const auto& app : notInAppList) {
                if (app.find(inputUscase) != std::string::npos &&
                    app.find(it->second) != std::string::npos &&
                    app.find(inputFileLanguage) != std::string::npos) {
                        warningMsg(app);
                }
            }
        }
    }
    return "";
}

// Only use case
std::string Application::getBestPerf(const std::set<std::string> &launchFilePathList, const std::string &inputUscase, const std::set<std::string> &notInAppList, const std::set<std::string> &inAppList)
{
    for(const auto& modelType: modelTypeList){
        for(const std::string& fileLanguage: fileLanguageList){
            for (const std::string& filePath : launchFilePathList){
                std::string useCase = extractUseCaseFromFilePath(filePath);
                if (useCase.find(inputUscase) != std::string::npos){
                    std::string _fileLanguage = extractFileLanguageFromFileName(filePath);
                    if (_fileLanguage == fileLanguage){
                        for (const std::string& app: inAppList){
                            if (app.find("stai-mpu-" + inputUscase + "-" + _fileLanguage + "-" + modelType.second) != std::string::npos)
                                return filePath + " " + modelType.first;
                        }
                    }
                }
            }
        }
    }

    for(const auto& modelType: modelTypeList){
        for(const std::string& fileLanguage: fileLanguageList){
            for (const std::string& app: notInAppList){
                if (app.find(inputUscase) != std::string::npos){
                    if(app.find(modelType.second) != std::string::npos && app.find(fileLanguage) != std::string::npos)
                        warningMsg(app);
                }
            }
        }
    }

    return "";
}

void Application::runApplication(const std::string &inputUscase, const std::string &inputModelType, const std::string &inputFileLanguage, const std::set<std::string> &launchFilePathList, const std::set<std::string> &notInAppList, const std::set<std::string> &inAppList)
{
    std::string file = "";
    if (inputUscase != ""  && inputFileLanguage == "" && inputModelType == ""){
        for (const std::string _uCase: getUseCaseList()){
            if (inputUscase == _uCase){

                std::string platform;
                platform = get_platform();
                if (platform=="STM32MP_NPU"){
                    file = getBestPerf(launchFilePathList,inputUscase, "nbg", "python", notInAppList, inAppList);
                } else {
                    file = getBestPerf(launchFilePathList,inputUscase, "tflite", "cpp", notInAppList, inAppList);
                }

                if (file != ""){
                    printApplicationInfo(extractModelTypeFromFilePath(file), extractUseCaseFromFilePath(file), extractFileLanguageFromFileName(file), file);
                    system(file.c_str());
                    exit(0);
                }
                else{
                    file = getBestPerf(launchFilePathList, inputUscase, notInAppList, inAppList);
                    if(file != ""){
                        printApplicationInfo(extractModelTypeFromFilePath(file), extractUseCaseFromFilePath(file), extractFileLanguageFromFileName(file), file);
                        system(file.c_str());
                        exit(0);
                    }
                }
            }
        }
        std::cout << "\n[ERROR]: " << inputUscase << " not found" << "\n\n";
    }

    else if (inputUscase != "" && inputFileLanguage != "" && inputModelType == ""){

        bool uCaseFound = false;
        bool fileLanguageFound = false;
        for (const std::string _uCase: getUseCaseList()){
            if (inputUscase == _uCase){
                uCaseFound = true;
                for (const std::string& _fileLanguage: fileLanguageList){
                    if (inputFileLanguage == _fileLanguage){
                        fileLanguageFound = true;
                        file = getBestPerf(launchFilePathList, inputUscase, inputFileLanguage, notInAppList, inAppList);
                        if (file != ""){
                            printApplicationInfo(extractModelTypeFromFilePath(file), extractUseCaseFromFilePath(file), extractFileLanguageFromFileName(file), file);
                            system(file.c_str());
                            exit(0);
                        }
                    }
                }
            }
        }

        if (!uCaseFound)
            std::cout << "\n[ERROR]: " << inputUscase << " not found" << "\n\n";
        else if (!fileLanguageFound && !uCaseFound)
            std::cout << "\n[ERROR]: " << inputFileLanguage << " is not supported for " << inputUscase << "\n\n";
        else
            std::cout << "\n[ERROR]: Application not found" << "\n\n";
    }

    else if (inputUscase != "" && inputFileLanguage == "" && inputModelType != ""){
        bool uCaseFound = false;
        bool modelTypeFound = false;
        for (const std::string _uCase: getUseCaseList()){
            if (inputUscase == _uCase){
                uCaseFound = true;
                for (const auto& _mdType: modelTypeList){
                    if (_mdType.first == inputModelType){
                        modelTypeFound = true;
                        for (const std::string& _fileLanguage: fileLanguageList){
                            file = getBestPerf(launchFilePathList,inputUscase, inputModelType, _fileLanguage, notInAppList, inAppList);
                            if (file != ""){
                                printApplicationInfo(extractModelTypeFromFilePath(file), extractUseCaseFromFilePath(file), extractFileLanguageFromFileName(file), file);
                                system(file.c_str());
                                exit(0);
                            }
                        }
                    }
                }
            }
        }

        if (!uCaseFound)
            std::cout << "\n[ERROR]: " << inputUscase << " not found" << "\n\n";
        else if (!modelTypeFound && uCaseFound){
            std::set<std::string> availableMlodelTypes = {};
            std::cout << "\n[ERROR]: " << inputModelType << " is not supported for " << inputUscase << "\n\n";
            std::cout << "[INFO]:  Here is the available model type(s) for this particular use case" << "\n\n";
            for (const auto& mdtype: modelTypeList){
                for (const auto& inApp: inAppList){
                    if (inApp.find(inputUscase) != std::string::npos){
                        if (inApp.find(mdtype.second) != std::string::npos){
                            if (!availableMlodelTypes.count(mdtype.first))
                                availableMlodelTypes.insert(mdtype.first);
                        }
                    }
                }

                for (const auto& notInApp: notInAppList){
                    if (notInApp.find(inputUscase) != std::string::npos){
                        if (notInApp.find(mdtype.second) != std::string::npos){
                            if (!availableMlodelTypes.count(mdtype.first))
                                availableMlodelTypes.insert(mdtype.first);
                        }
                    }
                }
            }

            for (const auto& element: availableMlodelTypes){
                std::cout<< " * " << element << std::endl;
            }

            std::cout << "\n";
        }

        else
            std::cout << "\n[ERROR]: Application not found" << "\n\n";
    }

    else if (inputUscase != "" && inputFileLanguage != "" && inputModelType != ""){
        bool uCaseFound = false;
        bool fileLanguageFound = false;
        bool modelTypeFound = false;
        for (const std::string _uCase: getUseCaseList()){
            if (inputUscase == _uCase){
                uCaseFound = true;
                for (const std::string& _fileLanguage: fileLanguageList){
                    if (inputFileLanguage == _fileLanguage){
                        fileLanguageFound = true;
                        for (const auto& _mdType: modelTypeList){
                            modelTypeFound = true;
                            if (_mdType.first == inputModelType){
                                file = getBestPerf(launchFilePathList,inputUscase, inputModelType, _fileLanguage, notInAppList, inAppList);
                                if (file != ""){
                                    printApplicationInfo(extractModelTypeFromFilePath(file), extractUseCaseFromFilePath(file), extractFileLanguageFromFileName(file), file);
                                    system(file.c_str());
                                    exit(0);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (!uCaseFound)
            std::cout << "\n[ERROR]: " << inputUscase << " not found" << "\n\n";
        else if (!fileLanguageFound && !modelTypeFound)
            std::cout << "\n[ERROR]: " << inputFileLanguage << " is not supported for " << inputModelType << "\n\n";
        else if (!modelTypeFound)
            std::cout << "\n[ERROR]: " << inputModelType << " is not supported for " << inputUscase << "\n\n";
        else
            std::cout << "\n[ERROR]: Application not found" << "\n\n";
    }

    exit(1);
}
