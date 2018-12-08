.load ./lib/luafunctions

insert into
  luafunctions(name, src)
values
  ('intoverlap', cast(readfile('examples/intervaloverlap.lua') as text))
;

create table intervals(
  left real not null,
  right real not null
)
;

create trigger
  dropoverlappingintervals
before insert on
  intervals
begin
  select
    raise(ignore)
  from
    intervals
  where
    intoverlap(left, right, NEW.left, NEW.right)
  ;
end
;

insert into
  intervals
values
  (1,2),
  (2,3),
  (3,4)
;

.header on

select * from intervals;
  
delete from intervals;

insert into
  intervals
values
  (2,3),
  (1,2),
  (3,4)
;

select * from intervals;
