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

namespace breep { namespace tcp {
	class network_manager: public network_manager_base<network_manager> {
	public:
		typedef boost::asio::ip::tcp::socket socket_type;

		network_manager(): m_owner(nullptr) {}

		network_manager(const network_manager&) = delete;
		network_manager& operator=(const network_manager&) = delete;

		template <typename data_container>
		void send(commands command, const data_container& data, const peer<network_manager>& peer) const;

		template <typename data_iterator>
		void send(commands command, data_iterator begin, const data_iterator& end, const peer<network_manager>& peer) const;

		peer<network_manager> connect(const boost::asio::ip::address&, unsigned short port);

		void disconnect(peer<network_manager>& peer);

	private:
		void owner(network<network_manager>* owner);

		network<network_manager>* m_owner;
	};
}}

#include "tcp/impl/network_manager.tcc"

#endif //BREEP_TCP_NETWORK_HPP
