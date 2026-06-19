
JAVA_HOME := $(shell dirname $(shell dirname $(shell readlink -f $(shell which javac))))
LIB_PATH := -Djava.library.path=.

all: compile_java compile_c

compile_java:
	javac -h . ChronosClient.java ChronosServer.java

compile_c:
    gcc -shared -fPIC \
        -o libchronoscore.so \
        ChronosCore.c \
        -I$(JAVA_HOME)/include \
        -I$(JAVA_HOME)/include/linux \
        -lpthread

server:
	java --enable-native-access=ALL-UNNAMED $(LIB_PATH) ChronosServer

clean:
	rm -f *.class *.h *.so *.dylib data.chronos