///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <utility>
#include <boost/detail/endian.hpp>

/**
 * @file utils.tcc
 * @author Lucas Lazare
 * @since 0.1.0
 */

template <typename Container, typename OutputIterator>
inline void breep::detail::little_endian(const Container& container, OutputIterator outputIterator) {
	using std::iterator_traits;
	static_assert(sizeof(typename Container::value_type) == 1, "Invalid container type.");
	static_assert(sizeof(typename OutputIterator::container_type::value_type) == 1, "Invalid container type.");

#ifdef BOOST_BIG_ENDIAN
	#ifndef BREEP_NOWARNING
		#ifdef _MSC_VER
			#pragma message ("This library as not been tested for big endian architectures, and is probably not working on such device. Feedback is very welcome.")
			#pragma message ("Note: to disable this warning, define the macro BREEP_NOWARNING before including breep's headers.")
		#else
			#warning "This library as not been tested for big endian architectures, and is probably not working on such device. Feedback is very welcome."
			#warning "Note: to disable this warning, define the macro BREEP_NOWARNING before including breep's headers."
		#endif
	#endif

	size_t max = container.size() - container.size() % sizeof(uintmax_t);
	for(size_t i{0} ; i < max ; i += sizeof(uintmax_t)) {
		for (uint_fast8_t j{sizeof(uintmax_t)} ; j-- ;) {
			*out++ = data[i + j];
		}
	}
	if (max < container.size()) {
		for (uint_fast8_t j{static_cast<uint_fast8_t>(max + sizeof(uintmax_t) - container.size() + 1)} ; --j ;) {
			*out++ = 0;
		}
		for (uint_fast8_t i{static_cast<uint_fast8_t>(data.size() - max)} ; i-- ;) {
			*out++ = data[max + i];
		}
	}
#elif defined BOOST_LITTLE_ENDIAN
	std::copy(container.cbegin(), container.cend(), outputIterator);
#else
#error "Unknown endianness (if endianness is known, please manually define BOOST_LITTLE_ENDIAN or BOOST_BIG_ENDIAN)."
#error "Middle endian is not supported."
#endif
}