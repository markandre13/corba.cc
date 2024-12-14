APP=tests

# MEM=-fsanitize=address -fsanitize=leak

# macOS
ifeq ($(shell uname -s),Darwin)
CXX=/usr/local/opt/llvm/bin/clang++
OS_CFLAGS=\
	-I/usr/local/opt/llvm/include/c++ \
	-I/usr/local/include
OS_LFLAGS=\
	-L/usr/local/opt/llvm/lib/c++ \
	-Wl,-rpath,/usr/local/opt/llvm/lib/c++ \
	-L/usr/local/lib
endif

# debian
ifeq ($(shell uname -s),Linux)
CXX=clang++-19
OS_CFLAGS=
OS_LFLAGS=-luuid
endif

CFLAGS=-std=c++23 $(MEM) -O0 -g \
	$(OS_CFLAGS) \
	-Wall -Wextra -Wno-deprecated-anon-enum-enum-conversion \
	-Werror=return-type -Werror=shadow \
	-Wno-unused-parameter -Wno-module-import-in-extern-c \
	-I../upstream/kaffeeklatsch/src \
	-I../src
# -D_COROUTINE_DEBUG=1

LFLAGS=$(OS_LFLAGS) $(MEM)

LIB=-lev -lwslay -lnettle -lomniORB4 -lomniDynamic4 -lomnithread

IDL_SRC=basic/basic.idl
IDL_CXX=$(patsubst %.idl,%SK.cc,$(IDL_SRC)) 
IDL_HXX=$(patsubst %.idl,%.hh,$(IDL_SRC))

APP_SRC=$(IDL_CXX) basic/basic.spec.cc main.cc

SRC = $(APP_SRC) \
	  ../upstream/kaffeeklatsch/src/kaffeeklatsch.cc

OBJ = $(SRC:.cc=.o) 
#$(patsubst %.idl,%SK.o,$(IDL_SRC)) 
# $(WSLAY:.c=.o)

.SUFFIXES: .idl .cc .c .h .hh .o

all: $(APP)

depend:
	makedepend -I../src -Y $(SRC)

run:
	DYLD_LIBRARY_PATH=$(MEDIAPIPE_CPP_DIR)/library ./$(APP) --reporter=info --report-timing --only=foo

valgrind:
	valgrind --track-origins=yes --tool=memcheck --leak-check=full --num-callers=20 ./$(APP)

clean:
	rm -f $(OBJ) $(IDL_CXX) $(IDL_HXX) */generated/* $(APP)

$(APP): $(OBJ) 
	@echo "linking..."
	$(CXX) $(LFLAGS) $(LIB) $(OBJ) -o $(APP)

.cc.o:
	@echo compiling $*.cc ...
	$(CXX) $(CFLAGS) -c -o $*.o $*.cc

.c.o:
	@echo compiling $*.c ...
	$(CC) $(MEM) -g -O2 -DHAVE_CONFIG_H -I../upstream/wslay/lib/ -I../upstream/wslay/lib/includes -c -o $*.o $*.c

%.hh %SK.cc : %.idl
	omniidl -bcxx -C$(dir $<) $*.idl

# DO NOT DELETE

basic/basic.spec.o: basic/basic.hh
basic/xtra.o: ../src/corba/ior.hh ../src/corba/object.hh ../src/corba/blob.hh
basic/xtra.o: ../src/corba/net/tcp/protocol.hh ../src/corba/net/protocol.hh
basic/xtra.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
basic/xtra.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
basic/xtra.o: ../src/corba/net/util/socket.hh
basic/generated/basic.o: ../src/corba/corba.hh ../src/corba/orb.hh
basic/generated/basic.o: ../src/corba/coroutine.hh
basic/generated/basic.o: ../upstream/async.cc/src/async.hh
basic/generated/basic.o: ../src/corba/giop.hh ../src/corba/cdr.hh
basic/generated/basic.o: ../src/corba/blob.hh ../src/corba/net/connection.hh
basic/generated/basic.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
basic/generated/basic.o: ../src/corba/net/util/socket.hh
basic/generated/basic.o: ../src/corba/skeleton.hh ../src/corba/object.hh
basic/generated/basic.o: ../src/corba/stub.hh ../src/corba/ior.hh
basic/generated/basic.o: ../src/corba/exception.hh ../src/corba/orb.hh
basic/generated/basic.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
basic/generated/basic.o: basic/generated/basic.hh
basic/generated/basic.o: basic/generated/basic_skel.hh
basic/generated/basic.o: basic/generated/basic_stub.hh
../src/corba/orb.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
../src/corba/orb.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
../src/corba/orb.o: ../src/corba/cdr.hh ../src/corba/blob.hh
../src/corba/orb.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
../src/corba/orb.o: ../src/corba/coroutine.hh ../src/corba/net/util/socket.hh
../src/corba/orb.o: ../src/corba/corba.hh ../src/corba/skeleton.hh
../src/corba/orb.o: ../src/corba/object.hh ../src/corba/stub.hh
../src/corba/orb.o: ../src/corba/ior.hh ../src/corba/exception.hh
../src/corba/orb.o: ../src/corba/hexdump.hh ../src/corba/naming.hh
../src/corba/orb.o: ../src/corba/net/protocol.hh
../src/corba/orb.o: ../src/corba/net/connection.hh ../src/corba/url.hh
../src/corba/ior.o: ../src/corba/ior.hh ../src/corba/object.hh
../src/corba/ior.o: ../src/corba/blob.hh ../src/corba/giop.hh
../src/corba/ior.o: ../src/corba/cdr.hh
../src/corba/skeleton.o: ../src/corba/skeleton.hh ../src/corba/object.hh
../src/corba/skeleton.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/skeleton.o: ../upstream/async.cc/src/async.hh
../src/corba/skeleton.o: ../src/corba/orb.hh ../src/corba/giop.hh
../src/corba/skeleton.o: ../src/corba/cdr.hh ../src/corba/net/connection.hh
../src/corba/skeleton.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/skeleton.o: ../src/corba/net/util/socket.hh
../src/corba/stub.o: ../src/corba/stub.hh ../src/corba/object.hh
../src/corba/stub.o: ../src/corba/blob.hh ../src/corba/orb.hh
../src/corba/stub.o: ../src/corba/coroutine.hh
../src/corba/stub.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
../src/corba/stub.o: ../src/corba/cdr.hh ../src/corba/net/connection.hh
../src/corba/stub.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/stub.o: ../src/corba/net/util/socket.hh
../src/corba/stub.o: ../src/corba/net/protocol.hh
../src/corba/stub.o: ../src/corba/net/connection.hh
../src/corba/giop.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/giop.o: ../src/corba/blob.hh ../src/corba/corba.hh
../src/corba/giop.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
../src/corba/giop.o: ../upstream/async.cc/src/async.hh
../src/corba/giop.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
../src/corba/giop.o: ../src/corba/coroutine.hh
../src/corba/giop.o: ../src/corba/net/util/socket.hh ../src/corba/skeleton.hh
../src/corba/giop.o: ../src/corba/object.hh ../src/corba/stub.hh
../src/corba/giop.o: ../src/corba/ior.hh ../src/corba/exception.hh
../src/corba/giop.o: ../src/corba/hexdump.hh ../src/corba/net/protocol.hh
../src/corba/giop.o: ../src/corba/net/connection.hh
../src/corba/cdr.o: ../src/corba/cdr.hh ../src/corba/blob.hh
../src/corba/url.o: ../src/corba/url.hh ../src/corba/cdr.hh
../src/corba/url.o: ../src/corba/blob.hh ../src/corba/giop.hh
../src/corba/url.o: ../src/corba/corba.hh ../src/corba/orb.hh
../src/corba/url.o: ../src/corba/coroutine.hh
../src/corba/url.o: ../upstream/async.cc/src/async.hh
../src/corba/url.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
../src/corba/url.o: ../src/corba/coroutine.hh ../src/corba/net/util/socket.hh
../src/corba/url.o: ../src/corba/skeleton.hh ../src/corba/object.hh
../src/corba/url.o: ../src/corba/stub.hh ../src/corba/ior.hh
../src/corba/url.o: ../src/corba/exception.hh
../src/corba/naming.o: ../src/corba/naming.hh ../src/corba/skeleton.hh
../src/corba/naming.o: ../src/corba/object.hh ../src/corba/blob.hh
../src/corba/naming.o: ../src/corba/coroutine.hh
../src/corba/naming.o: ../upstream/async.cc/src/async.hh ../src/corba/stub.hh
../src/corba/naming.o: ../src/corba/orb.hh ../src/corba/giop.hh
../src/corba/naming.o: ../src/corba/cdr.hh ../src/corba/net/connection.hh
../src/corba/naming.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/naming.o: ../src/corba/net/util/socket.hh
../src/corba/naming.o: ../src/corba/hexdump.hh ../src/corba/ior.hh
../src/corba/net/connection.o: ../src/corba/net/connection.hh
../src/corba/net/connection.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/net/connection.o: ../upstream/async.cc/src/async.hh
../src/corba/net/connection.o: ../src/corba/net/util/socket.hh
../src/corba/net/connection.o: ../src/corba/net/protocol.hh
../src/corba/net/stream2packet.o: ../src/corba/net/stream2packet.hh
../src/corba/net/stream2packet.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/stream2packet.o: ../src/corba/blob.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/tcp/protocol.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/protocol.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/protocol.o: ../src/corba/blob.hh
../src/corba/net/tcp/protocol.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/protocol.o: ../upstream/async.cc/src/async.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/util/socket.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/tcp/connection.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/stream2packet.hh
../src/corba/net/tcp/protocol.o: ../src/corba/orb.hh
../src/corba/net/tcp/protocol.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/protocol.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/tcp/protocol.o: ../src/corba/blob.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/protocol.o: ../src/corba/exception.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/tcp/connection.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/tcp/protocol.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/protocol.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/connection.o: ../src/corba/blob.hh
../src/corba/net/tcp/connection.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/connection.o: ../upstream/async.cc/src/async.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/util/socket.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/stream2packet.hh
../src/corba/net/tcp/connection.o: ../src/corba/orb.hh
../src/corba/net/tcp/connection.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/connection.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/tcp/connection.o: ../src/corba/blob.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/connection.hh
../src/corba/net/util/socket.o: ../src/corba/net/util/socket.hh
../src/corba/net/util/createAcceptKey.o: ../src/corba/net/util/createAcceptKey.hh
../upstream/kaffeeklatsch/src/kaffeeklatsch.o: ../upstream/kaffeeklatsch/src/kaffeeklatsch.hh
