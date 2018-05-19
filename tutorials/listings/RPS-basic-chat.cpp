//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Lucas Lazare                                                                              //
//                                                                                                          //
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and        //
// associated documentation files (the "Software"), to deal in the Software without restriction,            //
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,    //
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,    //
// subject to the following conditions:                                                                     //
//                                                                                                          //
// The above copyright notice and this permission notice shall be included in all copies or substantial     //
// portions of the Software.                                                                                //
//                                                                                                          //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT    //
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.      //
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,  //
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      //
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                                   //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include <breep/network/tcp.hpp>
#include <iostream>

BREEP_DECLARE_TYPE(std::string)

void string_listener(breep::tcp::netdata_wrapper<std::string>& dw) {
	std::cout << "Received: " << dw.data << std::endl;
}

int main(int argc, char* argv[]) {
	if (argc != 2 && argc != 4) {
		std::cerr<< "Usage: " << argv[0] << " <hosting port> [<target ip> <target port>]\n";
		return 1;
	}

	breep::tcp::network network(std::atoi(argv[1]));
	network.add_data_listener<std::string>(&string_listener);

	if (argc == 2) {
		network.awake();
	} else {
		if(!network.connect(boost::asio::ip::address::from_string(argv[2]), std::atoi(argv[3]))) {
			std::cerr << "Connection failed.\n";
			return 1;
		}
	}

	std::string message;
	std::getline(std::cin, message);
	while (message != "/q") {
		network.send_object(message);
		std::getline(std::cin, message);
	}

	network.disconnect();
	return 0;
}