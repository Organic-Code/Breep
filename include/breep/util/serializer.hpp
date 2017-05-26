#ifndef BREEP_UTIL_SERIALIZER_HPP
#define BREEP_UTIL_SERIALIZER_HPP

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
 * @file serializer.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <sstream>
#include <chrono>
#include <forward_list>

namespace breep {
	class serializer {

	public:
		serializer(): m_os() {}

		std::basic_string<uint8_t> str() {
			m_os.flush();
			return m_os.str();
		}

	private:
		std::basic_ostringstream<uint8_t> m_os;

		// serializing fundamental uint8_t
		friend serializer& operator<<(serializer&, uint8_t);
	};

	// serializing fundamental types
	serializer& operator<<(serializer&, bool);
	serializer& operator<<(serializer&, char);
	serializer& operator<<(serializer&, uint16_t);
	serializer& operator<<(serializer&, uint32_t);
	serializer& operator<<(serializer&, uint64_t);
	serializer& operator<<(serializer&, int8_t);
	serializer& operator<<(serializer&, int16_t);
	serializer& operator<<(serializer&, int32_t);
	serializer& operator<<(serializer&, int64_t);
	serializer& operator<<(serializer&, float);
	serializer& operator<<(serializer&, double);

	// generic method to serialize containers that supports iterators
	template <typename IterableContainer>
	serializer& operator<<(serializer&, const IterableContainer&);

	template <typename T>
	serializer& operator<<(serializer&, const std::forward_list<T>&);

	// serializing std::pair
	template <typename T, typename U>
	serializer& operator<<(serializer&, const std::pair<T,U>&);

	// serializing std::tuple with up to 7 elements
	template <typename T>
	serializer& operator<<(serializer&, const std::tuple<T>&);

	template <typename T, typename U>
	serializer& operator<<(serializer&, const std::tuple<T, U>&);

	template <typename T, typename U, typename V>
	serializer& operator<<(serializer&, const std::tuple<T, U, V>&);

	template <typename T, typename U, typename V, typename W>
	serializer& operator<<(serializer&, const std::tuple<T,U,V,W>&);

	template <typename T, typename U, typename V, typename W, typename X>
	serializer& operator<<(serializer&, const std::tuple<T,U,V,W,X>&);

	template <typename T, typename U, typename V, typename W, typename X, typename Y>
	serializer& operator<<(serializer&, const std::tuple<T,U,V,W,X,Y>&);

	template <typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	serializer& operator<<(serializer&, const std::tuple<T,U,V,W,X,Y>& val);

	// serializing durations
	template <typename T, typename U>
	serializer& operator<<(serializer&, const std::chrono::duration<T,U>&);
}

#include "impl/serializer.tcc"

#endif //BREEP_UTIL_SERIALIZER_HPP
