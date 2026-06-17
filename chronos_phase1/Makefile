# Tell Java where to find the C library when running
LIB_PATH := -Djava.library.path=.

all: compile_java compile_c run

compile_java:
	javac -h . ChronosClient.java

compile_c:
	# Linux uses .so, macOS uses .dylib. We try Linux first, fallback to Mac.
	gcc -shared -fPIC -o libchronoscore.so ChronosCore.c -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux 2>/dev/null || \
	gcc -shared -fPIC -o libchronoscore.dylib ChronosCore.c -I${JAVA_HOME}/include -I${JAVA_HOME}/include/darwin

run:
	java $(LIB_PATH) ChronosClient

clean:
	rm -f *.class *.so *.dylib data.chronos ChronosCore.h