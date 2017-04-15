#ifndef BREEP_PEER_HPP
#define BREEP_PEER_HPP

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
 * @file peer.hpp
 * @author Lucas Lazare
 */

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <memory>

namespace breep {

	/**
	 * @class peer peer.hpp
	 *
	 * @brief This class represents a network's member
	 * @since 0.1.0
	 */
	template <typename network_manager>
	class peer {
	public:

		/**
	 	 * @since 0.1.0
		 */
		peer(const boost::uuids::uuid& id, const boost::asio::ip::address& address, std::shared_ptr<network_manager::socket_type> socket = std::shared_ptr(nullptr))
				: m_id(id)
				, m_address(address)
				, m_socket(socket)
		{}

		/**
		 * @return the id of the peer
	 	 * @since 0.1.0
		 */
		const boost::uuids::uuid& id() const noexcept;

		/**
		 * @return the address
	 	 * @since 0.1.0
		 */
		const boost::asio::ip::address& address() const noexcept;

	private:
		const boost::uuids::uuid m_id;
		const boost::asio::ip::address m_address;

		/**
		 * m_socket must be set by the \em network_manager class
		 */
		std::shared_ptr<network_manager::socket_type> m_socket;

		friend network_manager;
	};

#include "peer.tpp"
}


#endif //BREEP_PEER_HPP
