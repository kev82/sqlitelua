local input = {"x"}
local output = {"ex"}
local function func(x)
  return math.exp(x)
end

return input, output, "simple", func
