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
 * @since 0.1.0
 */

#include "breep/network/typedefs.hpp"

namespace breep {
	template<typename>
	class basic_network;

	template<typename T, typename U>
	struct basic_netdata_wrapper {
		basic_netdata_wrapper(basic_network<T>& network_, const typename basic_network<T>::peer& source_, const U& data_, bool is_private_)
				: network(network_), source(source_), data(data_), is_private(is_private_), listener_id() {}


		/**
		 * @brief contains the instance of the network that called you
		 * @since 0.1.0
		 */
		basic_network<T>& network;

		/**
		 * @brief contains the peer that sent you the data
		 * @since 0.1.0
		 */
		const typename basic_network<T>::peer& source;

		/**
		 * @brief data that the peer sent you
		 * @since 0.1.0
		 */
		const U& data;

		/**
		 * @brief true if the data was sent only to you, false otherwise
		 * @since 0.1.0
		 */
		const bool is_private;

		/**
		 * @brief your own listener id
		 * @since 0.1.0
		 */
		breep::listener_id listener_id;
	};
} // namespace breep

#endif // BREEP_NETWORK_BASIC_NETDATA_WRAPPER_HPP
