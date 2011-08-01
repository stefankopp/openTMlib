
LIBOBJECTS += \
	session_factory.o \
	io_session.o \
	usbtmc_session.o \
	socket_session.o \
	vxi11_session.o \
	vxi11_clnt.o \
	vxi11_xdr.o \
	serial_session.o \
	opentmlib.o \
	configuration_store.o \
	io_monitor.o
	
TESTBENCHOBJECTS += \
	demo_opentmlib.o \
	session_factory.o \
	io_session.o \
	usbtmc_session.o \
	socket_session.o \
	vxi11_session.o \
	vxi11_clnt.o \
	vxi11_xdr.o \
	serial_session.o \
	opentmlib.o \
	configuration_store.o \
	io_monitor.o
	
all: libopentmlib.so demo_opentmlib
	@echo "$@ done."

libopentmlib.so: $(LIBOBJECTS)
	@echo "Linking $@"
	@g++ -o $@ -shared -Wl,-soname,$@ -rdynamic $(LIBOBJECTS)
		
demo_opentmlib: $(TESTBENCHOBJECTS)
	@echo "Linking $@"
	@g++ -o $@ $(TESTBENCHOBJECTS)
	
clean:
	rm *.o *.d demo_opentmlib opentmlib.so

.cpp.o:
	@echo "compiling $<"
	@g++ -fPIC -g -c -o $@ $<

.c.o:
	@echo "compiling $<"
	@gcc -fPIC -g -c -o $@ $<
