#!/usr/bin/python3
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import subprocess
import glob

""" Define global functions """

def get_OpenSTLinux_version():
    command = "cat /etc/apt/sources.*/packages.* | grep -oP '(?<=packages.openstlinux.st.com/)\d+\.\d+'"
    version = subprocess.run(command,shell=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    if not version.returncode == 0:
        print("Cannot get OpenSTLinux version !")
        return 1
    return version.stdout.strip()

def get_machine():
    command = "uname -n | head -c 8 | awk '{print toupper($0)}'"
    machine = subprocess.run(command,shell=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    if not machine.returncode == 0:
        print("Cannot get MACHINE name !")
        return 1
    return machine.stdout.strip()

def get_url(machine,version):
    if machine == "STM32MP1":
        return f"http://extra.packages.openstlinux.st.com/AI/{version}/pool/config/a/apt-openstlinux-ai/apt-openstlinux-ai_1.0_armhf.deb"
    return f"http://extra.packages.openstlinux.st.com/alpha/stm32mp2/AI/{version}/pool/config/a/apt-openstlinux-ai/apt-openstlinux-ai_1.0_arm64.deb"

def is_package_installed(pkg_name):
    status = subprocess.run(['dpkg','-s', pkg_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return status.returncode == 0

def download_package(url, path):
    download_result = subprocess.run(['wget', '-P', path, url])
    if download_result.returncode != 0:
        print(f"wget -P {path} {url} has been failed.")
        exit(1)

def install_package(path, package_name):
    package_files = glob.glob(f'{path}/{package_name}_*.deb')
    package_file = package_files[0]
    install_result = subprocess.run(['apt-get', 'install', '-y', package_file])
    if install_result.returncode != 0:
        print(f"Error installing package: {install_result.stderr}")
        return 1

    update_result = subprocess.run(['apt-get', 'update'])
    if update_result.returncode != 0:
        print(f"Error updating package lists: {update_result.stderr}")
        return 1
    return 0

def main():

    """ Define global variables """
    pkg_name = "apt-openstlinux-ai"
    x_linux_ai_pkg_path = "/var/cache/apt/archives"
    ostl_version = get_OpenSTLinux_version()
    machine = get_machine()
    url = get_url(machine,ostl_version)

    if is_package_installed(pkg_name):
        print(f"The package {pkg_name} is already installed.")
    else:
        try:
            download_package(url, x_linux_ai_pkg_path)
            install_package(x_linux_ai_pkg_path,pkg_name)
            print(f"The package {pkg_name} has been installed successfully.")
        except:
            print(f"Cannot download and install {pkg_name}")

if __name__ == "__main__":
    main()
