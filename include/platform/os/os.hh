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

#include <bitset>
#include <cstddef>
#ifdef __unix__
#include <sys/types.h>
#include <sched.h>
#endif

namespace WhiteBirdEngine {

#ifdef __unix__
using PID = pid_t;
using FileDescrip = int;
#endif

/**
 * @class OS
 * @brief Interface to operating system utilities.
 *
 */
class OS {
public:
    OS() = delete;

    /**
     * @brief Memory mapping protocols control bits.
     */
    enum class MMapProtBit {
        // Page may be read.
        READ = 0,
        // Page may be written.
        WRITE,
        // Page may be executed.
        EXEC,
        // Used for tracking total protocols.
        TOTAL_PROT
    };

    using MMapProt = std::bitset<(int)MMapProtBit::TOTAL_PROT>;

    /**
     * @brief Mapping types.
     */
    enum class MMapFlagBit {
        // Shared.
        SHARED = 0,
        // Private.
        PRIVATE,
        // Map to an anonymous file.
        ANON,
        // Used for tracking total types.
        TOTAL_MMAP_FLAGS
    };

    enum class FileOpenFlagBit {
        // Read
        READ = 0,
        // Write
        WRITE,
        // Used for tracking total flags.
        TOTAL_FILE_OPEN_FLAGS
    };

    using MMapFlags = std::bitset<(int)MMapFlagBit::TOTAL_MMAP_FLAGS>;
    using FileOpenFlags = std::bitset<(int)FileOpenFlagBit::TOTAL_FILE_OPEN_FLAGS>;

    /**
     * @brief Execute a program on a separate process. The execution can be a separate
     * program, or a script starting with a '#!' notion indicating the program to run it.
     *
     * @todo: Test
     * @param p_background Is the process running in background. If set to true, the process's
     * resource should be released by calling wait_process or wait_all.
     * @param p_exec_path The path to the process executable. 
     * @param p_argv Argv passed to the process.
     * @param p_envp Environment variables passed to the process.
     * @return The process ID if p_background is true. -1 otherwise.
     */
    static PID execute(bool p_background, const char* p_exec_path, const char* p_argv[], const char* p_envp[]);

    /**
     * @brief Execute a program on a separate process.
     *
     * @param p_background Is the process running in background. If set to true, the process's
     * resource should be released by calling wait_process or wait_all.
     * @param p_exec_path The path to the process executable. 
     * @param p_argv Argv passed to the process.
     * @return The process ID if p_background is true. -1 otherwise.
     */
    static PID execute(bool p_background, const char* p_exec_path, const char* p_argv[]);

    /**
     * @brief Fork a process. For every forked process, the process should be released
     * by calling wait_process or wait_all.
     * and keeps consuming resources.
     *
     * @return The process's pid.
     */
    static PID fork_process();

    /**
     * @brief Wait for a process to finish. This will also release the process resources
     * after called.
     *
     * @param p_pid The process to wait.
     * @param p_silent Should mute the output messages.
     */
    static void wait_process(PID p_pid, bool p_silent = true);

    /**
     * @brief Wait for all processes to finish. This will release all the process resources,
     * and is sugestted to be called every time exiting this program.
     *
     * @param p_silent Should mute the output messages.
     */
    static void wait_all(bool p_silent = true);

    /**
     * @brief Memory mapping.
     *
     * @param p_start The start of the virtual memory to map. Could set to nullptr if not specified.
     * @param p_length The length of the memory to be mapped in bytes.
     * @param p_prot The protocol of the memory mapping.
     * @param p_flags The flags of the memory mapping.
     * @param p_fd The file descriptor referencing to the file to map to.
     * @param p_offset The offset of the file that this memory is going to mapt to.
     */
    static void* memory_map(void* p_start, size_t p_length, MMapProt p_prot, MMapFlags p_flags, FileDescrip p_fd, off_t p_offset);

    /**
     * @brief Open file and get a file description.
     *
     * @param p_path The path to the file.
     * @param p_open_flags The flags of the file to be opened.
     * @return The file desciption.
     */
    static FileDescrip open_file(const char* p_path, FileOpenFlags p_open_flags);

    /**
     * @brief Close a file.
     *
     * @param p_fd The file descripter of the file to be closed.
     */
    static void close_file(FileDescrip p_fd);

    /**
     * @brief Unmap an memory.
     *
     * @param p_start The memory that was previously mapped.
     * @param p_length The length of the memory to be unmapped.
     */
    static void memory_unmap(void* p_start, size_t p_length);
};

}

#endif
