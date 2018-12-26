local inputs = {"pattern", "str"}
local outputs = {"idx", "elem"}

local function func(pattern, str)
  local i = 1
  for k in str:gmatch(pattern) do
    coroutine.yield(i, k)
    i = i + 1
  end
end

return
 inputs,
 outputs,
 "table",
 func 
