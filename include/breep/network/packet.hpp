#ifndef BREEP_NETWORK_PACKET_HPP
#define BREEP_NETWORK_PACKET_HPP

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
 * @file serializer.cpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include "breep/util/serializer.hpp"
#include "breep/util/type_traits.hpp"

namespace breep {

	/**
	 * Class used to send several classes at once, using basic_network::send_packet or basic_network::send_packet_to
	 */
	class packet {
	public:
		packet();

	private:
		serializer m_s;

		template <typename T>
		friend packet& operator<<(packet& p, const T& val);

		template <typename T>
		friend class basic_network;
	};

	template <typename T>
	packet& operator<<(packet& p, const T& val) {
		p.m_s << type_traits<T>::hash_code();
		p.m_s << val;
		return p;
	}
}

BREEP_DECLARE_TYPE(breep::packet)

inline breep::packet::packet() : m_s() {
	m_s << type_traits<packet>::hash_code();
}

#endif //BREEP_NETWORK_PACKET_HPP
