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

#include "../serializer.hpp" // Allows my IDE to work

namespace breep { namespace detail {
	template <typename ReturnType, typename FloatType, typename LeastExponentType, typename LeastMantissaType, unsigned int ExponentBits, unsigned int MantissaBits>
	ReturnType toIEEE(FloatType value) {
		static_assert(sizeof(FloatType) == sizeof(ReturnType), "You must have the same number of bits in ReturnType and FloatType");
		LeastExponentType exponentMax = 0;
		for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
			exponentMax = static_cast<LeastExponentType>(exponentMax | 1 << i);
		}

		switch (std::fpclassify(value)) {
			case FP_INFINITE:
				if (value > 0) {
					return ReturnType(exponentMax) << MantissaBits;
				} else {
					return (ReturnType(1) << (MantissaBits + ExponentBits)) | (ReturnType(exponentMax) << MantissaBits);
				}
			case FP_NAN:
				return (ReturnType(exponentMax) << MantissaBits) + 1;
			case FP_ZERO:
				if (value == -0.) {
					return ReturnType(1) << (MantissaBits + ExponentBits);
				} else {
					return 0;
				}
			case FP_SUBNORMAL:
			case FP_NORMAL: {
				int_fast8_t sign_bit = value > 0;
				int_fast16_t shift = 0;
				FloatType inner_val = std::abs(value);

				if (inner_val >= 2) {
					while (inner_val > (2 << 10)) {
						inner_val /= (2 << 10);
						shift = static_cast<int_fast8_t>(shift + 10);
					}
					while (inner_val >= FloatType(2.0)) {
						inner_val /= FloatType(2.);
						shift++;
					}
				} else {
					while (inner_val < FloatType(0.1)) {
						inner_val *= FloatType(10.);
						shift = static_cast<int_fast8_t>(shift - 10);
					}
					while (inner_val < FloatType(1.)) {
						inner_val *= FloatType(2.);
						shift--;
					}
				}
				inner_val -= FloatType(1.0);

				LeastMantissaType mantissa_v = static_cast<LeastMantissaType>(inner_val * ((LeastMantissaType(1) << MantissaBits) + FloatType(.5)));
				LeastExponentType exponent_v = static_cast<LeastExponentType>(shift + (LeastMantissaType(1) << ExponentBits) - 1);

				return (ReturnType(sign_bit) << MantissaBits + ExponentBits) | (ReturnType(exponent_v) << MantissaBits) | ReturnType(mantissa_v);
			}
			default:
				break;
		}
		return 0;
	}

	template <typename SizeType>
	void write_size(serializer& s, SizeType size) {
		uint_fast8_t bits_to_write = 0;
		while (size >> bits_to_write) {
			++bits_to_write;
		}
		uint_fast8_t oct_to_write = static_cast<uint_fast8_t>(bits_to_write / 8 + (bits_to_write % 8 == 0 ? 0 : 1) + 1);
		s << static_cast<uint8_t>(oct_to_write);
		while (oct_to_write--) {
			s << static_cast<uint8_t>(size >> (oct_to_write * 8));
		}
	}
}}

namespace breep {

	serializer& operator<<(serializer& s, bool val) {
		s.m_os.put(val ? '1' : '0');
		return s;
	}

	serializer& operator<<(serializer& s, uint8_t val) {
		s.m_os.put(val);
		return s;
	}

	serializer& operator<<(serializer& s, uint16_t val) {
		s.m_os.put(static_cast<uint8_t>(val >> 8));
		s.m_os.put(static_cast<uint8_t>(val));
		return s;
	}

	serializer& operator<<(serializer& s, uint32_t val) {
		s << static_cast<uint16_t>(val >> 16);
		s << static_cast<uint16_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, uint64_t val) {
		s << static_cast<uint32_t>(val >> 32);
		s << static_cast<uint32_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int8_t val) {
		s << static_cast<uint8_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int16_t val) {
		s << static_cast<uint16_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int32_t val) {
		s << static_cast<uint32_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, int64_t val) {
		s << static_cast<uint64_t>(val);
		return s;
	}

	serializer& operator<<(serializer& s, float val) {
		s << detail::toIEEE<uint32_t, float, uint_fast8_t, uint_fast32_t, 8, 23>(val);
		return s;
	}

	serializer& operator<<(serializer& s, double val) {
		s << detail::toIEEE<uint64_t, double, uint_fast16_t, uint_fast64_t, 11, 52>(val);
		return s;
	}

	template <typename IterableContainer>
	serializer& operator<<(serializer& s, const IterableContainer& val) {
		detail::write_size(s, val.size());
		for (const auto& current_value : val) {
			s << current_value;
		}
		return s;
	}

	template <typename T, typename U>
	serializer& operator<<(serializer& s, const std::pair<T,U>& val) {
		s << val.first << val.second;
		return s;
	}

	template <typename T>
	serializer& operator<<(serializer& s, const std::tuple<T>& val) {
		s << std::get<0>(val);
		return s;
	}

	template <typename T, typename U>
	serializer& operator<<(serializer& s, const std::tuple<T, U>& val) {
		s << std::get<0>(val) << std::get(1)(val);
		return s;
	}

	template <typename T, typename U, typename V>
	serializer& operator<<(serializer& s, const std::tuple<T, U, V>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W, typename X>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W,X>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W, typename X, typename Y>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W,X,Y>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val);
		return s;
	}

	template <typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
	serializer& operator<<(serializer& s, const std::tuple<T,U,V,W,X,Y>& val) {
		s << std::get<0>(val) << std::get<1>(val) << std::get<2>(val) << std::get<3>(val) << std::get<4>(val) << std::get<5>(val) << std::get<6>(val);
		return s;
	}

	template <typename T, typename U>
	serializer& operator<<(serializer& s, const std::chrono::duration<T,U>& val) {
		s << static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(val).count());
		return s;
	};
}
