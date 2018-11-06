#ifndef BREEP_UTIL_DESERIALIZER_HPP
#define BREEP_UTIL_DESERIALIZER_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017-2018 Lucas Lazare.                                                             //
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
#include <sstream>
#include <forward_list>
#include <vector>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <stack>
#include <queue>
#include <chrono>
#include <memory>

#include "breep/util/type_traits.hpp"

namespace breep {

	class deserializer;
	namespace detail {
		deserializer& right_shift_op_impl(deserializer&, uint8_t&);
	}

	class deserializer {

	public:
		explicit deserializer(const std::basic_string<uint8_t>& bs): m_is(bs) {}

		bool empty() const {
			return m_is.rdbuf()->in_avail() == 0;
		}

	private:
		std::basic_istringstream<uint8_t, std::char_traits<uint8_t>> m_is;

		// deserializing fundamental uint8_t
		friend deserializer& detail::right_shift_op_impl(deserializer&, uint8_t&);
	};

	/**
	 * If you wish to read the size of your custom container, which was written
	 * using breep::write_size(serializer&,SizeType), use this method.
	 */
	void read_size(deserializer& s, uint64_t& size);


	/**
	 * De-serializes a parameter
	 * @param d     de-serializer from which de-serialize
	 * @param value value to be de-serialized
	 * @return the original de-serializer, from which data has been removed
	 *
	 * @note if T is a pointer type, it will be de-referenced.
	 * @note if T is a pointer type, null values will call std::abort (through assert) except if NDEBUG is defined,
	 *       in which case behaviour with null values is undefined
	 */
	template <typename T> std::enable_if_t<!type_traits<T>::is_any_ptr, deserializer&> operator>>(deserializer& d, T&& value);
	template <typename T> std::enable_if_t<type_traits<T>::is_any_ptr, deserializer&> operator>>(deserializer& d, T&& value);
	template <typename T> deserializer& operator>>(deserializer& d, const std::unique_ptr<T>& value);
	template <typename T> deserializer& operator>>(deserializer& d, const std::shared_ptr<T>& value);

} // namespace breep

#include "impl/deserializer.tcc"

#endif //BREEP_UTIL_DESERIALIZER_HPP
