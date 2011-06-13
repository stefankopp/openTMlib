
OBJECTS += \
	demo_opentmlib.o \
	session_factory.o \
	io_session.o \
	usbtmc_session.o \
	socket_session.o \
	vxi11_session.o \
	vxi11_clnt.o \
	vxi11_xdr.o \
	serial_session.o \
	opentmlib.o
	
demo_opentmlib: $(OBJECTS)
	@echo "Linking $<"
	@g++ -o $@ $(OBJECTS)

clean:
	rm *.o *.d demo_opentmlib

.cpp.o:
	@echo "compiling $<"
	@$(CXX) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

.c.o:
	@echo "compiling $<"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
