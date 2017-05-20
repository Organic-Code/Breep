
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
#include <chrono>
#include <boost/uuid/uuid_io.hpp>

#include <breep/network/tcp.hpp>

class timed_message {
public:
	timed_message(): m_starting_time{time(0)} {}

	/*
	 * Using an operator() overload allows you to pass objects as listeners.
	 */
	void operator()(breep::tcp::peer_manager& /* peer_manager */, const breep::tcp::peer& source, breep::cuint8_random_iterator data,
	                      size_t data_size, bool /* sent_only_to_me */) {

		// print the time and the name of the buddy that sent me something
		time_t now = time(0) - m_starting_time;
		std::cout << '[' << std::string(ctime(&now)).substr(14,5) << "] " << source.id_as_string().substr(0, 4) << ": ";

		// prints what he sent.
		for (; data_size > 0 ; --data_size) {
			std::cout << static_cast<char>(*data++);
		}
		std::cout << std::endl;
		// we could reply directly here by using the peer_manager passed as parameter.
		//ie : peer_manager.send_to_all("reply"); or peer_manager.send_to(source, "reply");
	}

private:
	const time_t m_starting_time;
};

/*
 * This method will get called whenever a peer connects // disconnects
 * (connection listeners can be used as disconnection listeners and vice-versa)
 */
void connection_disconnection(breep::tcp::peer_manager& /* peer_manager */, const breep::tcp::peer& peer) {
	if (peer.is_connected()) {
		// someone connected
		std::cout << peer.id_as_string().substr(0, 4) << " connected!" << std::endl;
	} else {
		// someone disconnected
		std::cout << peer.id_as_string().substr(0, 4) << " disconnected" << std::endl;
	}
}

int main(int argc, char* argv[]) {

	if (argc != 2 && argc != 4) {
		std::cout << "Usage: chat.elf <hosting port> [<target ip> <target port>]" << std::endl;
		return 1;
	}

	// taking the local hosting port as parameter.
	breep::tcp::peer_manager peer_manager(static_cast<unsigned short>(atoi(argv[1])));

	std::cout << "you are " << peer_manager.self().id_as_string() << "." << std::endl;

	// adding listeners. Of course, more listeners could be added.
	breep::listener_id da_listener_id = peer_manager.add_data_listener(timed_message());
	breep::listener_id co_listener_id = peer_manager.add_connection_listener(&connection_disconnection);
	breep::listener_id dc_listener_id = peer_manager.add_disconnection_listener(&connection_disconnection);


	if (argc == 2) {
		// only hosting
		peer_manager.run();
	} else {
		// connecting to a remote peer.                                           v− address in string format (v4 or v6)
		boost::asio::ip::address address = boost::asio::ip::address::from_string(argv[2]);
		//                                                    target port -v
		if (!peer_manager.connect(address, static_cast<unsigned short>(atoi(argv[3])))) {
			std::cout << "Connection failed" << std::endl;
			return 1;
		}
	}


	std::string ans;
	while(true) {
		std::getline(std::cin, ans);
		if (ans == "/q") {
			std::cout << "Leaving..." << std::endl;
			peer_manager.disconnect();
			break;
		} else {
			peer_manager.send_to_all(ans);
		}
	}

	// this is not obligatory, as the peer_manager is going out of scope anyway
	peer_manager.remove_data_listener(da_listener_id);
	peer_manager.remove_connection_listener(co_listener_id);
	peer_manager.remove_disconnection_listener(dc_listener_id);
}
