#ifndef BREEP_UTILS_HPP
#define BREEP_UTILS_HPP

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
 * @file utils.hpp
 * @author Lucas Lazare
 */


namespace breep::detail {
	/**
	 * @brief Converts from local endianness to big endian and the reverse.
	 * @tparam data_container Using operator[](int) and size() methods, and ::type for underlying type (if no output container is given).
	 */
	template<typename data_container, typename output_container = std::vector<typename data_container::value_type>>
	output_container littleendian1(const data_container& data);

	template<typename output_container, typename data_container>
	inline output_container littleendian2(const data_container& data) {
		return littleendian1<data_container, output_container>(data);
	}

	template <typename Container>
	inline void insert_uint16(Container& container, uint16_t uint16) {
		static_assert(sizeof(typename Container::value_type) == 1);
		container.push_back(static_cast<uint8_t>(uint16 >> 8) & std::numeric_limits<uint8_t>::max());
		container.push_back(static_cast<uint8_t>(uint16 & std::numeric_limits<uint8_t>::max()));
	}



	template <typename... T>
	struct dependent_false { static constexpr bool value = false; };

	struct unused{ constexpr unused() {}};

	struct fake_mini_container {
		typedef uint8_t value_type;

		fake_mini_container(const uint8_t* data, size_t size)
			: data_(data)
			, size_(size)
		{}

		uint8_t operator[](size_t index) const {
			return data_[index];
		}

		size_t size() const {
			return size_;
		}

		const uint8_t* cbegin() const {
			return data_;
		}

		const uint8_t* cend() const {
			return data_ + size_;
		}

		uint8_t const * const data_;
		const size_t size_;
	};
}

namespace breep::constant {
	constexpr detail::unused unused_param{};
}

#include "detail/impl/utils.tcc"

#endif //BREEP_UTILS_HPP
