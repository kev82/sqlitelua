.load ./lib/luafunctions

--Create some functions whose name and sourcecode lengths are different
insert into
  luafunctions(name, src)
values
  ('one', 'return {"arg"}, {"rv"}, "simple", function(arg) return 1 end'),
  ('identity', 'return {"arg"}, {"rv"}, "simple", function(arg) return arg end'),
  ('square', 'return {"arg"}, {"rv"}, "simple", function(arg) return arg*arg end')
;

.header on

--rank the functions by length of sourcecode
select
  lf1.name as func,
  count(lf2.name) as srclengthrank
from
  luafunctions as lf1
  join luafunctions as lf2
    on length(lf1.src) >= length(lf2.src)
group by
  lf1.name
order by
  count(lf2.name) asc
;

