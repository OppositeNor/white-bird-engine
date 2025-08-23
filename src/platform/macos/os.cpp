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

namespace WhiteBirdEngine {

PID OS::execute(bool p_background, const char* p_exec_path, const char* p_argv[], const char* p_envp[]) {
    // TODO
    return -1;
}

PID OS::execute(bool p_background, const char* p_exec_path, const char* p_argv[]) {
    // TODO
    return -1;
}

PID OS::fork_process() {
    // TODO
    return -1;
}

void OS::wait_process(PID p_pid) {
    // TODO
}

}

