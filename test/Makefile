APP=tests

MEM=-fsanitize=address -fsanitize=leak

CXX=/usr/local/opt/llvm/bin/clang++
CFLAGS=-std=c++23 $(MEM) -O0 -g \
	-mmacosx-version-min=12.0 \
	-Wall -Wextra -Wno-deprecated-anon-enum-enum-conversion \
	-Werror=return-type -Werror=shadow \
	-Wno-unused-parameter -Wno-module-import-in-extern-c \
	-I/usr/local/opt/llvm/include/c++ \
	-I../upstream/kaffeeklatsch/src \
	-I../src
# -D_COROUTINE_DEBUG=1

LDFLAGS=-L/usr/local/opt/llvm/lib/c++ \
	-mmacosx-version-min=12.0 \
	-Wl,-rpath,/usr/local/opt/llvm/lib/c++ \
	-L/usr/local/lib $(MEM)

WSLAG_FLAGS=-I/usr/local/include
LIB=-lev -lwslay -lnettle

APP_SRC=cdr_decoder.spec.cc cdr_encoder.spec.cc giop.spec.cc corba.spec.cc blob.spec.cc \
	  lifecycle.spec.cc \
	  interface/interface.spec.cc interface/interface.cc \
	  net/ws.spec.cc net/tcp.spec.cc \
	  main.cc util.cc fake.cc
IDL_GEN=interface/interface.cc interface/interface.hh interface/interface_skel.hh interface/interface_stub.hh

CORBA_PATH=../src
CORBA_SRC=orb.cc ior.cc skeleton.cc stub.cc giop.cc cdr.cc url.cc \
		naming.cc hexdump.cc \
        net/ws.cc net/tcp.cc net/ws/socket.cc net/ws/createAcceptKey.cc

SRC = $(APP_SRC) \
	  $(patsubst %.cc,$(CORBA_PATH)/corba/%.cc,$(CORBA_SRC)) \
	  ../upstream/kaffeeklatsch/src/kaffeeklatsch.cc
	  
WSLAY=../upstream/wslay/lib/wslay_event.c \
  ../upstream/wslay/lib/wslay_frame.c \
  ../upstream/wslay/lib/wslay_net.c \
  ../upstream/wslay/lib/wslay_queue.c

OBJ = $(SRC:.cc=.o) 
# $(WSLAY:.c=.o)

.SUFFIXES: .idl .cc .c .h .hh .o

all: $(APP)

depend:
	makedepend -I../src -Y $(SRC)

run:
#	scp 192.168.178.105:omni-test3/IOR.txt .
	DYLD_LIBRARY_PATH=$(MEDIAPIPE_CPP_DIR)/library ./$(APP) --reporter=info --report-timing --only=foo

valgrind:
	/usr/local/Cellar/valgrind/HEAD-93191c1/bin/valgrind --track-origins=yes ./$(APP)

clean:
	rm -f $(OBJ) $(IDL_GEN)

$(APP): $(OBJ)
	@echo "linking..."
	$(CXX) $(LDFLAGS) $(LIB) $(OBJ) -o $(APP)

.cc.o:
	@echo compiling $*.cc ...
	$(CXX) $(CFLAGS) $(WSLAG_FLAGS) \
	-c -o $*.o $*.cc

.c.o:
	@echo compiling $*.c ...
	$(CC) $(MEM) -g -O2 -DHAVE_CONFIG_H -I../upstream/wslay/lib/ -I../upstream/wslay/lib/includes -c -o $*.o $*.c

$(IDL_GEN): interface/interface.idl
	bun ../upstream/corba.js/src/idl/idl.ts --cc-all interface/interface.idl

# DO NOT DELETE

cdr_decoder.spec.o: ../src/corba/cdr.hh ../src/corba/blob.hh
cdr_encoder.spec.o: ../src/corba/cdr.hh ../src/corba/blob.hh
giop.spec.o: ../src/corba/giop.hh ../src/corba/cdr.hh ../src/corba/blob.hh
giop.spec.o: util.hh ../src/corba/hexdump.hh ../src/corba/coroutine.hh
giop.spec.o: ../upstream/async.cc/src/async.hh
corba.spec.o: fake.hh ../src/corba/blob.hh ../src/corba/protocol.hh
corba.spec.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
corba.spec.o: ../upstream/async.cc/src/async.hh ../src/corba/corba.hh
corba.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh ../src/corba/cdr.hh
corba.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
corba.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
corba.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
corba.spec.o: ../src/corba/protocol.hh ../src/corba/url.hh util.hh
corba.spec.o: ../src/corba/hexdump.hh ../src/corba/coroutine.hh
blob.spec.o: ../src/corba/blob.hh
interface/interface.spec.o: fake.hh ../src/corba/blob.hh
interface/interface.spec.o: ../src/corba/protocol.hh ../src/corba/blob.hh
interface/interface.spec.o: ../src/corba/coroutine.hh
interface/interface.spec.o: ../upstream/async.cc/src/async.hh util.hh
interface/interface.spec.o: ../src/corba/hexdump.hh ../src/corba/coroutine.hh
interface/interface.spec.o: interface/interface_impl.hh
interface/interface.spec.o: interface/interface_skel.hh ../src/corba/corba.hh
interface/interface.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh
interface/interface.spec.o: ../src/corba/cdr.hh ../src/corba/skeleton.hh
interface/interface.spec.o: ../src/corba/object.hh ../src/corba/stub.hh
interface/interface.spec.o: ../src/corba/ior.hh ../src/corba/exception.hh
interface/interface.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh
interface/interface.spec.o: ../src/corba/coroutine.hh interface/interface.hh
interface/interface.o: ../src/corba/corba.hh ../src/corba/orb.hh
interface/interface.o: ../src/corba/coroutine.hh
interface/interface.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
interface/interface.o: ../src/corba/cdr.hh ../src/corba/blob.hh
interface/interface.o: ../src/corba/skeleton.hh ../src/corba/object.hh
interface/interface.o: ../src/corba/stub.hh ../src/corba/ior.hh
interface/interface.o: ../src/corba/exception.hh ../src/corba/orb.hh
interface/interface.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
interface/interface.o: interface/interface.hh interface/interface_skel.hh
interface/interface.o: interface/interface_stub.hh
net/ws.spec.o: ../src/corba/net/ws.hh ../src/corba/protocol.hh
net/ws.spec.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
net/ws.spec.o: ../upstream/async.cc/src/async.hh interface/interface_impl.hh
net/ws.spec.o: interface/interface_skel.hh ../src/corba/corba.hh
net/ws.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh ../src/corba/cdr.hh
net/ws.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
net/ws.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
net/ws.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
net/ws.spec.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
net/ws.spec.o: interface/interface.hh interface/interface_skel.hh util.hh
net/ws.spec.o: ../src/corba/hexdump.hh ../src/corba/coroutine.hh
net/ws.spec.o: ../src/corba/corba.hh
net/tcp.spec.o: ../src/corba/net/tcp.hh ../src/corba/protocol.hh
net/tcp.spec.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
net/tcp.spec.o: ../upstream/async.cc/src/async.hh interface/interface_impl.hh
net/tcp.spec.o: interface/interface_skel.hh ../src/corba/corba.hh
net/tcp.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh ../src/corba/cdr.hh
net/tcp.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
net/tcp.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
net/tcp.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
net/tcp.spec.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
net/tcp.spec.o: interface/interface.hh interface/interface_skel.hh
net/tcp.spec.o: ../src/corba/corba.hh util.hh ../src/corba/hexdump.hh
net/tcp.spec.o: ../src/corba/coroutine.hh
util.o: util.hh ../src/corba/hexdump.hh ../src/corba/blob.hh
util.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
fake.o: fake.hh ../src/corba/blob.hh ../src/corba/protocol.hh
fake.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
fake.o: ../upstream/async.cc/src/async.hh ../src/corba/orb.hh
fake.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/orb.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
../src/corba/orb.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
../src/corba/orb.o: ../src/corba/cdr.hh ../src/corba/blob.hh
../src/corba/orb.o: ../src/corba/corba.hh ../src/corba/skeleton.hh
../src/corba/orb.o: ../src/corba/object.hh ../src/corba/stub.hh
../src/corba/orb.o: ../src/corba/ior.hh ../src/corba/exception.hh
../src/corba/orb.o: ../src/corba/hexdump.hh ../src/corba/naming.hh
../src/corba/orb.o: ../src/corba/protocol.hh ../src/corba/url.hh
../src/corba/ior.o: ../src/corba/ior.hh ../src/corba/object.hh
../src/corba/ior.o: ../src/corba/blob.hh ../src/corba/giop.hh
../src/corba/ior.o: ../src/corba/cdr.hh
../src/corba/skeleton.o: ../src/corba/skeleton.hh ../src/corba/object.hh
../src/corba/skeleton.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/skeleton.o: ../upstream/async.cc/src/async.hh
../src/corba/skeleton.o: ../src/corba/orb.hh ../src/corba/giop.hh
../src/corba/skeleton.o: ../src/corba/cdr.hh
../src/corba/stub.o: ../src/corba/stub.hh ../src/corba/object.hh
../src/corba/stub.o: ../src/corba/blob.hh ../src/corba/orb.hh
../src/corba/stub.o: ../src/corba/coroutine.hh
../src/corba/stub.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
../src/corba/stub.o: ../src/corba/cdr.hh ../src/corba/protocol.hh
../src/corba/giop.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/giop.o: ../src/corba/blob.hh ../src/corba/corba.hh
../src/corba/giop.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
../src/corba/giop.o: ../upstream/async.cc/src/async.hh
../src/corba/giop.o: ../src/corba/skeleton.hh ../src/corba/object.hh
../src/corba/giop.o: ../src/corba/stub.hh ../src/corba/ior.hh
../src/corba/giop.o: ../src/corba/exception.hh ../src/corba/hexdump.hh
../src/corba/giop.o: ../src/corba/protocol.hh
../src/corba/cdr.o: ../src/corba/cdr.hh ../src/corba/blob.hh
../src/corba/url.o: ../src/corba/url.hh ../src/corba/cdr.hh
../src/corba/url.o: ../src/corba/blob.hh ../src/corba/giop.hh
../src/corba/url.o: ../src/corba/corba.hh ../src/corba/orb.hh
../src/corba/url.o: ../src/corba/coroutine.hh
../src/corba/url.o: ../upstream/async.cc/src/async.hh
../src/corba/url.o: ../src/corba/skeleton.hh ../src/corba/object.hh
../src/corba/url.o: ../src/corba/stub.hh ../src/corba/ior.hh
../src/corba/url.o: ../src/corba/exception.hh
../src/corba/naming.o: ../src/corba/naming.hh ../src/corba/skeleton.hh
../src/corba/naming.o: ../src/corba/object.hh ../src/corba/blob.hh
../src/corba/naming.o: ../src/corba/coroutine.hh
../src/corba/naming.o: ../upstream/async.cc/src/async.hh ../src/corba/stub.hh
../src/corba/naming.o: ../src/corba/orb.hh ../src/corba/giop.hh
../src/corba/naming.o: ../src/corba/cdr.hh ../src/corba/hexdump.hh
../src/corba/naming.o: ../src/corba/ior.hh
../src/corba/net/ws.o: ../src/corba/net/ws.hh ../src/corba/protocol.hh
../src/corba/net/ws.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/net/ws.o: ../upstream/async.cc/src/async.hh ../src/corba/orb.hh
../src/corba/net/ws.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/ws.o: ../src/corba/exception.hh
../src/corba/net/ws.o: ../src/corba/net/ws/createAcceptKey.hh
../src/corba/net/ws.o: ../src/corba/net/ws/socket.hh
../src/corba/net/tcp.o: ../src/corba/net/tcp.hh ../src/corba/protocol.hh
../src/corba/net/tcp.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/net/tcp.o: ../upstream/async.cc/src/async.hh
../src/corba/net/tcp.o: ../src/corba/net/ws/socket.hh ../src/corba/orb.hh
../src/corba/net/tcp.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/tcp.o: ../src/corba/hexdump.hh
../src/corba/net/ws/createAcceptKey.o: ../src/corba/net/ws/createAcceptKey.hh
../upstream/kaffeeklatsch/src/kaffeeklatsch.o: ../upstream/kaffeeklatsch/src/kaffeeklatsch.hh
