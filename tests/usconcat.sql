.load ./lib/luafunctions

insert into
  luafunctions(name, src)
values
  ('usconcat', cast(readfile('examples/usconcat.lua') as text))
;

create table presents(
  day text,
  present text
)
;

--The presents upto the 4th day of christmas in a random order
insert into
  presents
values
  ('4th', 'calling bird'),
  ('2nd', 'partridge in a pear tree'),
  ('3rd', 'partridge in a pear tree'),
  ('4th', 'calling bird'),
  ('3rd', 'turtle dove'),
  ('4th', 'calling bird'),
  ('4th', 'french hen'),
  ('3rd', 'french hen'),
  ('2nd', 'turtle dove'),
  ('4th', 'partridge in a pear tree'),
  ('4th', 'french hen'),
  ('3rd', 'turtle dove'),
  ('3rd', 'french hen'),
  ('4th', 'turtle dove'),
  ('1st', 'partridge in a pear tree'),
  ('3rd', 'french hen'),
  ('2nd', 'turtle dove'),
  ('4th', 'french hen'),
  ('4th', 'calling bird'),
  ('4th', 'turtle dove')
;

.header on

select
  day,
  usconcat(present) as presentlist
from
  presents
group by
  day
order by
  day asc
;
