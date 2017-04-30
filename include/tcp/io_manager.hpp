#ifndef BREEP_TCP_IO_MANAGER
#define BREEP_TCP_IO_MANAGER

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
 * @file tcp/network_manager.hpp
 * @author Lucas Lazare
 */

#include <iostream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <memory>
#include <limits>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "io_manager_base.hpp"
#include "exceptions.hpp"


namespace breep {
	template <typename T>
	class network_manager;
}

namespace breep::tcp {

	/**
	 * @brief reference tcp network_manager implementation
	 *
	 * @tparam BUFFER_LENGTH size of the local buffer
	 *
	 * @since 0.1.0
	 */
	template <unsigned int BUFFER_LENGTH>
	class io_manager final: public io_manager_base<io_manager<BUFFER_LENGTH>> {
	public:

		// The protocol ID should be changed at each compatibility break.
		static constexpr uint32_t IO_PROTOCOL_ID_1 =  755960663;
		static constexpr uint32_t IO_PROTOCOL_ID_2 = 1683390694;

		static constexpr std::size_t buffer_length = BUFFER_LENGTH;

		using peernm = peer<io_manager<BUFFER_LENGTH>>;
		typedef boost::asio::ip::tcp::socket socket_type;

		explicit io_manager(unsigned short port);

		io_manager(io_manager<BUFFER_LENGTH>&& other);

		~io_manager();

		io_manager(const io_manager<BUFFER_LENGTH>&) = delete;
		io_manager<BUFFER_LENGTH>& operator=(const io_manager<BUFFER_LENGTH>&) = delete;

		template <typename Container>
		void send(commands command, const Container& data, const peernm& peer) const;

		template <typename InputIterator, typename size_type>
		void send(commands command, InputIterator begin, size_type size, const peernm& peer) const;

		peernm connect(const boost::asio::ip::address&, unsigned short port);

		void process_connected_peer(peernm& peer) override;

		void disconnect() override;

		void run() override ;

	private:

		void port(unsigned short port) {
			make_id_packet();

			m_acceptor.close();
			m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port)};
			m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<BUFFER_LENGTH>::accept, this, _1));

			if (m_acceptor_v4 != nullptr) {
				m_acceptor_v4->close();
				*m_acceptor_v4 = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)};
				m_acceptor_v4->async_accept(*m_socket, boost::bind(&io_manager<BUFFER_LENGTH>::accept, this, _1));
			}
		}

		void owner(network_manager<io_manager<BUFFER_LENGTH>>* owner) override;

		void process_read(peernm& peer, boost::system::error_code error, std::size_t read);

		void write(const peernm& peer) const;

		void write_done(const peernm& peer) const;

		void accept(boost::system::error_code ec);

		void make_id_packet() {
			std::string id_str(boost::uuids::to_string(m_owner->self().id()));
			m_id_packet.clear();
			m_id_packet.resize(3, 0);
			detail::make_little_endian(id_str, m_id_packet);

			m_id_packet[0] = static_cast<char>(m_id_packet.size() - 1);
			m_id_packet[1] = static_cast<uint8_t>(m_owner->port() >> 8) & std::numeric_limits<uint8_t>::max();
			m_id_packet[2] = static_cast<uint8_t>(m_owner->port() & std::numeric_limits<uint8_t>::max());
		}

		network_manager<io_manager<BUFFER_LENGTH>>* m_owner;
		mutable boost::asio::io_service m_io_service;
		boost::asio::ip::tcp::acceptor m_acceptor;
		boost::asio::ip::tcp::acceptor* m_acceptor_v4;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;

		std::string m_id_packet;

		mutable std::unordered_map<boost::uuids::uuid, std::queue<std::vector<uint8_t>>, boost::hash<boost::uuids::uuid>> m_data_queues;
	};

	typedef io_manager<1024> default_io_manager;
	typedef network_manager<default_io_manager> default_network_manager;
	typedef peer<default_io_manager> default_peer;
}

#include "tcp/impl/io_manager.tcc"

#endif //BREEP_TCP_IO_MANAGER
