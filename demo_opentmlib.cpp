#include <stdio.h>
#include <string>
#include <iostream>
#include "session_factory.hpp"
#include "vxi11_session.hpp"
#include "usbtmc_session.hpp"
#include "opentmlib.hpp"

using namespace std;

int main()
{

	char buffer[1000];
	int ret;

	session_factory *factory;
	factory = new session_factory;

	io_session *session;

	try
	{

		session = factory->open_session("ASRL::INSTR", 0, 5);
		session->write_string("*IDN?", true);
		string response;
		response.resize(200);
		session->read_string(response);
		cout << "Response is " << response;
		delete session;

	}

	catch (int e)
	{
		cout << "Error code: " << e << endl;
		string error_message;
		error_message.resize(100);
		opentmlib_error(e, error_message);
		cout << "Error: " << error_message << endl;
	}

	delete factory;

	return 0;

}
