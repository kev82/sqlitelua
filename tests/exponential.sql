.load ./lib/luafunctions

insert into
  luafunctions(name, src)
values
  ('exp', cast(readfile('examples/exponentiate.lua') as text))
;

.header on

with
  seq(x) as (select
    -5.0
  union all select
    x+0.5
  from
    seq
  where x < 5)
  select
    x,
    printf('%.10e', exp(x)) exp10sf
  from
    seq
;
