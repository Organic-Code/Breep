#ifndef BREEP_TCP_NETWORK_HPP
#define BREEP_TCP_NETWORK_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <queue>
#include <vector>
#include <memory>
#include <limits>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "network_manager_base.hpp"
#include "network.hpp"

namespace boost { namespace asio {
		class io_service;
}}

namespace breep { namespace tcp {

	template <unsigned int BUFFER_LENGTH>
	class network_manager final: public network_manager_base<network_manager<BUFFER_LENGTH>> {
	public:
		typedef boost::asio::ip::tcp::socket socket_type;
		static constexpr std::size_t buffer_length = BUFFER_LENGTH;
		using peernm = peer<network_manager<BUFFER_LENGTH>>;

		explicit network_manager(unsigned short port)
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

		network_manager(network_manager<BUFFER_LENGTH>&& other)
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
				m_acceptor.async_accept(*m_socket, boost::bind(&network_manager<BUFFER_LENGTH>::accept, this, _1));
			}
		}

		~network_manager() {
			m_acceptor.close();
			m_socket->close();
			m_io_service.stop();
		}

		network_manager(const network_manager<BUFFER_LENGTH>&) = delete;
		network_manager<BUFFER_LENGTH>& operator=(const network_manager<BUFFER_LENGTH>&) = delete;

		template <typename data_container>
		void send(commands command, const data_container& data, const peernm& peer) const;

		template <typename data_iterator, typename size_type>
		void send(commands command, data_iterator begin, size_type size, const peernm& peer) const;

		peernm connect(const boost::asio::ip::address&, unsigned short port);

		void process_connected_peer(peernm& peer) override;

		void disconnect() override;

		void run() override ;

	private:
		void owner(network<network_manager<BUFFER_LENGTH>>* owner) override;

		void process_read(peernm& peer, boost::system::error_code error, std::size_t read);

		void write(const peernm& peer) const;

		void write_done(const peernm& peer) const;

		void accept(boost::system::error_code ec);

		network<network_manager<BUFFER_LENGTH>>* m_owner;
		mutable boost::asio::io_service m_io_service;
		boost::asio::ip::tcp::acceptor m_acceptor;
		std::unique_ptr<boost::asio::ip::tcp::socket> m_socket;

		std::string m_id_string_bigendian;

		mutable std::unordered_map<boost::uuids::uuid, std::queue<std::vector<uint8_t>>, boost::hash<boost::uuids::uuid>> m_data_queues;
	};

	typedef network_manager<1024> default_network_manager;
	typedef network<default_network_manager> default_network;
	typedef peer<default_network_manager> default_peer;
}}

#include "tcp/impl/network_manager.tcc"

#endif //BREEP_TCP_NETWORK_HPP
