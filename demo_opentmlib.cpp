#include <stdio.h>
#include <string>
#include <iostream>
#include "vxi11_session.hpp"

using namespace std;

int main()
{

	char buffer[1000];
	int ret;
	vxi11_session *session;

	try
	{
		session = new vxi11_session("169.254.2.20", "inst0", false, 5);
		session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		session->write_string("*IDN?", true);
		string response;
		response.resize(200);
		session->read_string(response);
		cout << "Response is " << response;
		delete session;
	}

	catch (int e)
	{
		cout << "Error " << e << endl;
	}

	return 0;

}
