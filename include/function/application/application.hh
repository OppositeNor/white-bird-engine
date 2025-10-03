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
#ifndef __WBE_APPLICATION_HH__
#define __WBE_APPLICATION_HH__

namespace WhiteBirdEngine {

/**
 * @class Application
 * @brief Application class.
 *
 */
class Application {
public:
    Application() = default;
    virtual ~Application() {}

    /**
     * @brief Function that ticks the rendering frames.
     *
     * @param p_delta The difference of time of the last two calls for this function.
     */
    virtual void render_tick(double p_delta) = 0;

    /**
     * @brief Function that ticks the logic frames.
     *
     * @param p_delta The difference of time of the last two calls for this function.
     */
    virtual void logic_tick(double p_delta) = 0;

};

}

#endif
