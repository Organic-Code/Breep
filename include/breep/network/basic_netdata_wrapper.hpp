#ifndef BREEP_NETWORK_BASIC_NETDATA_WRAPPER_HPP
#define BREEP_NETWORK_BASIC_NETDATA_WRAPPER_HPP

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
 * @file netdata_network.hpp
 * @author Lucas Lazare
 */

#include "breep/network/typedefs.hpp"

namespace breep {
	template<typename>
	class basic_network;

	template<typename T, typename U>
	struct basic_netdata_wrapper {
		basic_netdata_wrapper(basic_network<T>& network_, const typename basic_network<T>::peer& source_, const U& data_, bool is_private_)
				: network(network_), source(source_), data(data_), is_private(is_private_), listener_id() {}


		// instance of the network that called you
		basic_network<T>& network;

		// peer that sent you the data
		const typename basic_network<T>::peer& source;

		// the data itself
		const U& data;

		// true if the data was sent only to you, false otherwise
		const bool is_private;

		// your own listener_id
		breep::listener_id listener_id;
	};
}

#endif // BREEP_NETWORK_BASIC_NETDATA_WRAPPER_HPP
