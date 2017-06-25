
///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * @file chatv2/main.cpp
 * @author Lucas Lazare
 */

#include <breep/network/tcp.hpp>
#include <breep/util/serialization.hpp>

#include <string>
#include <iostream>


/* This class will be sent through the network */
class square {
public:
	/* Default constructor is required by serialization. */
	square(): m_height(0), m_width(0) {}
	explicit square(int height): m_height(height), m_width(2 * height) {}
	square(int height, int width): m_height(height), m_width(width) {}

	int height() const {
		return m_height;
	}

	int width() const {
		return m_width;
	}

private:
	int m_height;
	int m_width;
	// enabling de/serialization for the square class.
	BREEP_ENABLE_SERIALIZATION(square, m_width, m_height)
};

/* This is just a container class that is not of much use, but that is here to demonstrate the
 * use of template class with the network. */
template <typename T>
class chat_message {
public:

	/* Default constructor is required by serialization. */
	chat_message(): m_message{} {}
	explicit chat_message(const T& message)
			: m_message(message)
	{}

	const T& message() const {
		return m_message;
	}

	template <typename U>
	friend breep::deserializer& operator>>(breep::deserializer&, chat_message<U>&);

private:
	T m_message;
	BREEP_ENABLE_SERIALIZATION(chat_message<T>, m_message)
};


/* This class will be sent to each newly connected peer, so that he can know our name. */
struct name {

	/* Default constructor is required by serialization. */
	name() : value{} {}
	explicit name(const std::string& name_) : value(name_) {}

	std::string value;
};


// Writing the serialization operators manually this time (just to demonstrate how to do this)
breep::serializer& operator<<(breep::serializer& s, const name& local_name) {
	s << local_name.value;
	return s;
}

breep::deserializer& operator>>(breep::deserializer& d, name& local_name) {
	d >> local_name.value;
	return d;
}


/* Here is where we declare the types for breep::network.
 * These macros must be called outside of all namespace. */
BREEP_DECLARE_TYPE(name)
/* chat_message is a partially intiated type (missing template parameter */
BREEP_DECLARE_TEMPLATE(chat_message)


/* We need to declare these two types because they will be used as
 * template parameter of chat_message. */
BREEP_DECLARE_TYPE(std::string)
BREEP_DECLARE_TYPE(square)

class chat_room {
public:
	explicit chat_room(const std::string& name)
			: m_name(name)
            , peer_map()
            , m_co_id()
            , m_dc_id()
			, m_ids()
	{}

	void start_listening(breep::tcp::network& net) {
		// because we overloaded the () operator with [breep::tcp::network&, const breep::tcp::peer& parameters,
		// we can register ourselves this way. the () method will be called.
		m_co_id = net.add_connection_listener(std::ref(*this));
		m_dc_id = net.add_disconnection_listener(std::ref(*this));
		//nb: we are using std::ref to avoid a copy of ourself to be made

		// add_data_listener returns a different type than (add_connection/add_disconnecton)_listener.
		m_ids.push_back(net.add_data_listener<name>(std::ref(*this)));

		/* We can also record listeners this way */
		m_ids.push_back(net.add_data_listener<chat_message<std::string>>(
		                [this] (breep::tcp::netdata_wrapper<chat_message<std::string>>& dw) -> void {
			                this->string_received(dw);
		                }));

		m_ids.push_back(net.add_data_listener<chat_message<square>>(
				// The netdata_wrapper structure holds some nice infos. read the doc! :p
				[this] (breep::tcp::netdata_wrapper<chat_message<square>>& dw) -> void {
					this->square_received(dw);
				}));
	}

	void stop_listening(breep::tcp::network& net) {
		// unregistering listeners.
		net.remove_connection_listener(m_co_id);
		net.remove_disconnection_listener(m_dc_id);
		for (const breep::type_listener_id& tli : m_ids) {
			net.remove_data_listener(tli);
		}
	}

	// A peer just connected // disconnected
	void operator()(breep::tcp::network& network, const breep::tcp::peer& peer) {
		if (peer.is_connected()) {
			// if it's someone new, we send her/him our name
			network.send_object_to(peer, m_name);
		} else {
			// (s)he left :/
			std::cout << peer_map.at(peer.id()) << " disconnected." << std::endl;
			peer_map.erase(peer.id());
		}
	}

	void square_received(breep::tcp::netdata_wrapper<chat_message<square>>& dw) {
		// We received a square ! Let's draw it.
		int height = dw.data.message().height();
		int width  = dw.data.message().width();

		std::cout << peer_map.at(dw.source.id()) << ":\n";

		std::cout << "\t#";
		for (int i{2} ; i < width ; ++i) {
			std::cout << "-";
		}
		std::cout << "#\n";
		for (int i{2} ; i < height ; ++i) {
			std::cout << "\t|";
			for (int j{2} ; j < width ; ++j) {
				std::cout << ' ';
			}
			std::cout << "|\n";
		}

		std::cout << "\t#";
		for (int i{2} ; i < width ; ++i) {
			std::cout << "-";
		}
		std::cout << "#\n";
	}

	void string_received(breep::tcp::netdata_wrapper<chat_message<std::string>>& dw) {
		// Someone sent you a nice little message.
		std::cout << peer_map.at(dw.source.id()) << ": " << dw.data.message() << std::endl;
	}

	void operator()(breep::tcp::netdata_wrapper<name>& dw) {
		// The guy to whom we just connected sent us his/her name.
		std::cout << dw.data.value << " connected." << std::endl;
		peer_map.insert(std::make_pair(dw.source.id(), dw.data.value));
	}

private:
	const name m_name;
	std::unordered_map<boost::uuids::uuid, std::string, boost::hash<boost::uuids::uuid>> peer_map;

	breep::listener_id m_co_id, m_dc_id;
	std::vector<breep::type_listener_id> m_ids;
};

int main(int argc, char* argv[]) {

	if (argc != 2 && argc != 4) {
		std::cout << "Usage: chat.elf <hosting port> [<target ip> <target port>]" << std::endl;
		return 1;
	}

	std::string name;
	std::cout << "Enter a name: ";
	std::cin >> name;

	/*              we will be listening on port given as first parameter -v */
	breep::tcp::network network(static_cast<unsigned short>(std::atoi(argv[1])));

	// Disabling all logs (set to 'warning' by default).
	network.set_log_level(breep::log_level::none);

	chat_room cr(name);
	cr.start_listening(network);

	// If we receive a class for which we don't have any listener (such as an int, for example), this will be called.
	network.set_unlistened_type_listener([](breep::tcp::network&,const breep::tcp::peer&,breep::deserializer&,bool,uint64_t) -> void {
		std::cout << "Unlistened class received." << std::endl;
	});

	std::cout << "Commands: /q to quit, /square <size> to send a rectangle, and /packet to send a packet with several things\n";
	std::cout << "Starting..." << std::endl;
	if (argc == 2) {
		// runs the network in another thread.
		network.awake();
	} else {
		// let's try to connect to a buddy at address argv[2] and port argv[3]
		boost::asio::ip::address address = boost::asio::ip::address::from_string(argv[2]);
		if(!network.connect(address, static_cast<unsigned short>(atoi(argv[3])))) {
			// oh noes, it failed!
			std::cout << "Connection failed." << std::endl;
			return 1;
		}
	}

	std::cin.ignore();
	std::string ans;
	while(true) {
		std::getline(std::cin, ans);
		if (ans[0] == '/') {
			if (ans == "/q") {
				// Good bye !
				network.disconnect();
				break;
			} else if (ans.substr(0,7) == "/square") {
				// Here is a square for the peers
				network.send_object(chat_message<square>(square(atoi(ans.data() + 8))));
			} else if (ans.substr(0,7) == "/packet"){
				// Just to demonstrate the usage of breep::packet
				using str = chat_message<std::string>;
				breep::packet p;
				p << str("pa") << str("ck") << str("et") << chat_message<square>(square(25)) << 3.1415;
				network.send_packet(p);
			} else {
				std::cout << "Unknown command: " << ans << std::endl;
			}
		} else {
			// Here is a message for the peers
			network.send_object(chat_message<std::string>(ans));
		}
	}

	// we'll remove any listeners (useless here, as we're going out of scope.
	network.clear_any();
	return 0;
}
