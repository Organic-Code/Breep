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
 * @file serializer.cpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <cstdint>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <bitset>
#include <type_traits>
#include <cassert>


#include "breep/util/tuple_iter.hpp"
#include "../serializer.hpp" // Allows my IDE to work

namespace breep { namespace detail {
	// The code from the following method was adapted from the public domain
	template <typename ReturnType, typename FloatType, unsigned int ExponentBits, unsigned int MantissaBits>
	constexpr ReturnType toIEEE(FloatType value) {

		switch (std::fpclassify(value)) {
			case FP_INFINITE: {
				ReturnType exponentMax(0);
				for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
					exponentMax = (exponentMax << 1) + 1;
				}
				if (value > 0) {
					return exponentMax << MantissaBits;
				} else {
					return (ReturnType(1) << (MantissaBits + ExponentBits)) | (exponentMax << MantissaBits);
				}
			}
			case FP_NAN: {
				ReturnType exponentMax(0);
				for (uint_fast8_t i = 0 ; i < ExponentBits ; ++i) {
					exponentMax = (exponentMax << 1) + 1;
				}
				return (exponentMax << MantissaBits) | 1;
			}
			case FP_NORMAL:
			case FP_SUBNORMAL: {
				FloatType fnorm;
				ReturnType shift;
				ReturnType sign, exp, mantissa;

				if (value < 0) {
					sign = 1;
					fnorm = -value;
				} else {
					sign = 0;
					fnorm = value;
				}

				// get the normalized form of f and track the exponent
				shift = 0;

				if (fnorm >= FloatType(2.)) {
                    for (uint_fast8_t i = std::numeric_limits<ReturnType>::digits ; --i ;) {
                        while(fnorm >= FloatType(ReturnType(1) << i)) {
                            fnorm /= FloatType(ReturnType(1) << i);
                            shift += i;
                        }
                    }
				} else {
                    for (uint_fast8_t i = std::numeric_limits<ReturnType>::digits ; i-- ;) {
                        while(fnorm < FloatType(1) / FloatType(ReturnType(1) << i)) {
                            fnorm *= FloatType(ReturnType(2) << i);
                            shift -= (i + 1);
                        }
                    }
				}
				fnorm = fnorm - FloatType(1.);

				mantissa = static_cast<ReturnType>(fnorm * ((ReturnType(1) << MantissaBits) + FloatType(.5)));

				exp = static_cast<ReturnType>(shift + ((ReturnType(1) << (ExponentBits - 1)) - 1));

				return static_cast<ReturnType>((sign << (MantissaBits + ExponentBits)) | (exp << MantissaBits) | mantissa);
			}
			case FP_ZERO:
			default: {
				return std::signbit(value) ? ReturnType(1) << (ExponentBits + MantissaBits) : 0;
			}
		}
	}
}}

namespace breep {

	template <typename SizeType>
	void write_size(serializer& s, SizeType size) {
		static_assert(std::is_fundamental<SizeType>::value && std::is_integral<SizeType>::value, "SizeType must be an integer.");

		uint_fast8_t bits_to_write = 0;
		while (size >> bits_to_write) {
			++bits_to_write;
		}
		auto oct_to_write = static_cast<uint_fast8_t>(bits_to_write / 8 + (bits_to_write % 8 == 0 ? 0 : 1));
		s << static_cast<uint8_t>(oct_to_write);
		while (oct_to_write--) {
			s << static_cast<uint8_t>(size >> (oct_to_write * 8));
		}
	}

namespace detail { // breep::detail
	inline serializer& left_shift_op_impl(serializer&, uint8_t);
	inline serializer& left_shift_op_impl(serializer&, uint16_t);
	inline serializer& left_shift_op_impl(serializer&, uint32_t);
	inline serializer& left_shift_op_impl(serializer&, uint64_t);
	inline serializer& left_shift_op_impl(serializer&, int8_t);
	inline serializer& left_shift_op_impl(serializer&, int16_t);
	inline serializer& left_shift_op_impl(serializer&, int32_t);
	inline serializer& left_shift_op_impl(serializer&, int64_t);
	inline serializer& left_shift_op_impl(serializer&, bool);
	inline serializer& left_shift_op_impl(serializer&, char);
	inline serializer& left_shift_op_impl(serializer&, float);
	template <typename T> std::enable_if_t<!breep::type_traits<T>::is_enum, serializer&> left_shift_op_impl(serializer&, const T&);
	serializer& left_shift_op_impl(serializer&, const std::vector<bool>&);
	template <typename T> serializer& left_shift_op_impl(serializer&, const std::forward_list<T>&);
	template <typename T, typename U> serializer& left_shift_op_impl(serializer&, const std::pair<T,U>&);
	template <typename... T> serializer& left_shift_op_impl(serializer&, const std::tuple<T...>&);
	template <typename T> std::enable_if_t<breep::type_traits<T>::is_enum_class, serializer&> left_shift_op_impl(serializer&, T);
	template <typename T> std::enable_if_t<breep::type_traits<T>::is_enum_plain, serializer&> left_shift_op_impl(serializer&, T&&);

	inline serializer& left_shift_op_impl(serializer& s, uint8_t val) {
		s.m_os.put(val);
		return s;
	}

	inline serializer& left_shift_op_impl(serializer& s, uint16_t val) {
		left_shift_op_impl(s, static_cast<uint8_t>(val >> 8));
		left_shift_op_impl(s, static_cast<uint8_t>(val));
		return s;
	}

	inline serializer& left_shift_op_impl(serializer& s, uint32_t val) {
		left_shift_op_impl(s, static_cast<uint16_t>(val >> 16));
		left_shift_op_impl(s, static_cast<uint16_t>(val));
		return s;
	}

	inline serializer& left_shift_op_impl(serializer& s, uint64_t val) {
		left_shift_op_impl(s, static_cast<uint32_t>(val >> 32));
		left_shift_op_impl(s, static_cast<uint32_t>(val));
		return s;
	}

	inline serializer& left_shift_op_impl(serializer& s, int8_t val) {
		return left_shift_op_impl(s, static_cast<uint8_t>(val));
	}

	inline serializer& left_shift_op_impl(serializer& s, int16_t val) {
		return left_shift_op_impl(s, static_cast<uint16_t>(val));
	}

	inline serializer& left_shift_op_impl(serializer& s, int32_t val) {
		return left_shift_op_impl(s, static_cast<uint32_t>(val));
	}

	inline serializer& left_shift_op_impl(serializer& s, int64_t val) {
		return left_shift_op_impl(s, static_cast<uint64_t>(val));
	}

	inline serializer& left_shift_op_impl(serializer& s, bool val) {
		left_shift_op_impl(s, (val ? static_cast<uint8_t>('1') : static_cast<uint8_t>('0')));
		return s;
	}

	inline serializer& left_shift_op_impl(serializer& s, char val) {
		if (std::numeric_limits<char>::is_signed) {
			auto unsigned_value = static_cast<signed char>(val);
			left_shift_op_impl(s, unsigned_value);
		} else {
			auto signed_value = static_cast<unsigned char>(val);
			left_shift_op_impl(s, signed_value);
		}
		return s;
	}

	inline serializer& left_shift_op_impl(serializer& s, float val) {
		return left_shift_op_impl(s, detail::toIEEE<int32_t, float, 8, 23>(val));
	}

	inline serializer& left_shift_op_impl(serializer& s, double val) {
		left_shift_op_impl(s, detail::toIEEE<int64_t, double, 11, 52>(val));
		return s;
	}

	template <typename IterableContainer>
	std::enable_if_t<!breep::type_traits<IterableContainer>::is_enum, serializer&>
	left_shift_op_impl(serializer& s, const IterableContainer& val) {
		static_assert(!std::is_pointer<IterableContainer>::value, "failed to dereference IterableContainer");

		write_size(s, val.size());
		for (const auto& current_value : val) {
			s << current_value;
		}
		return s;
	}

    inline serializer& left_shift_op_impl(serializer& s, const std::vector<bool>& vect) {
        write_size(s, vect.size());
        for (std::vector<bool>::size_type i = 0 ; i < vect.size() ; i += std::numeric_limits<uint8_t>::digits) {
            uint8_t mask = 0;
            for (uint_fast8_t j = 0 ; j < std::numeric_limits<uint8_t>::digits && i+j < vect.size() ; ++j) {
                mask = static_cast<uint8_t>(mask | (vect[i + j] ? 1 << j : 0));
            }
            left_shift_op_impl(s, mask);
        }
        return s;
    }

	template <typename T>
	serializer& left_shift_op_impl(serializer& s, const std::forward_list<T>& forward_list) {
		uint64_t size = 0;
		for (auto it = forward_list.begin(), end = forward_list.end() ; it != end ; ++it, ++size);
		write_size(s, size);
		for (const T& current_value : forward_list) {
			s << current_value;
		}
		return s;
	}

	template <typename T, typename U>
	serializer& left_shift_op_impl(serializer& s, const std::pair<T,U>& val) {
		s << val.first << val.second;
		return s;
	}

	template <typename... T>
	serializer& left_shift_op_impl(serializer& s, const std::tuple<T...>& values) {
        for_each(values, [&s](auto&& val) -> void {
                    s << val;
                });
		return s;
	}

	template <typename T>
	std::enable_if_t<breep::type_traits<T>::is_enum_class, serializer&>
	left_shift_op_impl(serializer& s, T value) {
		return s << static_cast<std::underlying_type_t<T>>(value);
	}

	template <typename T>
	std::enable_if_t<breep::type_traits<T>::is_enum_plain, serializer&>
    left_shift_op_impl(serializer& s, T&& value) {
//    	 std::underlying_type is implementation defined for plain enums
		return s << static_cast<int64_t>(value);
    }


} // namespace detail


	template <typename T>
	std::enable_if_t<!type_traits<T>::is_any_ptr, serializer&>
	operator<<(serializer& s, T&& value) {
		return detail::left_shift_op_impl(s, value);
	}

	template <typename T>
	std::enable_if_t<type_traits<T>::is_any_ptr, serializer&>
	operator<<(serializer& s, T&& value) {
		assert(value);
		return s << *value;
	}

	template <typename T>
	serializer& operator<<(serializer& s, const std::unique_ptr<T>& value) {
		return s << value.get();
	}

	template <typename T>
	serializer& operator<<(serializer& s, const std::shared_ptr<T>& value) {
		return s << value.get();
	}



} // namespace breep
