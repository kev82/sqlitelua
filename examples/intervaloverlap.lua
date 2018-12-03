local inputs = {"start1", "end1", "start2", "end2"}
local outputs = {"overlaps"}

local function inside(x, a, b)
  return x >= a and x <= b
end

local function func(s1, e1, s2, e2)
  return inside(s1, s2, e2)
   or inside(e1, s2, e2)
   or inside(s2, s1, e1)
   or inside(e2, s1, e1)
end

return inputs, outputs, "simple", func
  
