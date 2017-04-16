
///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


#include "tcp/network_manager.hpp" // TODO: remove [Seems useless, but allows my IDE to work]

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp> // boost::uuids::to_string
#include <boost/array.hpp>
#include <boost/uuid/string_generator.hpp>
#include <memory>
#include <string>

#include "detail/utils.hpp"
#include "network.hpp"

template <typename data_container>
inline void breep::tcp::network_manager::send(commands command, const data_container& data, const peer<network_manager>& peer) const {
	send(command, data.cbegin(), data.cend(), peer);
}

template <typename data_iterator>
inline void breep::tcp::network_manager::send(commands command, data_iterator begin, const data_iterator& end, const peer<network_manager>& peer) const {
	std::vector<uint8_t> buff;
	buff.reserve(4096);
	buff.push_back(static_cast<uint8_t>(command));
	std::copy(begin, end, std::back_inserter(buff));
	boost::asio::write(*(peer.m_socket), boost::asio::buffer(buff));
}

breep::peer<breep::tcp::network_manager> breep::tcp::network_manager::connect(const boost::asio::ip::address& address, unsigned short port) {
	boost::asio::io_service io_service;
	boost::asio::ip::tcp::resolver resolver(io_service);
	boost::asio::ip::tcp::resolver::iterator endpoint_iterator =
			resolver.resolve(boost::asio::ip::tcp::resolver::query(address.to_string(),std::to_string(port)));

	std::shared_ptr<boost::asio::ip::tcp::socket> socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service);
	boost::asio::connect(*socket, endpoint_iterator);
	boost::asio::write(
			*socket,
			boost::asio::buffer(breep::detail::to_bigendian1(boost::uuids::to_string(m_owner->self().id())))
	);
	boost::system::error_code error;
	boost::array<char, 512> buffer;
	size_t len = socket->read_some(boost::asio::buffer(buffer), error);

	if (error) {
		return constant::bad_peer<network_manager>;
	}

	std::vector<char> input{};
	input.reserve(len);
	std::copy(buffer.cbegin(), buffer.cend(), std::back_inserter(input));
	return peer<network_manager>(
			boost::uuids::string_generator{}(breep::detail::to_bigendian2<std::string>(input)),
			boost::asio::ip::address(address),
	        std::move(socket)
	);
}

inline void breep::tcp::network_manager::disconnect(breep::peer<network_manager>& peer) {
	send(commands::peer_disconnection, breep::detail::to_bigendian1(boost::uuids::to_string(peer.id())), peer);
	peer.m_socket = std::shared_ptr<socket_type>(nullptr);
}

inline void breep::tcp::network_manager::owner(network<network_manager>* owner) {
	if (m_owner == nullptr) {
		m_owner = owner;
	} else {
		throw invalid_state("Tried to set an already set owner. This object shouldn't be shared.");
	}
}