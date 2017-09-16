#ifndef BREEP_NETWORK_DETAIL_UTILS_HPP
#define BREEP_NETWORK_DETAIL_UTILS_HPP

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
 * @since 0.1.0
 */

#include <limits>
#include <vector>
#include <string>
#include <array>
#include <boost/optional.hpp>
#include <boost/any.hpp>

namespace breep { namespace detail {

	template <typename T>
	using optional = boost::optional<T>;

	using any = boost::any;

	template <typename T>
	auto any_cast(any& lany) {
		return boost::any_cast<T>(lany);
	}

	struct unowning_linear_container;

	/**
	 * Converts data from local endiannes to little endian and vice versa.
	 *
	 * @param container data to convert
	 * @param outputIterator iterator where to store resulting data.
	 */
	template <typename Container, typename OutputIterator>
	void little_endian(const Container& container, OutputIterator outputIterator);

	template <typename Container, typename ValueType>
	inline void make_little_endian(const Container& container, std::vector<ValueType>& vect) {
		size_t idx = vect.size();
		vect.reserve(idx + container.size() + 1);
		vect.push_back(0);
		little_endian(container, std::back_inserter(vect));
		vect[idx] = static_cast<ValueType>(vect.size() - 1 - container.size()); // number of trailing 0 introduced by the endianness change.
	}

	template <typename Container>
	inline void make_little_endian(const Container& container, std::string& str) {
		size_t idx = str.size();
		str.reserve(idx + container.size() + 1);
		str.push_back(0);
		little_endian(container, std::back_inserter(str));
		str[idx] = static_cast<char>(str.size() - idx - 1 - container.size()); // number of trailing 0 introduced by the endianness change.
	}

	template <typename LinearContainer, typename ValueType>
	inline void unmake_little_endian(const LinearContainer& container, std::vector<ValueType>& vect) {
		vect.reserve(container.size() - 1 - container[0]);
		little_endian(detail::unowning_linear_container(container.data() + 1, container.size() - 1 - container[0]),
		              std::back_inserter(vect));
	}

	template <typename LinearContainer>
	inline void unmake_little_endian(const LinearContainer& container, std::string& str) {
		str.reserve(container.size() - 1 - container[0]);
		little_endian(detail::unowning_linear_container(container.data() + 1, container.size() - 1 - container[0]),
		              std::back_inserter(str));
	}

	template <typename Container>
	inline void insert_uint16(Container& container, uint16_t uint16) {
		static_assert(sizeof(typename Container::value_type) == 1, "Invalid container type.");
		container.push_back(static_cast<uint8_t>(uint16 >> 8) & std::numeric_limits<uint8_t>::max());
		container.push_back(static_cast<uint8_t>(uint16 & std::numeric_limits<uint8_t>::max()));
	}

	template <typename Container>
	inline void insert_uint32(Container& container, uint32_t uint32) {
		insert_uint16(container, static_cast<uint16_t>((uint32 >> 16) & std::numeric_limits<uint16_t>::max()));
		insert_uint16(container, static_cast<uint16_t>(uint32 & std::numeric_limits<uint16_t>::max()));
	}

	template <typename Container>
	inline uint16_t read_uint16(const Container& container, typename Container::size_type idx = 0) {
		return static_cast<uint16_t>(container[idx] << 8 | container[idx + 1]);
	}

	template <typename Container>
	inline uint32_t read_uint32(const Container& container, typename Container::size_type idx = 0) {
		return read_uint16(container, idx) << 16 | read_uint16(container, idx + sizeof(uint16_t));
	}



	template <typename... T>
	struct dependent_false { static constexpr bool value = false; };

	using unused = std::array<uint8_t, 1>;

	struct unowning_linear_container {
		using value_type = uint8_t;

		unowning_linear_container(const uint8_t* data, size_t size)
				: data_(data)
				, size_(size)
		{}

		template <size_t N>
		explicit unowning_linear_container(const uint8_t(&data)[N])
				: data_(data)
				, size_(N)
		{}

		const uint8_t* data() const {
			return data_;
		}

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

	template <typename T>
	bool constexpr is_const_ref = std::is_lvalue_reference<T>::value && std::is_const<typename std::remove_reference<T>::type>::value;
}}

namespace breep { namespace constant {
	constexpr detail::unused unused_param{};
}}

#include "breep/network/detail/impl/utils.tcc"

#endif //BREEP_NETWORK_DETAIL_UTILS_HPP
