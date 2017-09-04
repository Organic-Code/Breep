#ifndef BREEP_NETWORK_TYPEDEFS_HPP
#define BREEP_NETWORK_TYPEDEFS_HPP

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
 * @file typedefs.hpp
 * @author Lucas Lazare
 */

#include <cstdint>
#include <utility>

namespace breep {

	using cuint8_random_iterator = const uint8_t*;
	using listener_id = unsigned long;

	struct type_listener_id : private std::pair<listener_id, uint64_t> {
		type_listener_id(listener_id id_, uint64_t type_hash_) : std::pair<listener_id,uint64_t>(id_, type_hash_) {}

		// id of the listener
		listener_id id() const { return first; }
		// hash of the type listened by the listener
		uint64_t type_hash() const { return second; }
	};
}
#endif //BREEP_NETWORK_TYPEDEFS_HPP
