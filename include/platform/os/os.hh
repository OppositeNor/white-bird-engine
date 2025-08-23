/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef __WBE_OS_HH__
#define __WBE_OS_HH__

#ifdef __unix__
#include <sched.h>
#endif

namespace WhiteBirdEngine {

#ifdef __unix__
using PID = pid_t;
#endif

class OS {
public:
    OS() = delete;

    /**
     * @brief Execute a program on a separate process.
     *
     * @todo: Test
     * @param p_background Is the process running in background.
     * @param p_exec_path The path to the process executable. 
     * @param p_argv Argv passed to the process.
     * @param p_envp Environment variables passed to the process.
     * @return The process ID if p_background is true. -1 otherwise.
     */
    static PID execute(bool p_background, const char* p_exec_path, const char* p_argv[], const char* p_envp[]);

    /**
     * @brief Execute a program on a separate process.
     *
     * @param p_background Is the process running in background.
     * @param p_exec_path The path to the process executable. 
     * @param p_argv Argv passed to the process.
     * @return The process ID if p_background is true. -1 otherwise.
     */
    static PID execute(bool p_background, const char* p_exec_path, const char* p_argv[]);

    /**
     * @brief Fork a process.
     *
     * @return The process's pid.
     */
    static PID fork_process();

    /**
     * @brief Wait a process to finish.
     */
    static void wait_process(PID p_pid);
};

}

#endif
