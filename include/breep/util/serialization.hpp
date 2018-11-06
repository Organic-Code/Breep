///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BREEP_UTIL_SERIALIZATION_HPP
#define BREEP_UTIL_SERIALIZATION_HPP
/**
 * @file serialization.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#include "serializer.hpp"
#include "deserializer.hpp"

#define BREEP_DETAIL_SERIALIZE_WRITE(r, value, attribute) s << value.attribute;

#define BREEP_DETAIL_SERIALIZE_READ(r, value, attribute) d >> value.attribute;

/**
 * @brief Allows automation of simple serialization
 * @details This macro should be used in the class definition,
 *          and replaces the explicit declaration of serializer operators.
 *          For any non trivial serialization operations or versioning system,
 *          you will have to implement stream operators by hand.
 *
 * @param class_name The name of the class, not necessarily fully qualified
 * @param ...        List of fields that should be serialized
 *
 * @attention This macro cannot be used for pointers of any sort.
 *            It can be used like this:
 *            @code{.cpp}
 *                BREEP_ENABLE_SERIALIZATION(foo, a, b) // good if a and b are not pointers
 *            @endcode
 *            but not like this:
 *            @code{.cpp}
 *                BREEP_ENABLE_SERIALIZATION(bar, a, *b) // bad
 *            @endcode
 *
 * @since 1.0.0
 */
#define BREEP_ENABLE_SERIALIZATION(class_name, ...) \
	friend breep::serializer& operator<<(breep::serializer& s, const class_name& value) { \
		BOOST_PP_SEQ_FOR_EACH(BREEP_DETAIL_SERIALIZE_WRITE, value, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
		return s; \
	} \
	\
	friend breep::deserializer& operator>>(breep::deserializer& d, class_name& value) { \
		BOOST_PP_SEQ_FOR_EACH(BREEP_DETAIL_SERIALIZE_READ, value, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
		return d; \
	}

#endif //BREEP_UTIL_SERIALIZATION_HPP
