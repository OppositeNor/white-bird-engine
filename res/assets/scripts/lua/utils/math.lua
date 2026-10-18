-- Copyright 2025 OppositeNor
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

-- Integer math helpers.

local exports = {}

---Divide two non-negative integers, rounding up.
---@param p_value integer
---@param p_divisor integer
---@return integer
function exports.ceil_div(p_value, p_divisor)
    return (p_value + p_divisor - 1) // p_divisor
end

---Clamp p_value to [p_min, p_max].
---@param p_value number
---@param p_min number
---@param p_max number
---@return number
function exports.clamp(p_value, p_min, p_max)
    return math.min(math.max(p_value, p_min), p_max)
end

return exports
