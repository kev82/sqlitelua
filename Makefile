luafunctions.so : src/main.c src/executelua.c src/functable.c src/registersimple.c src/registeraggregate.c
	gcc -g -shared -fPIC -Iinclude src/main.c src/executelua.c src/functable.c src/registersimple.c src/registeraggregate.c lib/liblua.a -o luafunctions.so -lm -ldl
