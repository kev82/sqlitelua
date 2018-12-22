all : lib/luafunctions.so bin/sqlite3.26

tests : runtest/selfjoin \
        runtest/exponential \
        runtest/intervaloverlap \
        runtest/usconcat \
        runtest/aggerr

clean :
	rm -rf deps
	rm -rf include
	rm -rf lib
	rm -rf bin

# *** Downloading dependencies ***
.depcache/sqlite-amalgamation-3260000.zip :
	mkdir -p .depcache
	wget -O $@ http://www.sqlite.org/2018/sqlite-amalgamation-3260000.zip

.depcache/lua-5.3.5.tar.gz :
	mkdir -p .depcache
	wget -O $@ http://www.lua.org/ftp/lua-5.3.5.tar.gz

# *** Extracting dependencies ***
deps/sqlite-amalgamation-3260000/.exists : .depcache/sqlite-amalgamation-3260000.zip
	mkdir -p deps
	rm -rf `dirname $@`
	unzip $< -d deps
	touch $@

deps/lua-5.3.5/.exists : .depcache/lua-5.3.5.tar.gz
	mkdir -p deps
	rm -rf `dirname $@`
	tar xvzf $< -C deps
	touch $@

# *** include directory ***
include/.sqlite.exists : deps/sqlite-amalgamation-3260000/.exists
	mkdir -p include
	cd include && ln -s ../deps/sqlite-amalgamation-3260000/sqlite3.h
	cd include && ln -s ../deps/sqlite-amalgamation-3260000/sqlite3ext.h
	touch $@

include/.lua.exists : deps/lua-5.3.5/.exists
	mkdir -p include
	cd include && ln -s ../deps/lua-5.3.5/src/lua.h
	cd include && ln -s ../deps/lua-5.3.5/src/lualib.h
	cd include && ln -s ../deps/lua-5.3.5/src/lauxlib.h
	cd include && ln -s ../deps/lua-5.3.5/src/luaconf.h
	touch $@

# *** lib dirctory ***
lib/liblua.a : deps/lua-5.3.5/.exists
	cd deps/lua-5.3.5 && make MYCFLAGS=-fPIC linux
	mkdir -p lib
	cd lib && ln -s ../deps/lua-5.3.5/src/liblua.a

oursrc=src/main.c src/functable.c src/registersimple.c src/registeraggregate.c src/rclua.c

lib/luafunctions.so : $(oursrc) include/.sqlite.exists include/.lua.exists lib/liblua.a
	gcc -std=c99 -g -shared -fPIC -Iinclude $(oursrc) lib/liblua.a -o $@ -lm -ldl

# *** bin directory ***
bin/sqlite3.26 : deps/sqlite-amalgamation-3260000/.exists
	mkdir -p bin
	gcc -O2 -o $@ \
   -DSQLITE_THREADSAFE=0 -DSQLITE_ENABLE_JSON1 -DSQLITE_ENABLE_EXPLAIN_COMMENTS -DHAVE_USLEEP -DHAVE_READLINE \
   deps/sqlite-amalgamation-3260000/shell.c deps/sqlite-amalgamation-3260000/sqlite3.c \
   -ldl -lreadline

# *** running tests ***
runtest/% : tests/%.sql tests/%.txt all
	cat tests/$(notdir $@).sql | bin/sqlite3.26 2>&1 | diff /dev/stdin tests/$(notdir $@).txt
