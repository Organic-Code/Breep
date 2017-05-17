#ifndef BREEP_BASIC_PEER_HPP
#define BREEP_BASIC_PEER_HPP

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
#include <utility>

#include "breep/commands.hpp"

namespace breep {

	/**
	 * @class peer peer.hpp
	 *
	 * @brief This class represents a network's member
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class basic_peer {
	public:

		/**
	 	 * @since 0.1.0
		 */
		basic_peer(const boost::uuids::uuid& id, const boost::asio::ip::address& address, unsigned short port = 0,
		     const typename io_manager::data_type& data = typename io_manager::data_type())
			: basic_peer(
				boost::uuids::uuid(id),
				boost::asio::ip::address(address),
				port,
			    typename io_manager::data_type(data)
			)
		{}

		/**
	 	 * @since 0.1.0
		 */
		basic_peer(boost::uuids::uuid&& id, boost::asio::ip::address&& address, unsigned short port,
		     typename io_manager::data_type&& data)
				: m_id(std::move(id))
				, m_address(std::move(address))
				, m_port(port)
				, m_distance()
				, io_data(std::move(data))
		{}

		/**
	 	 * @since 0.1.0
		 */
		basic_peer(const basic_peer<io_manager>& p)
			: m_id(p.m_id)
			, m_address(p.m_address)
			, m_port(p.m_port)
			, m_distance(p.m_distance)
			, io_data(p.io_data)
		{}

		/**
		 * @since 0.1.0
		 */
		basic_peer(basic_peer<io_manager>&&) = default;

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

		bool operator==(const basic_peer<io_manager>& lhs) const;

		bool operator!=(const basic_peer<io_manager>& lhs) const;

		/**
		 * @return The distance from you to the peer.
		 * @since 0.1.0
		 */
		unsigned char distance() const {
			return m_distance;
		}

		void distance(unsigned char d) {
			m_distance = d;
		}

		void connection_port(unsigned short port) {
			m_port = port;
		}

		unsigned short connection_port() const {
			return m_port;
		}

		bool is_connected() const {
			return m_distance != std::numeric_limits<unsigned char>::max(); // todo.
		}

	private:
		const boost::uuids::uuid m_id;
		const boost::asio::ip::address m_address;

		unsigned short m_port;

		/**
		 * Distance from here to the other peers.
		 */
		unsigned char m_distance;

		/**
		 * Datas to be used by the io_manager class
		 */
		typename io_manager::data_type io_data;

		friend io_manager;
	};

#include "impl/basic_peer.tcc"
}


#endif //BREEP_BASIC_PEER_HPP
