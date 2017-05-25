#ifndef BREEP_UTIL_DESERIALIZER_HPP
#define BREEP_UTIL_DESERIALIZER_HPP

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
 * @file derializer.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <istream>
#include <forward_list>

namespace breep {
	class deserializer {

	public:
		explicit deserializer(std::basic_streambuf<uint8_t>* sb): m_is(sb) {}

	private:
		std::basic_istream<uint8_t> m_is;

		// deserializing fundamental uint8_t
		friend deserializer& operator>>(deserializer&, uint8_t&);
	};

	// deserializing fundamental types
	deserializer& operator>>(deserializer&, uint8_t&);
	deserializer& operator>>(deserializer&, uint16_t&);
	deserializer& operator>>(deserializer&, uint32_t&);
	deserializer& operator>>(deserializer&, uint64_t&);
	deserializer& operator>>(deserializer&, int8_t&);
	deserializer& operator>>(deserializer&, int16_t&);
	deserializer& operator>>(deserializer&, int32_t&);
	deserializer& operator>>(deserializer&, int64_t&);
	deserializer& operator>>(deserializer&, float&);
	deserializer& operator>>(deserializer&, double&);


	// generic method deserializing containers that support .insert(iterator, value)
	template <typename InsertableContainer>
	deserializer& operator>>(deserializer&, InsertableContainer&);

	// deserializing std::forward_list
	template <typename T>
	deserializer& operator>>(deserializer&, std::forward_list<T>&);

	// deserializing std::pair
	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::pair<T,U>&);

	// deserializing std::tuple with up to 7 elements
	template <typename T>
	deserializer& operator>>(deserializer&, std::tuple<T>&);

	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::tuple<T, U>&);

	template <typename T, typename U, typename V>
	deserializer& operator>>(deserializer&, std::tuple<T, U, V>&);

	template <typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W,X>&);

	template <typename T, typename U, typename V, typename W, typename X, typename Y>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W,X,Y>&);

	template <typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	deserializer& operator>>(deserializer&, std::tuple<T,U,V,W,X,Y>&);

	// deserializing durations
	template <typename T, typename U>
	deserializer& operator>>(deserializer&, std::chrono::duration<T,U>&);
}

#include "impl/deserializer.tcc"

#endif //BREEP_UTIL_DESERIALIZER_HPP
