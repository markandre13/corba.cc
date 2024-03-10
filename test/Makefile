APP=tests

MEM=-fsanitize=address -fsanitize=leak

CXX=/usr/local/opt/llvm/bin/clang++
CFLAGS=-std=c++23 -fmodules $(MEM) -O0 -g \
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

SRC =  net/ws.spec.cc ../src/corba/net/ws.cc \
	  interface/interface.spec.cc interface/interface.cc \
	  cdr_decoder.spec.cc cdr_encoder.spec.cc giop.spec.cc corba.spec.cc blob.spec.cc \
	  main.cc util.cc fake.cc \
	  ../upstream/kaffeeklatsch/src/kaffeeklatsch.cc \
	  ../src/corba/orb.cc \
	  ../src/corba/ior.cc ../src/corba/skeleton.cc ../src/corba/stub.cc \
	  ../src/corba/giop.cc ../src/corba/cdr.cc \
	  ../src/corba/url.cc ../src/corba/hexdump.cc \
	  ../src/corba/net/ws/socket.cc ../src/corba/net/ws/createAcceptKey.cc

WSLAY=../upstream/wslay/lib/wslay_event.c \
  ../upstream/wslay/lib/wslay_frame.c \
  ../upstream/wslay/lib/wslay_net.c \
  ../upstream/wslay/lib/wslay_queue.c

OBJ = $(SRC:.cc=.o) 
# $(WSLAY:.c=.o)

.SUFFIXES: .idl .cc .c .h .hh .o

all: $(APP)

depend:
	makedepend -I. -Y $(SRC)

run:
	DYLD_LIBRARY_PATH=$(MEDIAPIPE_CPP_DIR)/library ./$(APP) --reporter=info --report-timing --only=foo

valgrind:
	/usr/local/Cellar/valgrind/HEAD-93191c1/bin/valgrind --track-origins=yes ./$(APP)

clean:
	rm -f $(OBJ)

$(APP): $(OBJ)
	@echo "linking..."
	$(CXX) $(LDFLAGS) $(PROTOBUF_LDFLAGS) $(MEDIAPIPE_CPP_LDFLAGS) $(LIB) $(OBJ) -o $(APP)

.cc.o:
	@echo compiling $*.cc ...
	$(CXX) $(PROTOBUF_FLAGS) $(CFLAGS) $(WSLAG_FLAGS) $(OPENCV_FLAGS) \
	-c -o $*.o $*.cc

.c.o:
	@echo compiling $*.c ...
	$(CC) $(MEM) -g -O2 -DHAVE_CONFIG_H -I../upstream/wslay/lib/ -I../upstream/wslay/lib/includes -c -o $*.o $*.c

corba/interface/interface.hh: corba/interface/interface.idl
	bun ../upstream/corba.js/src/idl/idl.ts --cc-all corba/interface/interface.idl

makehuman.hh: makehuman.idl
	bun ../upstream/corba.js/src/idl/idl.ts --cc-all makehuman.idl

# DO NOT DELETE

net/ws.spec.o: ../src/corba/net/ws.hh ../src/corba/protocol.hh
net/ws.spec.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
net/ws.spec.o: ../upstream/async.cc/src/async.hh interface/interface_impl.hh
net/ws.spec.o: interface/interface_skel.hh interface/interface.hh
net/ws.spec.o: interface/interface_skel.hh util.hh ../src/corba/hexdump.hh
net/ws.spec.o: ../src/corba/coroutine.hh ../src/corba/corba.hh
net/ws.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh ../src/corba/cdr.hh
net/ws.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
net/ws.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
net/ws.spec.o: ../src/corba/exception.hh
../src/corba/net/ws.o: ../src/corba/net/ws.hh ../src/corba/protocol.hh
../src/corba/net/ws.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/net/ws.o: ../upstream/async.cc/src/async.hh ../src/corba/orb.hh
../src/corba/net/ws.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/ws.o: ../src/corba/net/ws/createAcceptKey.hh
../src/corba/net/ws.o: ../src/corba/net/ws/socket.hh
../src/corba/net/ws.o: ../../../upstream/wslay/lib/wslay_event.h
../src/corba/net/ws.o: ../../../upstream/wslay/lib/wslay_queue.h
interface/interface.spec.o: interface/interface.hh fake.hh util.hh
interface/interface.spec.o: ../src/corba/hexdump.hh ../src/corba/blob.hh
interface/interface.spec.o: ../src/corba/coroutine.hh
interface/interface.spec.o: ../upstream/async.cc/src/async.hh
interface/interface.spec.o: interface/interface_skel.hh
interface/interface.spec.o: interface/interface_stub.hh
interface/interface.o: interface/interface.hh interface/interface_skel.hh
interface/interface.o: interface/interface_stub.hh
cdr_decoder.spec.o: ../src/corba/cdr.hh ../src/corba/blob.hh
cdr_encoder.spec.o: ../src/corba/cdr.hh ../src/corba/blob.hh
giop.spec.o: ../src/corba/giop.hh ../src/corba/cdr.hh ../src/corba/blob.hh
giop.spec.o: util.hh ../src/corba/hexdump.hh ../src/corba/coroutine.hh
giop.spec.o: ../upstream/async.cc/src/async.hh
corba.spec.o: fake.hh ../src/corba/corba.hh ../src/corba/orb.hh
corba.spec.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
corba.spec.o: ../src/corba/giop.hh ../src/corba/cdr.hh ../src/corba/blob.hh
corba.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
corba.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
corba.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
corba.spec.o: ../src/corba/protocol.hh ../src/corba/url.hh util.hh
corba.spec.o: ../src/corba/hexdump.hh ../src/corba/coroutine.hh
blob.spec.o: ../src/corba/blob.hh
util.o: util.hh ../src/corba/hexdump.hh ../src/corba/blob.hh
util.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
fake.o: fake.hh
../upstream/kaffeeklatsch/src/kaffeeklatsch.o: ../upstream/kaffeeklatsch/src/kaffeeklatsch.hh
../src/corba/orb.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
../src/corba/orb.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
../src/corba/orb.o: ../src/corba/cdr.hh ../src/corba/blob.hh
../src/corba/orb.o: ../src/corba/corba.hh ../src/corba/skeleton.hh
../src/corba/orb.o: ../src/corba/object.hh ../src/corba/stub.hh
../src/corba/orb.o: ../src/corba/ior.hh ../src/corba/exception.hh
../src/corba/orb.o: ../src/corba/hexdump.hh ../src/corba/protocol.hh
../src/corba/orb.o: ../src/corba/url.hh
../src/corba/ior.o: ../src/corba/ior.hh ../src/corba/object.hh
../src/corba/ior.o: ../src/corba/blob.hh ../src/corba/giop.hh
../src/corba/ior.o: ../src/corba/cdr.hh
../src/corba/skeleton.o: ../src/corba/skeleton.hh ../src/corba/object.hh
../src/corba/skeleton.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
../src/corba/skeleton.o: ../upstream/async.cc/src/async.hh
../src/corba/skeleton.o: ../src/corba/orb.hh ../src/corba/giop.hh
../src/corba/skeleton.o: ../src/corba/cdr.hh
../src/corba/stub.o: ../src/corba/stub.hh ../src/corba/object.hh
../src/corba/stub.o: ../src/corba/blob.hh
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
../src/corba/net/ws/createAcceptKey.o: ../src/corba/net/ws/createAcceptKey.hh
