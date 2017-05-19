
///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <vector>
#include <boost/uuid/uuid_io.hpp>

#include <breep/network/basic_peer_manager.hpp>
#include <breep/network/tcp/basic_io_manager.hpp>

#define forever for(;;)

void message_received(breep::tcp::peer_manager&, const breep::tcp::peer&, breep::cuint8_random_iterator, size_t, bool);
void connection(breep::tcp::peer_manager&, const breep::tcp::peer&);
void disconnection(breep::tcp::peer_manager&, const breep::tcp::peer&);
void connection_disconnection(breep::tcp::peer_manager&, const breep::tcp::peer&);

void message_received(breep::tcp::peer_manager&, const breep::tcp::peer& source, breep::cuint8_random_iterator data, size_t size, bool /**/) {
	std::cout << '\r' << boost::uuids::to_string(source.id()).substr(0,4) << ": ";
	for (; size > 0 ; --size) {
		std::cout << static_cast<char>(*data++);
	}
	std::cout << std::endl;
}

void connection(breep::tcp::peer_manager&, const breep::tcp::peer&) {
	std::cout << "Connection." << std::endl;
}

void disconnection(breep::tcp::peer_manager&, const breep::tcp::peer&) {
	std::cout << "Disconnection." << std::endl;
}

void connection_disconnection(breep::tcp::peer_manager&, const breep::tcp::peer& peer) {
	if (peer.is_connected()) {
		std::cout << "SYSTEM: " << boost::uuids::to_string(peer.id()).substr(0, 4) << " connected!" << std::endl;
	} else {
		std::cout << "SYSTEM: " << boost::uuids::to_string(peer.id()).substr(0,4) << " disconnected" << std::endl;
	}
}

int main(int argc, char* argv[]) {

	if (argc != 2 && argc != 4) {
		std::cout << "Usage: chat.elf <hosting port> [<target ip> <target port>]" << std::endl;
		return 1;
	}

	breep::tcp::peer_manager network((unsigned short)atoi(argv[1]));

	std::cout << "SYSTEM: " << network.self().id_as_string() << " is your identity" << std::endl;


	network.add_data_listener(&message_received);

	network.add_connection_listener(&connection);
	network.add_disconnection_listener(&disconnection);

	network.add_connection_listener(&connection_disconnection);
	network.add_disconnection_listener(&connection_disconnection);


	if (argc == 2) {
		network.run();
	} else {
		boost::asio::ip::address address = boost::asio::ip::address::from_string(argv[2]);
		network.connect(address, static_cast<unsigned short>(atoi(argv[3])));
	}

	std::string ans;
	forever {
		std::getline(std::cin, ans);
		if (ans == "/q") {
			std::cout << "Leaving..." << std::endl;
			network.disconnect();
			break;
		} else {
			network.send_to_all(ans);
		}
	}
}
