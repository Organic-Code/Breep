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

namespace breep {
	class tcp_nmanager: public network_manager_base<tcp_nmanager> {
	public:
		typedef boost::asio::ip::tcp::socket socket_type;

		tcp_nmanager(): m_owner(nullptr) {}

		tcp_nmanager(const tcp_nmanager&) = delete;
		tcp_nmanager& operator=(const tcp_nmanager&) = delete;

		template <typename data_container>
		void send(commands command, const data_container& data, const peer<tcp_nmanager>& peer) const;

		template <typename data_iterator>
		void send(commands command, data_iterator begin, const data_iterator& end, const peer<tcp_nmanager>& peer) const;

		peer<tcp_nmanager> connect(const boost::asio::ip::address&, unsigned short port);

		void disconnect(const peer<tcp_nmanager>&);

	private:
		void owner(network<tcp_nmanager>* owner);

		network<tcp_nmanager>* m_owner;
	};
}

#include "impl/tcp_nmanager.tcc"

#endif //BREEP_TCP_NETWORK_HPP
