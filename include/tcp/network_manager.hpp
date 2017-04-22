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

		network_manager(): m_owner(nullptr), m_io_service{} {
			static_assert(BUFFER_LENGTH > std::numeric_limits<uint8_t>::max(), "Buffer too small");
		}

		network_manager(const network_manager<BUFFER_LENGTH>&) = delete;
		network_manager<BUFFER_LENGTH>& operator=(const network_manager<BUFFER_LENGTH>&) = delete;

		template <typename data_container>
		void send(commands command, const data_container& data, const peernm& peer) const;

		template <typename data_iterator, typename size_type>
		void send(commands command, data_iterator begin, size_type size, const peernm& peer) const;

		peernm connect(const boost::asio::ip::address&, unsigned short port);

		void process_connected_peer(peernm& peer) override;

		void disconnect(peernm& peer) override;

		void run() override ;

	private:
		void owner(network<network_manager<BUFFER_LENGTH>>* owner) override;

		void process_read(peernm& peer, boost::system::error_code error, std::size_t read);

		void process_disconnection(peernm& disconnected_peer);

		void write_done(boost::system::error_code error, std::size_t length) const;

		network<network_manager<BUFFER_LENGTH>>* m_owner;
		boost::asio::io_service m_io_service;
	};

	typedef network_manager<1024> default_network_manager;
}}

#include "tcp/impl/network_manager.tcc"

#endif //BREEP_TCP_NETWORK_HPP
