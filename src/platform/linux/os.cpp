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

#include "platform/os/os.hh"
#include <alloca.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sched.h>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace WhiteBirdEngine {

PID OS::execute(bool p_background, const char* p_exec_path, const char* p_argv[], const char* p_envp[]) {
    pid_t pid = fork_process();
    if (pid == 0) {
        int ret_val;
        if ((ret_val = execve(p_exec_path, (char* const*)p_argv, (char* const*)p_envp)) < 0) {
            printf("Failed to run program: %s.\n", p_exec_path);
            exit(EXIT_FAILURE);
        }
    }
    if (!p_background) {
        wait_process(pid);
        return -1;
    }
    return pid;
}

PID OS::execute(bool p_background, const char* p_exec_path, const char* p_argv[]) {
    return execute(p_background, p_exec_path, p_argv, nullptr);
}

PID OS::fork_process() {
    pid_t pid;
    if ((pid = fork()) < 0) {
        throw std::runtime_error("Failed to fork a process: " + std::string(strerror(errno)));
    }
    return pid;
}

void OS::wait_process(PID p_pid, bool p_silent) {
    int status;
    if (waitpid(p_pid, &status, 0) < 0) {
        throw std::runtime_error("Failed to wait for process pid: " + std::to_string(p_pid) + " to finish: " + std::string(strerror(errno)));
    }
    if (p_silent) {
        return;
    }
    if (WIFEXITED(status)) {
        printf("Process with pid: %d terminated normally with exit status: %d.\n", p_pid, WEXITSTATUS(status));
    }
    else {
        printf("Process with pid: %d terminated abnormally with exit status: %d.\n", p_pid, WEXITSTATUS(status));
    }
}

void OS::wait_all(bool p_silent) {
    PID pid;
    int status;
    while ((pid = waitpid(-1, &status, 0)) > 0) {
        if (p_silent) {
            continue;
        }
        if (WIFEXITED(status)) {
            printf("Process with pid: %d terminated normally with exit status: %d.\n", pid, WEXITSTATUS(status));
        }
        else {
            printf("Process with pid: %d terminated abnormally with exit status: %d.\n", pid, WEXITSTATUS(status));
        }
    }
    if (errno != ECHILD) {
        throw std::runtime_error("Failed to wait for all process to finish: " + std::string(strerror(errno)));
    }
}

}

