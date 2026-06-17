# Automatically detect JAVA_HOME from javac
JAVA_HOME := $(shell dirname $(shell dirname $(shell readlink -f $(shell which javac))))

# Tell Java where to find the native library
LIB_PATH := -Djava.library.path=.

all: compile_java compile_c run

compile_java:
	javac -h . ChronosClient.java

compile_c:
	gcc -shared -fPIC \
		-o libchronoscore.so \
		ChronosCore.c \
		-I$(JAVA_HOME)/include \
		-I$(JAVA_HOME)/include/linux

run:
    java --enable-native-access=ALL-UNNAMED $(LIB_PATH) ChronosClient

clean:
	rm -f *.class *.h *.so *.dylib data.chronos