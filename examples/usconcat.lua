local inputs = {"item"}
local outputs = {"joineditem"}

local function func(item)
  if aggregate_now(item) then
    return ""
  end

  local itemset = {}
  while true do
    if aggregate_now(item) then
      break
    end

    if item then 
      itemset[tostring(item)] = true
    end

    item = coroutine.yield()
  end

  local rv = {}
  for k in pairs(itemset) do
    rv[#rv+1] = k
  end
  table.sort(rv)
  return table.concat(rv, ",")
end

return inputs, outputs, "aggregate", func
