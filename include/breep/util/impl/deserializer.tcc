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
 * @file serializer.cpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <chrono>

#include "../deserializer.hpp" // Allows my IDE to work

namespace breep {

	namespace detail {
		template <typename FloatType, typename InputType, typename LeastExponentType, typename LeastMantissaType, unsigned int ExponentBits, unsigned int MantissaBits>
		FloatType fromIEEE(InputType value) {

			LeastExponentType exponentMax = 0;
			for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
				exponentMax = static_cast<LeastExponentType>(exponentMax | 1 << i);
			}

			if (value == 0) {
				return FloatType(0.);
			}

			if (value == (InputType(1) << (MantissaBits + ExponentBits))) {
				return FloatType(-0.);
			}

			if (value & (exponentMax << MantissaBits) == (exponentMax << MantissaBits)) {

			}

			FloatType result = (value & ((InputType(1) << MantissaBits) - 1));
			result /= (InputType(1) << MantissaBits);
			result += FloatType(1.);

			LeastExponentType biased_exponent = (1 << (ExponentBits - 1)) - 1;
			int_fast16_t exponent = ((value >> MantissaBits) & ((InputType(1) << ExponentBits) - 1)) - biased_exponent;
			while(exponent > 0) { result *= 2.0; exponent--; }
			while(exponent < 0) { result /= 2.0; exponent++; }

			return result * ((value >> (ExponentBits + MantissaBits)) & 1 ? -1.0 : 1.0);
		}

		void read_size(deserializer& s, uint64_t size) {
			uint8_t uint8;
			uint_fast8_t oct_to_read;
			s >> oct_to_read;
			while (oct_to_read--) {
				s >> uint8;
				size |= (uint8 << (oct_to_read * 8));
			}
		}
	}

	deserializer& operator>>(deserializer& s, uint8_t& val) {
		s.m_is.get(val);
		return s;
	}

	deserializer& operator>>(deserializer& s, bool& val) {
		uint8_t uint8;
		s >> uint8;
		val = (uint8 == '1');
		return s;
	}

	deserializer& operator>>(deserializer& s, uint16_t& val) {
		uint8_t uint8;
		s >> uint8;
		val = uint8 << 8;
		s >> uint8;
		val |= uint8;
		return s;
	}

	deserializer& operator>>(deserializer& s, uint32_t& val) {
		uint16_t uint16;
		s >> uint16;
		val = uint16 << 16;
		s >> uint16;
		val |= uint16;
		return s;
	}

	deserializer& operator>>(deserializer& s, uint64_t& val) {
		uint32_t uint32;
		s >> uint32;
		val = uint32 << 32;
		s >> uint32;
		val |= uint32;
		return s;
	}

	deserializer& operator>>(deserializer& s, int8_t& val) {
		uint8_t uint8;
		s >> uint8;
		val = static_cast<int8_t>(uint8);
		return s;
	}

	deserializer& operator>>(deserializer& s, int16_t& val) {
		uint16_t uint16;
		s >> uint16;
		val = static_cast<int16_t>(uint16);
		return s;
	}

	deserializer& operator>>(deserializer& s, int32_t& val) {
		uint32_t uint32;
		s >> uint32;
		val = static_cast<int32_t>(uint32);
		return s;
	}

	deserializer& operator>>(deserializer& s, int64_t& val) {
		uint64_t uint64;
		s >> uint64;
		val = static_cast<int64_t>(uint64);
		return s;
	}

	deserializer& operator>>(deserializer& s, float& val) {
		uint32_t uint32;
		s >> uint32;
		val = detail::fromIEEE<float, uint32_t, uint8_t, uint_fast32_t, 8, 23>(uint32);
		return s;
	}

	deserializer& operator>>(deserializer& s, double& val) {
		uint64_t uint64;
		s >> uint64;
		val = detail::fromIEEE<double, uint64_t, uint16_t, uint_fast64_t, 11, 52>(uint64);
		return s;
	}

	// FIXME: what about non-fifo STL containers?
	// TODO: optimize for STL containers (.insert is there for almost everyone, but is not the best)
	template<typename InsertableContainer>
	deserializer& operator>>(deserializer& s, InsertableContainer& val) {
		uint64_t size = 0;
		detail::read_size(s, size);
		while (size--) {
			typename  InsertableContainer::value_type value;
			s >> value;
			val.insert(val.end(), std::move(value));
		}
		return s;
	}

	template<typename T>
	deserializer& operator>>(deserializer& s, std::forward_list<T>& val) {
		uint64_t size = 0;
		detail::read_size(s, size);
		std::vector<T> tmp;
		while (size--) {
			T value;
			s >> value;
			tmp.emplace_back(std::move(value));
		}
		for (auto it = tmp.rbegin(), end = tmp.rend() ; it != end ; ++it) {
			val.emplace_front(std::move(*it));
		}

		return s;
	}

	template<typename T, typename U>
	deserializer& operator>>(deserializer& s, std::pair<T, U>& val) {
		s >> val.first >> val.second;
		return s;
	}

	template<typename T>
	deserializer& operator>>(deserializer& s, std::tuple<T>& val) {
		s >> std::get<0>(val);
		return s;
	}

	template<typename T, typename U>
	deserializer& operator>>(deserializer& s, std::tuple<T, U>& val) {
		s >> std::get<0>(val) << std::get<1>(val);
		return s;
	}

	template<typename T, typename U, typename V>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W, typename X>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W, X>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W, typename X, typename Y>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W, X, Y>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val);
		return s;
	}

	template<typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	deserializer& operator>>(deserializer& s, std::tuple<T, U, V, W, X, Y>& val) {
		s >> std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val) << std::get<6>(val);
		return s;
	}

// deserializing durations
	template<typename T, typename U>
	deserializer& operator>>(deserializer& s, std::chrono::duration<T, U>& val) {
		uint64_t uint64;
		s >> uint64;
		val = std::chrono::duration<T,U>(std::chrono::nanoseconds(uint64));
		return s;
	}
}