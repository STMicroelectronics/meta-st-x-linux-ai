/* Copyright (C) 2019 NXP Semiconductors
* Author: 
* Xiyue Shi xiyue_shi@163.com
* Devin Jiao bin.jiao@nxp.com
* Any questions, please contact with bin.jiao@nxp.com
* 19/06/2019
*
* SPDX-License-Identifier:    Apache-2.0
*
*/
#include <chrono>

#define GET_TIME_IN_US() (std::chrono::time_point<std::chrono::steady_clock>)(std::chrono::steady_clock::now());
#define GET_TIME_DIFF(start, end) (int)(((std::chrono::duration<double>)(end - start)).count() * 1000000.0)
