.load ./lib/luafunctions

insert into
  luafunctions(name, src)
values
  ('gmatch', cast(readfile('examples/gmatch.lua') as text))
;

create table test(
  data text
);

insert into
  test
values
  ('1;2;3'),
  ('a;b;c'),
  ('2,3,5'),
  ('b,d,f')
;

.header on

select distinct
  t1.data,
  t2.data
from
  test as t1
  join test as t2
    on t1.data < t2.data
  join gmatch('[^;,]+', t1.data) as g1
  join gmatch('[^;,]+', t2.data) as g2
where
  g1.elem = g2.elem
order by
  t1.data,
  t2.data
;
  


