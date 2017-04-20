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
	class network_manager final: public network_manager_base<network_manager> {
	public:
		typedef boost::asio::ip::tcp::socket socket_type;
		static const std::size_t buffer_length = 1024;

		network_manager(): m_owner(nullptr), m_io_service{} {}

		network_manager(const network_manager&) = delete;
		network_manager& operator=(const network_manager&) = delete;

		template <typename data_container>
		void send(commands command, const data_container& data, const peer<network_manager>& peer) const;

		template <typename data_iterator, typename size_type>
		void send(commands command, data_iterator begin, size_type size, const peer<network_manager>& peer) const;

		peer<network_manager> connect(const boost::asio::ip::address&, unsigned short port);

		void process_connected_peer(peer<network_manager>& peer) override;

		void disconnect(peer<network_manager>& peer) override;

	private:
		void owner(network<network_manager>* owner) override;

		void process_disconnection(peer<network_manager>& disconnected_peer);

		network<network_manager>* m_owner;
		boost::asio::io_service m_io_service;
	};
}}

#include "tcp/impl/network_manager.tcc"

#endif //BREEP_TCP_NETWORK_HPP
