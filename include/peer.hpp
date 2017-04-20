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
#include <cstdint>
#include <vector>
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
		peer(const boost::uuids::uuid& id, const boost::asio::ip::address& address,
		     std::shared_ptr<typename network_manager::socket_type> socket = std::shared_ptr<typename network_manager::socket_type>(nullptr))
			: peer(
				boost::uuids::uuid(id),
				boost::asio::ip::address(address),
				std::shared_ptr<typename network_manager::socket_type>(socket)
		)
		{}

		/**
	 	 * @since 0.1.0
		 */
		peer(boost::uuids::uuid&& id, boost::asio::ip::address&& address,
		     std::shared_ptr<typename network_manager::socket_type>&& socket)
				: m_id(std::move(id))
				, m_address(std::move(address))
				, m_socket(std::move(socket))
				, m_fixed_buffer()
				, m_dynamic_buffer()
				, m_last_received_command()
		{
			m_dynamic_buffer.reserve(network_manager::buffer_length);
		}

		/**
	 	 * @since 0.1.0
		 */
		peer(const peer<network_manager>& p)
			: peer(
				boost::uuids::uuid(p.m_id),
				boost::asio::ip::address(p.m_address),
				std::shared_ptr<typename network_manager::socket_type>(p.m_socket)
		)
		{}

		/**
		 * @since 0.1.0
		 */
		peer(peer<network_manager>&&) = default;

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

		bool operator==(const peer<network_manager>& lhs) const;

		bool operator!=(const peer<network_manager>& lhs) const;

	private:
		const boost::uuids::uuid m_id;
		const boost::asio::ip::address m_address;

		/**
		 * m_socket must be set by the \em network_manager class
		 */
		std::shared_ptr<typename network_manager::socket_type> m_socket;

		/**
		 * fixed size buffer unused by this class, left to use for network_manager
		 */
		mutable std::array<uint8_t, network_manager::buffer_length> m_fixed_buffer;

		/**
		 * dynamic buffer unused by this class, left to use for network_manager
		 */
		mutable std::vector<uint8_t> m_dynamic_buffer;

		/**
		 * variable unused by this class, left to use for network_manager.
		 */
		mutable commands m_last_received_command;

		friend network_manager;
	};

	namespace constant {
		template<typename T>
		static const peer<T> bad_peer =
				peer<T>(boost::uuids::nil_uuid(), boost::asio::ip::address(), std::shared_ptr<typename T::socket_type>());
	}

#include "impl/peer.tcc"
}


#endif //BREEP_PEER_HPP
