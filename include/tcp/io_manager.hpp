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

#include <unordered_map>
#include <queue>
#include <vector>
#include <memory>
#include <limits>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "io_manager_base.hpp"


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
		typedef boost::asio::ip::tcp::socket socket_type;
		static constexpr std::size_t buffer_length = BUFFER_LENGTH;
		using peernm = peer<io_manager<BUFFER_LENGTH>>;

		explicit io_manager(unsigned short port)
				: m_owner(nullptr)
				, m_io_service{}
				, m_acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
				, m_socket{std::make_shared<boost::asio::ip::tcp::socket>(m_io_service)}
				, m_id_string_bigendian()
				, m_data_queues()
		{
			static_assert(BUFFER_LENGTH > std::numeric_limits<uint8_t>::max(), "Buffer too small");
			m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		}

		io_manager(io_manager<BUFFER_LENGTH>&& other)
				: m_owner(other.m_owner)
				, m_io_service()
				, m_acceptor(std::move(other.m_acceptor))
				, m_socket(std::make_shared<boost::asio::ip::tcp::socket>(m_io_service))
				, m_id_string_bigendian(std::move(other.m_id_string_bigendian))
				, m_data_queues(std::move(other.m_data_queues))
		{
			other.m_socket->close();
			other.m_io_service.stop();
			unsigned short port = m_acceptor.local_endpoint().port();
			m_acceptor.close();
			m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)};
			if (m_owner != nullptr) {
				m_acceptor.async_accept(*m_socket, boost::bind(&io_manager<BUFFER_LENGTH>::accept, this, _1));
			}
		}

		~io_manager() {
			m_acceptor.close();
			m_socket->close();
			m_io_service.stop();
		}

		io_manager(const io_manager<BUFFER_LENGTH>&) = delete;
		io_manager<BUFFER_LENGTH>& operator=(const io_manager<BUFFER_LENGTH>&) = delete;

		template <typename data_container>
		void send(commands command, const data_container& data, const peernm& peer) const;

		template <typename data_iterator, typename size_type>
		void send(commands command, data_iterator begin, size_type size, const peernm& peer) const;

		peernm connect(const boost::asio::ip::address&, unsigned short port);

		void process_connected_peer(peernm& peer) override;

		void disconnect() override;

		void run() override ;

	private:

		void port(unsigned short port) {
			m_acceptor.close();
			m_acceptor = {m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)};
			m_acceptor.async_accept(*m_socket, boost::bind((&io_manager<BUFFER_LENGTH>::accept, this, _1)));
		}

		void owner(network_manager<io_manager<BUFFER_LENGTH>>* owner) override;

		void process_read(peernm& peer, boost::system::error_code error, std::size_t read);

		void write(const peernm& peer) const;

		void write_done(const peernm& peer) const;

		void accept(boost::system::error_code ec);

		network_manager<io_manager<BUFFER_LENGTH>>* m_owner;
		mutable boost::asio::io_service m_io_service;
		boost::asio::ip::tcp::acceptor m_acceptor;
		std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;

		std::string m_id_string_bigendian;

		mutable std::unordered_map<boost::uuids::uuid, std::queue<std::vector<uint8_t>>, boost::hash<boost::uuids::uuid>> m_data_queues;
	};

	typedef io_manager<1024> default_io_manager;
	typedef network_manager<default_io_manager> default_network_manager;
	typedef peer<default_io_manager> default_peer;
}

#include "tcp/impl/io_manager.tcc"

#endif //BREEP_TCP_IO_MANAGER
