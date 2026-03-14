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
#ifndef WBE_FILE_APPLICATION_HH
#define WBE_FILE_APPLICATION_HH

#include "utils/defs.hh"

namespace WhiteBirdEngine {

/**
 * @class IApplication
 * @brief IApplication class.
 *
 */
class IApplication {
public:
    WBE_R6_DEFAULT_VIRTUAL(IApplication)

    /**
     * @brief Run the application.
     */
    virtual void run() = 0;

    /**
     * @brief Get the difference between pair of render frames.
     *
     * @return The difference between pair of render frames.
     */
    virtual double get_render_delta() const = 0;

    /**
     * @brief Get the difference between pair of logic frames.
     *
     * @return The difference between pair of logic frames.
     */
    virtual double get_logic_delta() const = 0;

    /**
     * @brief The function called every render frame.
     *
     * @param p_delta The delta time since last render frame.
     */
    virtual void render_update(double p_delta) {};

    /**
     * @brief The function called every logic frame.
     *
     * @param p_delta The delta time since last render frame.
     */
    virtual void logic_update(double p_delta) {};

protected:

};

} // namespace WhiteBirdEngine

#endif
