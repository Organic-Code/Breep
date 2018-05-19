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
#include <breep/util/serialization.hpp>
#include <iostream>

BREEP_DECLARE_TYPE(std::string)

enum class rpc : unsigned char {
	rock,
	paper,
	scissors
};
BREEP_DECLARE_TYPE(rpc)

breep::serializer& operator<<(breep::serializer& s, rpc value) {
	s << static_cast<unsigned char>(value);
	return s;
}

breep::deserializer& operator>>(breep::deserializer& d, rpc& value) {
	unsigned char extracted;
	d >> extracted;
	value = static_cast<rpc>(extracted);
	return d;
}

struct name {
	name() : name_() {}

	name(const std::string& val)
			: name_(val)
	{}

	std::string name_;

	BREEP_ENABLE_SERIALIZATION(name, name_)
};
BREEP_DECLARE_TYPE(name)

class game_manager {
public:
	game_manager(const std::string& name, breep::tcp::network& network)
			: m_name(name)
			, m_inputted(false)
			, m_score(0)
			, m_player_nbr(1)
			, m_choice()
			, m_plays()
			, m_nicknames()
			, m_network(network)
	{}

	void connection_event(breep::tcp::network& network, const breep::tcp::peer& peer) {
		if (peer.is_connected()) {
			network.send_object_to(peer, m_name);
		} else {
			std::cout << m_nicknames.at(peer.id()) << " disconnected." << std::endl;
			--m_player_nbr;
		}
	}

	void name_received(breep::tcp::netdata_wrapper<name>& dw) {
		++m_player_nbr,
				m_nicknames.insert(std::make_pair(dw.source.id(), dw.data.name_));
		std::cout << dw.data.name_ << " connected." << std::endl;
		if (m_inputted) {
			dw.network.send_object_to(dw.source, m_choice);
		}
	}

	void message_received(breep::tcp::netdata_wrapper<std::string>& dw) {
		std::cout << m_nicknames.at(dw.source.id()) << ": " << dw.data << std::endl;
	}

	void input(rpc player_choice) {
		if (!m_inputted) {
			m_inputted = true;
			m_choice = player_choice;
			m_network.send_object(m_choice);

			if (m_plays.size() >= m_player_nbr - 1) {
				next_turn();
			}
		}
	}

	void rpc_received(breep::tcp::netdata_wrapper<rpc>& dw) {
		m_plays.push_back(dw.data);

		if (m_plays.size() >= m_player_nbr - 1 && m_inputted) {
			next_turn();
		}
	}

	void next_turn() {
		for (rpc opponent_choice: m_plays) {
			if (opponent_choice != m_choice) {
				if (static_cast<unsigned char>(opponent_choice) + 1 == static_cast<unsigned char>(m_choice)
						|| (opponent_choice == rpc::scissors && m_choice == rpc::rock)) {
					m_score++;
				} else {
					m_score--;
				}
			}
		}
		m_inputted = false;
		m_plays.clear();
		std::cout << "Everyone played!\n";
		std::cout << "Your new score: " << m_score << std::endl;
	}

private:
	name m_name;
	bool m_inputted;
	int m_score;
	int m_player_nbr;
	rpc m_choice;
	std::vector<rpc> m_plays;
	std::unordered_map<boost::uuids::uuid, std::string,  boost::hash<boost::uuids::uuid>> m_nicknames;
	breep::tcp::network& m_network;
};

int main(int argc, char* argv[]) {
	if (argc != 2 && argc != 4) {
		std::cerr<< "Usage: " << argv[0] << " <hosting port> [<target ip> <target port>]\n";
		return 1;
	}

	std::string nick;
	std::cout << "Enter your nickname: ";
	std::getline(std::cin, nick);


	breep::tcp::network network(std::atoi(argv[1]));
	game_manager game(nick, network);

	network.add_data_listener<name>([&game](breep::tcp::netdata_wrapper<name>& dw) -> void {
		game.name_received(dw);
	});
	network.add_data_listener<std::string>([&game](breep::tcp::netdata_wrapper<std::string>& dw) -> void {
		game.message_received(dw);
	});
	
	network.add_data_listener<rpc>([&game](breep::tcp::netdata_wrapper<rpc>& dw) -> void {
		game.rpc_received(dw);
	});

	network.add_connection_listener([&game](breep::tcp::network& net, const breep::tcp::peer& peer) -> void {
		game.connection_event(net, peer);
	});

	network.add_disconnection_listener([&game](breep::tcp::network& net, const breep::tcp::peer& peer) -> void {
		game.connection_event(net, peer);
	});

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
		if (message[0] == '/') {
			if (message == "/rock") {
				game.input(rpc::rock);
			} else if (message == "/paper") {
				game.input(rpc::paper);
			} else if (message == "/scissors") {
				game.input(rpc::scissors);
			} else {
				std::cout << "Unknown command: " << message << std::endl;
			}
		} else {
			network.send_object(message);
		}
		std::getline(std::cin, message);
	}

	network.disconnect();
	return 0;
}