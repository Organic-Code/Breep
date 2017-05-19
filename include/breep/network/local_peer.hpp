#ifndef BREEP_NETWORK_LOCAL_PEER_HPP
#define BREEP_NETWORK_LOCAL_PEER_HPP

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
 * @file local_peer.hpp
 * @author Lucas Lazare
 *
 * @since 0.1.0
 */

#include <vector>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/functional/hash.hpp>

#include "basic_peer.hpp"

namespace breep {

	/**
	 * @class local_peer local_peer.hpp
	 * @extends peer
	 *
	 * @brief This class represent the user's client
	 * @since 0.1.0
	 */
	template <typename io_manager>
	class local_peer: public basic_peer<io_manager> {
	public:

		local_peer()
				: basic_peer<io_manager>::basic_peer(boost::uuids::random_generator{}(),
				                              boost::asio::ip::address_v4::loopback())
				, m_bridging_from_to{}
				, m_path_to_passing_by{}
		{}

		/**
		 * @brief   Returns the peer who's bridging towards \em p.
		 * @details If there is no direct network access from
		 *          the user's to the peer \em p returns the peer to whom
		 *          data should be sent so that they will be re-routed to
		 *          \em p. If there is a direct link, return \em p.
		 * @param p Peer to whom data are to be sent.
		 * @return  The peer who's bridging towards \em p.
		 *
		 * @since 0.1.0
		 */
		const basic_peer<io_manager>*& path_to(const basic_peer<io_manager>& p);

		/**
		 * @copydoc local_peer::path_to(const peer<io_manager>&)
		 */
		basic_peer<io_manager> const * const & path_to(const basic_peer<io_manager>& p) const;

		/**
		 * @return A reference to the private field m_path_to_passing_by
		 *
		 * @since 0.1.0
		 */
		std::unordered_map<boost::uuids::uuid, const breep::basic_peer<io_manager>*, boost::hash<boost::uuids::uuid>>&
		path_to_passing_by() noexcept;

		/**
		 * @copydoc local_peer::path_to_passing_by()
		 */
		const std::unordered_map<boost::uuids::uuid, const breep::basic_peer<io_manager>*, boost::hash<boost::uuids::uuid>>&
		path_to_passing_by() const noexcept;

		/**
		 * @return A reference to the private field m_bridging_from_to
		 *
		 * @since 0.1.0
		 */
		std::unordered_map<boost::uuids::uuid, std::vector<const breep::basic_peer<io_manager>*>, boost::hash<boost::uuids::uuid>>&
		bridging_from_to() noexcept;

		/**
		 * @copydoc local_peer::bridging_from_to()
		 */
		const std::unordered_map<boost::uuids::uuid, std::vector<const breep::basic_peer<io_manager>*>, boost::hash<boost::uuids::uuid>>&
		bridging_from_to() const noexcept;

	private:
		/**
		 * @brief   Map representing links.
		 * @details Assuming you are peer \em a. If the connection from
		 *          peer \em b to peer \em c is using you as a bridge,
		 *          then we have
		 *          @code{.cpp}
		 *          	const std::vector<peer<T>>& v1 = m_bridging_from_to.at(b);
		 *          	std::find(v1.cbegin(), v1.cend(), c) != v1.end();
		 *          	const std::vector<peer<T>>& v2 = m_bridging_from_to.at(c);
		 *          	std::find(v2.cbegin(), v2.cend(), b) != v2.end();
		 *          @endcode
		 *          otherwise, we have
		 *          @code{.cpp}
		 *          	const std::vector<peer<T>>& v1 = m_bridging_from_to.at(b);
		 *          	std::find(v1.cbegin(), v1.cend(), c) == v1.end();
		 *          	const std::vector<peer<T>>& v2 = m_bridging_from_to.at(c);
		 *          	std::find(v2.cbegin(), v2.cend(), b) == v2.end();
		 *          @endcode
		 *
		 * @sa local_peer::path_to(const peer& p)
		 * @sa local_peer::m_path_to_passing_by
		 *
		 * @since 0.1.0
		 */
		std::unordered_map<boost::uuids::uuid, std::vector<const breep::basic_peer<io_manager>*>, boost::hash<boost::uuids::uuid>> m_bridging_from_to;


		/**
		 * @brief Map representing links.
		 * @details Assuming you are peer \em a. If the link to a peer \em b
		 *          passes through peer \em c, then we have
		 *          @code{.cpp}
		 *          	m_path_to_passing_by.at(b) == c;
		 *          @endcode
		 *          otherwise, we have
		 *          @code{.cpp}
		 *          	m_bridging_from_to.at(b) == b;
		 *          @endcode
		 *
		 * @sa local_peer::path_to(const peer& p)
		 * @sa local_peer::m_bridging_from_to
		 *
		 * @since 0.1.0
		 */
		std::unordered_map<boost::uuids::uuid, const breep::basic_peer<io_manager>*, boost::hash<boost::uuids::uuid>> m_path_to_passing_by;
	};

}

#include "impl/local_peer.tcc"

#endif //BREEP_NETWORK_LOCAL_PEER_HPP
