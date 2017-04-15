#ifndef BREEP_NETWORK_MANAGER_BASE_HPP
#define BREEP_NETWORK_MANAGER_BASE_HPP


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
 * @file invalid_state.hpp
 * @author Lucas Lazare
 */

#include "commands.hpp"

namespace boost::asio::ip {
	class address;
}

namespace breep {

	template <typename T>
	class network<T>;

	/**
	 * @class network_manager_base network_manager_base.hpp
	 * @brief base class for network managers, used by \em network<typename network_manager>.
	 *
	 * @sa breep::network
	 *
	 * @since 0.1.0
	 */
	template <typename network_manager>
	class network_manager_base {
	public:

		virtual ~network_manager_base() {

		}

		/**
		 * @brief Sends data to a peer
		 *
		 * @tparam data_container Any data container type you want to support.
		 * @param command command of the packet (considered as data)
		 * @param data data to be sent
		 * @param address address of the peer to whom to send the data.
		 */
		template <typename data_container>
		virtual void send(commands command, const data_container& data, const boost::asio::ip::address& address) = 0;

		/**
		 * @brief connects to a peer
		 */
		virtual void connect(const boost::asio::ip::address&) = 0;

		/**
		 * @brief disconnects from a peer
		 */
		virtual void disconnect(const boost::asio::ip::address&) = 0;

	private:
		/**
		 * @brief sets the owner of the network_manager, ie the object to whom received datas should be redirected.
		 */
		virtual void owner(network<network_manager>* owner) = 0;

		friend class network<network_manager>;
	};
}

#endif //BREEP_NETWORK_MANAGER_BASE_HPP
