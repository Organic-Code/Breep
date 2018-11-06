#ifndef BREEP_UTIL_SERIALIZER_HPP
#define BREEP_UTIL_SERIALIZER_HPP

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
 * @file serializer.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <sstream>
#include <chrono>
#include <forward_list>
#include <vector>
#include <memory>

#include "breep/util/type_traits.hpp"

namespace breep {

	class serializer;
	namespace detail {
		serializer& left_shift_op_impl(serializer&, uint8_t);
	}

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
		friend serializer& ::breep::detail::left_shift_op_impl(serializer&, uint8_t);
	};


	/**
	 * If you want to write the size of your custom container, use this method
	 * @tparam SizeType fundamental integer type.
	 */
	template <typename SizeType>
	void write_size(serializer& s, SizeType size);

	/**
	 * Serializes parameter
	 * @param s     serializer where to serialize
	 * @param value value to be serialized
	 * @return the original serializer, appended with extra data
	 *
	 * @note if T is a (smart)pointer type, it will be de-referenced.
	 * @note if T is a (smart)pointer type, null values will call std::abort (through assert) except if NDEBUG is defined,
	 *       in which case behaviour with null values is undefined
	 */
	template <typename T> std::enable_if_t<!type_traits<T>::is_any_ptr, serializer&> operator<<(serializer& s, T&& value);
	template <typename T> std::enable_if_t<type_traits<T>::is_any_ptr, serializer&> operator<<(serializer& s, T&& value);
	template <typename T> serializer& operator<<(serializer& s, const std::unique_ptr<T>& value);
	template <typename T> serializer& operator<<(serializer& s, const std::shared_ptr<T>& value);
}

#include "impl/serializer.tcc"

#endif //BREEP_UTIL_SERIALIZER_HPP
