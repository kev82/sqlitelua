.load ./lib/luafunctions

insert into
  luafunctions(name, src)
values
  ('grperrtext', 'return {"x"}, {"x"}, "aggregate", function(x) error("some error text") end'),
  ('grperrtbl', 'return {"x"}, {"x"}, "aggregate", function(x) error({"some text in table"}) end'),
  ('grperrtextfinal', 'return {"x"}, {"x"}, "aggregate", ' ||
   'function(x) local function l() if aggregate_now(x) then error("error text") end end while true do l() x = coroutine.yield() end end'),
  ('grperrtblfinal', 'return {"x"}, {"x"}, "aggregate", ' ||
   'function(x) local function l() if aggregate_now(x) then error({"error text"}) end end while true do l() x = coroutine.yield() end end')
;

create view
  one2ten
as with
  rec(i) as (select
    1 as i
  union all select
    i+1
  from
    rec
  where
    i < 10)
  select * from rec
;

select
  grperrtext(i)
from
  one2ten
;

select
  grperrtextfinal(i)
from
  one2ten
;

select
  grperrtbl(i)
from
  one2ten
;

select
  grperrtblfinal(i)
from
  one2ten
;

