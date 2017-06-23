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
#include <vector>

namespace breep {
	class serializer {

	public:
		serializer(): m_os() {}

		std::basic_string<uint8_t> str() const {
			m_os.flush();
			return m_os.str();
		}

	private:
		mutable std::basic_ostringstream<uint8_t> m_os;

		// serializing fundamental uint8_t
		friend serializer& operator<<(serializer&, uint8_t);
	};


	/**
	 * If you want to write the size of your custom container, use this method
	 * @tparam SizeType fundamental integer type.
	 */
	template <typename SizeType>
	void write_size(serializer& s, SizeType size);

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

	// generic method to serialize containers that supports iterators (assuming we can dereference and get a const ref)
	template <typename IterableContainer>
	serializer& operator<<(serializer&, const IterableContainer&);

    // std::vector<bool> is not like that.
    serializer& operator<<(serializer& s, const std::vector<bool>&);

	template <typename T>
	serializer& operator<<(serializer&, const std::forward_list<T>&);

	// serializing std::pair
	template <typename T, typename U>
	serializer& operator<<(serializer&, const std::pair<T,U>&);

	// serializing std::tuple
	template <typename... T>
	serializer& operator<<(serializer&, const std::tuple<T...>&);
}

#include "impl/serializer.tcc"

#endif //BREEP_UTIL_SERIALIZER_HPP
