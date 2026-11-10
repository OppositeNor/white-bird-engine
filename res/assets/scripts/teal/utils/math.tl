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

local record math_utils
    -- Divide two non-negative integers, rounding up.
    ceil_div: function(value: integer, divisor: integer): integer
    -- Clamp value to [min, max].
    clamp: function(value: integer, min: integer, max: integer): integer
end

function math_utils.ceil_div(value: integer, divisor: integer): integer
    return (value + divisor - 1) // divisor
end

function math_utils.clamp(value: integer, min: integer, max: integer): integer
    return math.min(math.max(value, min), max)
end

return math_utils
