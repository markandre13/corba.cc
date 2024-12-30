APP=tests

MEM=-fsanitize=address -fsanitize=leak

# brew install llvm libev nettle wslay

# Download and install Command_Line_Tools_for_Xcode_15.3.dmg manually
#   https://developer.apple.com/download/all/?q=Command%20Line%20Tools%20for%20Xcode
# then link to to where brew's llvm expexts it as root:
#
# sudo /bin/bash
# cd /Library/Developer/CommandLineTools/SDKs
# ln -s /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX15.sdk

# macOS
ifeq ($(shell uname -s),Darwin)
LLVM_DIR=$(shell for x in /opt/homebrew/opt/llvm /usr/local/opt/llvm ; do if test -d $$x ; then echo $$x ; break ; fi ; done)
CXX=$(LLVM_DIR)/bin/clang++
OS_CFLAGS=\
	-I$(LLVM_DIR)/include/c++ \
	-I/opt/homebrew/include \
	-I/usr/local/include
OS_LFLAGS=\
	-L$(LLVM_DIR)/lib/c++ \
	-Wl,-rpath,$(LLVM_DIR)/lib/c++ \
	-L/opt/homebrew/lib/ \
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

LIB=-lev -lwslay -lnettle

APP_SRC=cdr_decoder.spec.cc \
	cdr_encoder.spec.cc \
	giop.spec.cc \
	memory.spec.cc \
	lifecycle.spec.cc \
	net/tcp.spec.cc \
	blob.spec.cc \
	corba.spec.cc \
	interface/interface.spec.cc \
	interface/interface.cc \
	fake.cc \
	main.cc util.cc 

#   interface/interface.cc \
# fake.cc
# interface/interface.spec.cc
#		corba.spec.cc
#	  net/ws.spec.cc 
IDL_GEN=interface/interface.cc interface/interface.hh interface/interface_skel.hh interface/interface_stub.hh

CORBA_PATH=../src
CORBA_SRC=orb.cc ior.cc skeleton.cc stub.cc giop.cc cdr.cc url.cc \
	naming.cc hexdump.cc \
	net/connection.cc net/stream2packet.cc \
	net/tcp/protocol.cc net/tcp/connection.cc \
	net/ws/protocol.cc net/ws/connection.cc \
	net/util/socket.cc net/util/createAcceptKey.cc

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
	valgrind --track-origins=yes --tool=memcheck --leak-check=full --num-callers=20 ./$(APP)

clean:
	rm -f $(OBJ) $(IDL_GEN) $(APP)

$(APP): $(OBJ) 
	@echo "linking..."
	$(CXX) $(LFLAGS) $(LIB) $(OBJ) -o $(APP)

.cc.o:
	@echo compiling $*.cc ...
	$(CXX) $(CFLAGS) -c -o $*.o $*.cc

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
memory.spec.o: ../src/corba/exception.hh ../src/corba/giop.hh
memory.spec.o: ../src/corba/cdr.hh ../src/corba/blob.hh
memory.spec.o: ../src/corba/net/stream2packet.hh
memory.spec.o: ../src/corba/net/tcp/connection.hh
memory.spec.o: ../src/corba/net/tcp/protocol.hh ../src/corba/net/protocol.hh
memory.spec.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
memory.spec.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
memory.spec.o: ../src/corba/net/util/socket.hh ../src/corba/net/connection.hh
memory.spec.o: ../src/corba/net/stream2packet.hh
memory.spec.o: ../src/corba/net/tcp/protocol.hh
memory.spec.o: ../src/corba/net/util/socket.hh ../src/corba/orb.hh
memory.spec.o: ../src/corba/coroutine.hh ../src/corba/giop.hh
memory.spec.o: ../src/corba/net/connection.hh interface/interface_impl.hh
memory.spec.o: interface/interface_skel.hh ../src/corba/corba.hh
memory.spec.o: ../src/corba/orb.hh ../src/corba/skeleton.hh
memory.spec.o: ../src/corba/object.hh ../src/corba/stub.hh
memory.spec.o: ../src/corba/ior.hh ../src/corba/exception.hh
memory.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh
memory.spec.o: ../src/corba/coroutine.hh interface/interface.hh
memory.spec.o: interface/interface_skel.hh util.hh ../src/corba/hexdump.hh
memory.spec.o: ../src/corba/coroutine.hh
lifecycle.spec.o: ../src/corba/exception.hh ../src/corba/giop.hh
lifecycle.spec.o: ../src/corba/cdr.hh ../src/corba/blob.hh
lifecycle.spec.o: ../src/corba/net/stream2packet.hh
lifecycle.spec.o: ../src/corba/net/tcp/connection.hh
lifecycle.spec.o: ../src/corba/net/tcp/protocol.hh
lifecycle.spec.o: ../src/corba/net/protocol.hh ../src/corba/net/connection.hh
lifecycle.spec.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
lifecycle.spec.o: ../upstream/async.cc/src/async.hh
lifecycle.spec.o: ../src/corba/net/util/socket.hh
lifecycle.spec.o: ../src/corba/net/connection.hh
lifecycle.spec.o: ../src/corba/net/stream2packet.hh
lifecycle.spec.o: ../src/corba/net/tcp/protocol.hh
lifecycle.spec.o: ../src/corba/net/util/socket.hh ../src/corba/orb.hh
lifecycle.spec.o: ../src/corba/coroutine.hh ../src/corba/giop.hh
lifecycle.spec.o: ../src/corba/net/connection.hh interface/interface_impl.hh
lifecycle.spec.o: interface/interface_skel.hh ../src/corba/corba.hh
lifecycle.spec.o: ../src/corba/orb.hh ../src/corba/skeleton.hh
lifecycle.spec.o: ../src/corba/object.hh ../src/corba/stub.hh
lifecycle.spec.o: ../src/corba/ior.hh ../src/corba/exception.hh
lifecycle.spec.o: ../src/corba/orb.hh ../src/corba/giop.hh
lifecycle.spec.o: ../src/corba/coroutine.hh interface/interface.hh
lifecycle.spec.o: interface/interface_skel.hh util.hh ../src/corba/hexdump.hh
lifecycle.spec.o: ../src/corba/coroutine.hh
net/tcp.spec.o: interface/interface_impl.hh interface/interface_skel.hh
net/tcp.spec.o: ../src/corba/corba.hh ../src/corba/orb.hh
net/tcp.spec.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
net/tcp.spec.o: ../src/corba/giop.hh ../src/corba/cdr.hh ../src/corba/blob.hh
net/tcp.spec.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
net/tcp.spec.o: ../src/corba/coroutine.hh ../src/corba/net/util/socket.hh
net/tcp.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
net/tcp.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
net/tcp.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
net/tcp.spec.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
net/tcp.spec.o: interface/interface.hh interface/interface_skel.hh
net/tcp.spec.o: ../src/corba/corba.hh ../src/corba/net/tcp/protocol.hh
net/tcp.spec.o: ../src/corba/net/protocol.hh ../src/corba/net/connection.hh
net/tcp.spec.o: ../src/corba/net/tcp/connection.hh
net/tcp.spec.o: ../src/corba/net/tcp/protocol.hh
net/tcp.spec.o: ../src/corba/net/connection.hh
net/tcp.spec.o: ../src/corba/net/stream2packet.hh util.hh
net/tcp.spec.o: ../src/corba/hexdump.hh ../src/corba/coroutine.hh
blob.spec.o: ../src/corba/blob.hh
corba.spec.o: fake.hh ../src/corba/blob.hh ../src/corba/net/protocol.hh
corba.spec.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
corba.spec.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
corba.spec.o: ../src/corba/net/util/socket.hh ../src/corba/corba.hh
corba.spec.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
corba.spec.o: ../src/corba/giop.hh ../src/corba/cdr.hh ../src/corba/blob.hh
corba.spec.o: ../src/corba/net/connection.hh ../src/corba/skeleton.hh
corba.spec.o: ../src/corba/object.hh ../src/corba/stub.hh ../src/corba/ior.hh
corba.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
corba.spec.o: ../src/corba/net/protocol.hh ../src/corba/url.hh util.hh
corba.spec.o: ../src/corba/hexdump.hh ../src/corba/coroutine.hh
interface/interface.spec.o: fake.hh ../src/corba/blob.hh
interface/interface.spec.o: ../src/corba/net/protocol.hh
interface/interface.spec.o: ../src/corba/net/connection.hh
interface/interface.spec.o: ../src/corba/blob.hh ../src/corba/coroutine.hh
interface/interface.spec.o: ../upstream/async.cc/src/async.hh
interface/interface.spec.o: ../src/corba/net/util/socket.hh util.hh
interface/interface.spec.o: ../src/corba/hexdump.hh ../src/corba/blob.hh
interface/interface.spec.o: ../src/corba/coroutine.hh
interface/interface.spec.o: interface/interface_impl.hh
interface/interface.spec.o: interface/interface_skel.hh ../src/corba/corba.hh
interface/interface.spec.o: ../src/corba/orb.hh ../src/corba/coroutine.hh
interface/interface.spec.o: ../src/corba/giop.hh ../src/corba/cdr.hh
interface/interface.spec.o: ../src/corba/net/connection.hh
interface/interface.spec.o: ../src/corba/skeleton.hh ../src/corba/object.hh
interface/interface.spec.o: ../src/corba/stub.hh ../src/corba/ior.hh
interface/interface.spec.o: ../src/corba/exception.hh ../src/corba/orb.hh
interface/interface.spec.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
interface/interface.spec.o: interface/interface.hh
interface/interface.o: ../src/corba/corba.hh ../src/corba/orb.hh
interface/interface.o: ../src/corba/coroutine.hh
interface/interface.o: ../upstream/async.cc/src/async.hh ../src/corba/giop.hh
interface/interface.o: ../src/corba/cdr.hh ../src/corba/blob.hh
interface/interface.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
interface/interface.o: ../src/corba/coroutine.hh
interface/interface.o: ../src/corba/net/util/socket.hh
interface/interface.o: ../src/corba/skeleton.hh ../src/corba/object.hh
interface/interface.o: ../src/corba/stub.hh ../src/corba/ior.hh
interface/interface.o: ../src/corba/exception.hh ../src/corba/orb.hh
interface/interface.o: ../src/corba/giop.hh ../src/corba/coroutine.hh
interface/interface.o: interface/interface.hh interface/interface_skel.hh
interface/interface.o: interface/interface_stub.hh
fake.o: fake.hh ../src/corba/blob.hh ../src/corba/net/protocol.hh
fake.o: ../src/corba/net/connection.hh ../src/corba/blob.hh
fake.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
fake.o: ../src/corba/net/util/socket.hh ../src/corba/orb.hh
fake.o: ../src/corba/coroutine.hh ../src/corba/giop.hh ../src/corba/cdr.hh
fake.o: ../src/corba/blob.hh ../src/corba/net/connection.hh
util.o: util.hh ../src/corba/hexdump.hh ../src/corba/blob.hh
util.o: ../src/corba/coroutine.hh ../upstream/async.cc/src/async.hh
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
../src/corba/net/connection.o: ../src/corba/net/tcp/protocol.hh
../src/corba/net/connection.o: ../src/corba/net/protocol.hh
../src/corba/net/connection.o: ../src/corba/stub.hh ../src/corba/object.hh
../src/corba/net/connection.o: ../src/corba/blob.hh
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
../src/corba/net/tcp/protocol.o: ../src/corba/orb.hh
../src/corba/net/tcp/protocol.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/protocol.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/tcp/protocol.o: ../src/corba/blob.hh
../src/corba/net/tcp/protocol.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/protocol.o: ../src/corba/exception.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/connection.hh
../src/corba/net/tcp/connection.o: ../src/corba/blob.hh
../src/corba/net/tcp/connection.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/connection.o: ../upstream/async.cc/src/async.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/util/socket.hh
../src/corba/net/tcp/connection.o: ../src/corba/exception.hh
../src/corba/net/tcp/connection.o: ../src/corba/orb.hh
../src/corba/net/tcp/connection.o: ../src/corba/coroutine.hh
../src/corba/net/tcp/connection.o: ../src/corba/giop.hh ../src/corba/cdr.hh
../src/corba/net/tcp/connection.o: ../src/corba/blob.hh
../src/corba/net/tcp/connection.o: ../src/corba/net/connection.hh
../src/corba/net/ws/protocol.o: ../src/corba/net/tcp/protocol.hh
../src/corba/net/ws/protocol.o: ../src/corba/net/protocol.hh
../src/corba/net/ws/protocol.o: ../src/corba/net/connection.hh
../src/corba/net/ws/protocol.o: ../src/corba/blob.hh
../src/corba/net/ws/protocol.o: ../src/corba/coroutine.hh
../src/corba/net/ws/protocol.o: ../upstream/async.cc/src/async.hh
../src/corba/net/ws/protocol.o: ../src/corba/net/util/socket.hh
../src/corba/net/util/socket.o: ../src/corba/net/util/socket.hh
../src/corba/net/util/createAcceptKey.o: ../src/corba/net/util/createAcceptKey.hh
../upstream/kaffeeklatsch/src/kaffeeklatsch.o: ../upstream/kaffeeklatsch/src/kaffeeklatsch.hh
