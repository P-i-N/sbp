#pragma once

#if !defined(SBP_NOEXCEPT)
	#define SBP_NOEXCEPT noexcept
#endif

#if defined(_MSC_VER) && !defined(SBP_MSVC)
	#define SBP_MSVC
#endif

#if !defined(SBP_FORCE_INLINE)
	#if defined(SBP_MSVC)
		#define SBP_FORCE_INLINE __forceinline
	#else
		#define SBP_FORCE_INLINE inline
	#endif
#endif

#if !defined(SBP_EXTENSION)
#define SBP_EXTENSION(_Type, _TypeID) namespace sbp::detail { \
	inline void write( buffer &b, const _Type &value ) { write_ext<sizeof(_Type)>(b, (_TypeID), &value); } \
	inline error read( buffer &b, _Type &value ) { \
		const void* data = nullptr; \
		if (auto err = read_ext<sizeof(_Type), (_TypeID)>(b, data)) return err; \
		memcpy( &value, data, sizeof( _Type ) ); \
		return b.valid(); } \
	}
#endif

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace sbp {

struct error final
{
	enum
	{
		none = 0,
		corrupted_data,
		unexpected_end
	};

	int value = none;

	operator int() const SBP_NOEXCEPT { return value; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class buffer
{
public:
	buffer() SBP_NOEXCEPT
	{
		_data = _stackBuffer;
		_readCursor = _writeCursor = _data;
		_endCap = _data + stack_buffer_capacity;
	}

	~buffer() { reset(); }

	uint8_t *data() SBP_NOEXCEPT { return _data; }

	const uint8_t *data() const SBP_NOEXCEPT { return _data; }

	size_t size() const SBP_NOEXCEPT { return static_cast<size_t>( _writeCursor - _data ); }

	size_t capacity() const SBP_NOEXCEPT { return static_cast<size_t>( _endCap - _data ); }

	error valid() const SBP_NOEXCEPT { return ( _readCursor <= _writeCursor ) ? error() : error{ error::unexpected_end }; }

	void reset( bool freeMemory = true ) SBP_NOEXCEPT;

	void reserve( size_t newCapacity ) SBP_NOEXCEPT;

	void write( const void *data, size_t numBytes ) SBP_NOEXCEPT;

	template <size_t NumBytes> void write( const void *data ) SBP_NOEXCEPT;

	template <typename T> void write( T &&value ) SBP_NOEXCEPT { write<sizeof( T )>( &value ); }

	template <typename T> void write( uint8_t header, T &&value ) SBP_NOEXCEPT;

	void read( void *data, size_t numBytes ) SBP_NOEXCEPT;

	template <typename T> T read() SBP_NOEXCEPT;

	template <typename RT, typename T> error read( T &result ) SBP_NOEXCEPT;

	size_t tell() const SBP_NOEXCEPT { return _readCursor - _data; }

	const void *seek( size_t offset ) SBP_NOEXCEPT;

private:
	void ensure_capacity( size_t NumBytes ) SBP_NOEXCEPT;

	static constexpr size_t stack_buffer_capacity = 256;

	uint8_t *_data = nullptr;

	uint8_t *_writeCursor = nullptr;
	const uint8_t *_readCursor = nullptr;
	const uint8_t *_endCap = nullptr;

	uint8_t _stackBuffer[stack_buffer_capacity] = { };
};

//---------------------------------------------------------------------------------------------------------------------
inline void buffer::reset( bool freeMemory ) SBP_NOEXCEPT
{
	if ( freeMemory )
	{
		if ( _data != _stackBuffer )
			delete[] _data;

		_data = _stackBuffer;
		_endCap = _data + stack_buffer_capacity;
	}

	_readCursor = _writeCursor = _data;
}

//---------------------------------------------------------------------------------------------------------------------
inline void buffer::reserve( size_t newCapacity ) SBP_NOEXCEPT
{
	if ( newCapacity > capacity() )
	{
		auto readOffset = _readCursor - _data;
		auto writeOffset = _writeCursor - _data;

		auto *newData = new uint8_t[newCapacity];
		memcpy( newData, _data, writeOffset );

		if ( _data != _stackBuffer )
			delete[] _data;

		_data = newData;
		_readCursor = _data + readOffset;
		_writeCursor = _data + writeOffset;
		_endCap = _data + newCapacity;
	}
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void buffer::write( const void *data, size_t numBytes ) SBP_NOEXCEPT
{
	ensure_capacity( numBytes );
	memcpy( _writeCursor, data, numBytes );
	_writeCursor += numBytes;
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumBytes>
SBP_FORCE_INLINE void buffer::write( const void *data ) SBP_NOEXCEPT
{
	if constexpr ( NumBytes == 1 )
	{
		if ( _writeCursor == _endCap )
			reserve( capacity() * 2 );

		*_writeCursor++ = *reinterpret_cast<const uint8_t *>( data );
	}
	else
	{
		ensure_capacity( NumBytes );
		memcpy( _writeCursor, data, NumBytes );
		_writeCursor += NumBytes;
	}
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE void buffer::write( uint8_t header, T &&value ) SBP_NOEXCEPT
{
	ensure_capacity( 1 + sizeof( T ) );
	*_writeCursor++ = header;
	memcpy( _writeCursor, &value, sizeof( T ) );
	_writeCursor += sizeof( T );
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void buffer::read( void *data, size_t numBytes ) SBP_NOEXCEPT
{
	if ( _readCursor + numBytes <= _writeCursor )
		memcpy( data, _readCursor, numBytes );

	_readCursor += numBytes;
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE T buffer::read() SBP_NOEXCEPT
{
	if constexpr ( sizeof( T ) == 1 )
		return ( _readCursor < _writeCursor ) ? *reinterpret_cast<const T *>( _readCursor++ ) : T( 0 );

	_readCursor += sizeof( T );
	return ( _readCursor <= _writeCursor ) ? *reinterpret_cast<const T *>( _readCursor - sizeof( T ) ) : T( 0 );
}

//---------------------------------------------------------------------------------------------------------------------
template <typename RT, typename T>
SBP_FORCE_INLINE error buffer::read( T &result ) SBP_NOEXCEPT
{
	if constexpr ( sizeof( T ) < sizeof( RT ) )
		return { error::corrupted_data };

	if ( _readCursor + sizeof( RT ) > _writeCursor )
		return { error::unexpected_end };

	result = static_cast<T>( *reinterpret_cast<const RT *>( _readCursor ) );
	_readCursor += sizeof( RT );
	return { error::none };
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE const void *buffer::seek( size_t offset ) SBP_NOEXCEPT
{
	const void *result = _readCursor;
	_readCursor = _data + offset;
	return result;
}

//---------------------------------------------------------------------------------------------------------------------
void buffer::ensure_capacity( size_t NumBytes ) SBP_NOEXCEPT
{
	if ( _writeCursor + NumBytes > _endCap )
	{
		auto cap1 = size() + NumBytes;
		auto cap2 = capacity() * 3 / 2;

		reserve( ( cap1 > cap2 ) ? cap1 : cap2 );
	}
}

} // namespace sbp

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace sbp::detail {

template <size_t>
struct any { template <typename T> operator T() const; };

template <typename T, typename In, typename = void>
struct has_n_members : std::false_type { };

template <typename T, size_t... In>
struct has_n_members < T, std::index_sequence<In...>, std::void_t < decltype( T { any<In>()... } ) >> :
std::true_type { };

template <typename T, typename In>
struct has_n_members<T &, In> : has_n_members<T, In> { };

template <typename T, typename In>
struct has_n_members < T &&, In > : has_n_members<T, In> { };

template <typename T, size_t N>
constexpr bool has_n_members_v = has_n_members<T, std::make_index_sequence<N>>::value;

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE void write_int( buffer &b, T value ) SBP_NOEXCEPT
{
	if constexpr ( sizeof( T ) == 1 )
	{
		if ( value >= 0 && value <= 127 )
			b.write( value );
		else if ( value < 0 && value >= -32 )
			b.write( value );
		else
			b.write( 0xd0u, value );
	}
	else if constexpr ( sizeof( T ) == 2 )
	{
		if ( value >= 0 && value <= 127 )
			b.write( int8_t( value ) );
		else if ( value < 0 && value >= -32 )
			b.write( int8_t( value ) );
		else if ( value <= 127 && value >= -128 )
			b.write( 0xd0u, int8_t( value ) );
		else
			b.write( 0xd1u, value );
	}
	else if constexpr ( sizeof( T ) == 4 )
	{
		if ( value >= 0 && value <= 127 )
			b.write( int8_t( value ) );
		else if ( value < 0 && value >= -32 )
			b.write( int8_t( value ) );
		else if ( value <= 127 && value >= -128 )
			b.write( 0xd0u, int8_t( value ) );
		else if ( value <= 32767 && value >= -32768 )
			b.write( 0xd1u, int16_t( value ) );
		else
			b.write( 0xd2u, value );
	}
	else
	{
		if ( value >= 0 && value <= 127 )
			b.write( int8_t( value ) );
		else if ( value < 0 && value >= -32 )
			b.write( int8_t( value ) );
		else if ( value <= 127 && value >= -128 )
			b.write( 0xd0u, int8_t( value ) );
		else if ( value <= 32767 && value >= -32768 )
			b.write( 0xd1u, int16_t( value ) );
		else if ( value <= 2147483647ll && value >= -2147483648ll )
			b.write( 0xd2u, int32_t( value ) );
		else
			b.write( 0xd3u, value );
	}
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write( buffer &b, int8_t  value ) SBP_NOEXCEPT { write_int( b, value ); }
SBP_FORCE_INLINE void write( buffer &b, int16_t value ) SBP_NOEXCEPT { write_int( b, value ); }
SBP_FORCE_INLINE void write( buffer &b, int32_t value ) SBP_NOEXCEPT { write_int( b, value ); }
SBP_FORCE_INLINE void write( buffer &b, int64_t value ) SBP_NOEXCEPT { write_int( b, value ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE void write_uint( buffer &b, T value ) SBP_NOEXCEPT
{
	if constexpr ( sizeof( T ) == 1 )
	{
		if ( value <= 127 )
			b.write( value );
		else
			b.write( 0xccu, value );
	}
	else if constexpr ( sizeof( T ) == 2 )
	{
		if ( value <= 127 )
			b.write( uint8_t( value ) );
		else if ( value <= 255 )
			b.write( 0xccu, uint8_t( value ) );
		else
			b.write( 0xcdu, value );
	}
	else if constexpr ( sizeof( T ) == 4 )
	{
		if ( value <= 127 )
			b.write( uint8_t( value ) );
		else if ( value <= 255 )
			b.write( 0xccu, uint8_t( value ) );
		else if ( value <= 65535 )
			b.write( 0xcdu, uint16_t( value ) );
		else
			b.write( 0xceu, value );
	}
	else
	{
		if ( value <= 127 )
			b.write( uint8_t( value ) );
		else if ( value <= 255 )
			b.write( 0xccu, uint8_t( value ) );
		else if ( value <= 65535 )
			b.write( 0xcdu, uint16_t( value ) );
		else if ( value <= 4294967295 )
			b.write( 0xceu, uint32_t( value ) );
		else
			b.write( 0xcfu, value );
	}
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write( buffer &b, uint8_t  value ) SBP_NOEXCEPT { write_uint( b, value ); }
SBP_FORCE_INLINE void write( buffer &b, uint16_t value ) SBP_NOEXCEPT { write_uint( b, value ); }
SBP_FORCE_INLINE void write( buffer &b, uint32_t value ) SBP_NOEXCEPT { write_uint( b, value ); }
SBP_FORCE_INLINE void write( buffer &b, uint64_t value ) SBP_NOEXCEPT { write_uint( b, value ); }

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write_str( buffer &b, const char *value, size_t length ) SBP_NOEXCEPT
{
	if ( length <= 31 )
		b.write( uint8_t( uint8_t( 0b10100000u ) | static_cast<uint8_t>( length ) ) );
	else if ( length <= 255 )
		b.write( 0xd9u, uint8_t( length ) );
	else if ( length <= 65535 )
		b.write( 0xdau, uint16_t( length ) );
	else
		b.write( 0xdbu, uint32_t( length ) );

	b.write( value, length );
}

SBP_FORCE_INLINE void write( buffer &b, const char *value ) SBP_NOEXCEPT { write_str( b, value, value ? ( strlen( value ) + 1 ) : 0 ); }

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write( buffer &b, float value ) SBP_NOEXCEPT { b.write( 0xca, value ); }
SBP_FORCE_INLINE void write( buffer &b, double value ) SBP_NOEXCEPT { b.write( 0xcb, value ); }

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write( buffer &b, bool value ) SBP_NOEXCEPT { b.write( value ? uint8_t( 0xc3u ) : uint8_t( 0xc2u ) ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE void write_array( buffer &b, const T *values, size_t numValues ) SBP_NOEXCEPT
{
	if ( numValues <= 15 )
		b.write( uint8_t( uint8_t( 0b10010000u ) | static_cast<uint8_t>( numValues ) ) );
	else if ( numValues <= 65535 )
		b.write( 0xdcu, uint16_t( numValues ) );
	else
		b.write( 0xddu, uint32_t( numValues ) );

	for ( size_t i = 0; i < numValues; ++i )
		write( b, values[i] );
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumValues, typename T>
SBP_FORCE_INLINE void write_array_fixed( buffer &b, const T *values ) SBP_NOEXCEPT
{
	if constexpr ( NumValues <= 15 )
		b.write( uint8_t( uint8_t( 0b10010000u ) | static_cast<uint8_t>( NumValues ) ) );
	else if constexpr ( NumValues <= 65535 )
		b.write( 0xdcu, uint16_t( NumValues ) );
	else
		b.write( 0xddu, uint32_t( NumValues ) );

	for ( size_t i = 0; i < NumValues; ++i )
		write( b, values[i] );
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, size_t NumValues>
SBP_FORCE_INLINE void write( buffer &b, const T( &value )[NumValues] ) SBP_NOEXCEPT { write_array( b, value, NumValues ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE void write_map( buffer &b, const T &value ) SBP_NOEXCEPT
{
	auto numValues = value.size();

	if ( numValues <= 15 )
		b.write( uint8_t( uint8_t( 0b10000000u ) | static_cast<uint8_t>( numValues ) ) );
	else if ( numValues <= nl<uint16_t>::max() )
		b.write( 0xdeu, uint16_t( numValues ) );
	else
		b.write( 0xdfu, uint32_t( numValues ) );

	for ( const auto & [k, v] : value )
	{
		write( b, k );
		write( b, v );
	}
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write_bin( buffer &b, const void *data, size_t numBytes ) SBP_NOEXCEPT
{
	if ( numBytes <= 255 )
		b.write( 0xc4u, uint8_t( numBytes ) );
	else if ( numBytes <= 65535 )
		b.write( 0xc5u, uint16_t( numBytes ) );
	else
		b.write( 0xc6u, uint32_t( numBytes ) );

	b.write( data, numBytes );
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumBytes>
SBP_FORCE_INLINE void write_ext( buffer &b, int8_t type, const void *data ) SBP_NOEXCEPT
{
	if constexpr ( NumBytes == 1 )
		b.write( 0xd4u, type );
	else if constexpr ( NumBytes == 2 )
		b.write( 0xd5u, type );
	else if constexpr ( NumBytes == 4 )
		b.write( 0xd6u, type );
	else if constexpr ( NumBytes == 8 )
		b.write( 0xd7u, type );
	else if constexpr ( NumBytes == 16 )
		b.write( 0xd8u, type );
	else if constexpr ( NumBytes <= 255 )
	{
		b.write( 0xc7u, uint8_t( NumBytes ) );
		b.write( type );
	}
	else if constexpr ( NumBytes <= 65535 )
	{
		b.write( 0xc8u, uint16_t( NumBytes ) );
		b.write( type );
	}
	else
	{
		b.write( 0xc9u, uint32_t( NumBytes ) );
		b.write( type );
	}

	b.write( data, NumBytes );
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write_ext( buffer &b, int8_t type, const void *data, size_t numBytes ) SBP_NOEXCEPT
{
	if ( numBytes == 1 )
		b.write( 0xd4u, type );
	else if ( numBytes == 2 )
		b.write( 0xd5u, type );
	else if ( numBytes == 4 )
		b.write( 0xd6u, type );
	else if ( numBytes == 8 )
		b.write( 0xd7u, type );
	else if ( numBytes == 16 )
		b.write( 0xd8u, type );
	else if ( numBytes <= 255 )
	{
		b.write( 0xc7u, uint8_t( numBytes ) );
		b.write( type );
	}
	else if ( numBytes <= 65535 )
	{
		b.write( 0xc8u, uint16_t( numBytes ) );
		b.write( type );
	}
	else
	{
		b.write( 0xc9u, uint32_t( numBytes ) );
		b.write( type );
	}

	b.write( data, numBytes );
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename... Tail>
void write_multiple( buffer &b, const T &value, const Tail &... tail ) SBP_NOEXCEPT
{
	if constexpr ( std::is_enum_v<T> )
		write( b, std::underlying_type_t<T>( value ) );
	else
		write( b, value );

	if constexpr ( sizeof...( Tail ) > 0 )
		write_multiple( b, tail... );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE error read_int( buffer &b, T &value ) SBP_NOEXCEPT
{
	if ( auto header = b.read<uint8_t>(); ( header & 0b10000000u ) == 0 )
	{
		value = static_cast<T>( header );
		return b.valid();
	}
	else if ( ( header & 0b11100000u ) == 0b11100000u )
	{
		value = static_cast<T>( static_cast<int8_t>( header ) );
		return b.valid();
	}
	else if ( header == 0xd0u )
		return b.read<int8_t>( value );
	else if ( header == 0xd1u )
		return b.read<int16_t>( value );
	else if ( header == 0xd2u )
		return b.read<int32_t>( value );
	else if ( header == 0xd3u )
		return b.read<int64_t>( value );

	return { error::corrupted_data };
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, int8_t  &value ) SBP_NOEXCEPT { return read_int( b, value ); }
SBP_FORCE_INLINE error read( buffer &b, int16_t &value ) SBP_NOEXCEPT { return read_int( b, value ); }
SBP_FORCE_INLINE error read( buffer &b, int32_t &value ) SBP_NOEXCEPT { return read_int( b, value ); }
SBP_FORCE_INLINE error read( buffer &b, int64_t &value ) SBP_NOEXCEPT { return read_int( b, value ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
SBP_FORCE_INLINE error read_uint( buffer &b, T &value ) SBP_NOEXCEPT
{
	if ( auto header = b.read<uint8_t>(); ( header & 0b10000000u ) == 0 )
	{
		value = header;
		return b.valid();
	}
	else if ( header == 0xccu )
		return b.read<uint8_t>( value );
	else if ( header == 0xcdu )
		return b.read<uint16_t>( value );
	else if ( header == 0xceu )
		return b.read<uint32_t>( value );
	else if ( header == 0xcfu )
		return b.read<uint64_t>( value );

	return { error::corrupted_data };
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, uint8_t  &value ) SBP_NOEXCEPT { return read_uint( b, value ); }
SBP_FORCE_INLINE error read( buffer &b, uint16_t &value ) SBP_NOEXCEPT { return read_uint( b, value ); }
SBP_FORCE_INLINE error read( buffer &b, uint32_t &value ) SBP_NOEXCEPT { return read_uint( b, value ); }
SBP_FORCE_INLINE error read( buffer &b, uint64_t &value ) SBP_NOEXCEPT { return read_uint( b, value ); }

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read_string_length( buffer &b, size_t &value ) SBP_NOEXCEPT
{
	if ( auto header = b.read<uint8_t>(); ( header & 0b11100000u ) == 0b10100000u )
	{
		value = header & 0b00011111u;
		return b.valid();
	}
	else if ( header == 0xd9u )
		return b.read<uint8_t>( value );
	else if ( header == 0xdau )
		return b.read<uint16_t>( value );
	else if ( header == 0xdbu )
		return b.read<uint32_t>( value );

	return { error::corrupted_data };
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, const char *&value ) SBP_NOEXCEPT
{
	size_t length = 0;
	if ( auto err = read_string_length( b, length ) )
		return err;

	value = reinterpret_cast<const char *>( b.seek( b.tell() + length ) );
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, float &value ) SBP_NOEXCEPT
{
	if ( b.read<uint8_t>() != 0xcau )
		return { error::corrupted_data };

	return b.read<float>( value );
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, double &value ) SBP_NOEXCEPT
{
	if ( b.read<uint8_t>() != 0xcbu )
		return { error::corrupted_data };

	return b.read<double>( value );
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, bool &value ) SBP_NOEXCEPT
{
	if ( auto header = b.read<uint8_t>(); header == 0xc2u || header == 0xc3u )
	{
		value = ( header == 0xc3u );
		return { error::none };
	}

	return { error::corrupted_data };
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read_array_length( buffer &b, size_t &value ) SBP_NOEXCEPT
{
	if ( auto header = b.read<uint8_t>(); ( header & 0b11110000u ) == 0b10010000u )
	{
		value = header & 0b00001111u;
		return { error::none };
	}
	else if ( header == 0xdcu )
		return b.read<uint16_t>( value );
	else if ( header == 0xddu )
		return b.read<uint32_t>( value );

	return { error::corrupted_data };
}

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read_map_length( buffer &b, size_t &value ) SBP_NOEXCEPT
{
	if ( auto header = b.read<uint8_t>(); ( header & 0b11110000u ) == 0b10000000u )
	{
		value = header & 0b00001111u;
		return { error::none };
	}
	else if ( header == 0xdeu )
		return b.read<uint16_t>( value );
	else if ( header == 0xdfu )
		return b.read<uint32_t>( value );

	return { error::corrupted_data };
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename KeyType, typename ValueType>
SBP_FORCE_INLINE error read_map( buffer &b, T &value ) SBP_NOEXCEPT
{
	size_t numValues = 0;
	if ( auto err = read_map_length( b, numValues ) )
		return err;

	value.clear();
	for ( size_t i = 0; i < numValues; ++i )
	{
		KeyType k;
		if ( auto err = read( b, k ) )
			return err;

		ValueType v;
		if ( auto err = read( b, v ) )
			return err;

		value[std::move( k )] = std::move( v );
	}

	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <size_t NumBytes, int8_t TypeID = 0>
SBP_FORCE_INLINE error read_ext( buffer &b, const void *&value ) SBP_NOEXCEPT
{
	auto header = b.read<uint8_t>();
	if constexpr ( NumBytes == 1 )
	{
		if ( header != 0xd4u )
			return { error::corrupted_data };
	}
	else if constexpr ( NumBytes == 2 )
	{
		if ( header != 0xd5u )
			return { error::corrupted_data };
	}
	else if constexpr ( NumBytes == 4 )
	{
		if ( header != 0xd6u )
			return { error::corrupted_data };
	}
	else if constexpr ( NumBytes == 8 )
	{
		if ( header != 0xd7u )
			return { error::corrupted_data };
	}
	else if constexpr ( NumBytes == 16 )
	{
		if ( header != 0xd8u )
			return { error::corrupted_data };
	}
	else if constexpr ( NumBytes <= 255 )
	{
		if ( header != 0xc7u || b.read<uint8_t>() != NumBytes )
			return { error::corrupted_data };
	}
	else if constexpr ( NumBytes <= 65535 )
	{
		if ( header != 0xc8u || b.read<uint16_t>() != NumBytes )
			return { error::corrupted_data };
	}
	else
	{
		if ( header != 0xc9u || b.read<uint32_t>() != NumBytes )
			return { error::corrupted_data };
	}

	if ( b.read<int8_t>() != TypeID )
		return { error::corrupted_data };

	value = b.seek( b.tell() + NumBytes );
	return b.valid();
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename... Tail>
error read_multiple( buffer &b, T &value, Tail &... tail ) SBP_NOEXCEPT
{
	error err;

	if constexpr ( std::is_enum_v<T> )
		err = read( b, std::underlying_type_t<T>( value ) );
	else
		err = read( b, value );

	if constexpr ( sizeof...( Tail ) > 0 )
	{
		if ( auto err = read_multiple( b, tail... ) )
			return err;
	}

	return err;
}

} // namespace sbp::detail

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(SBP_MSVC)
	#if defined(_ARRAY_)
		#define SBP_STL_ARRAY
	#endif

	#if defined(_MAP_)
		#define SBP_STL_MAP
	#endif

	#if defined(_STRING_)
		#define SBP_STL_STRING
	#endif

	#if defined(_STRING_VIEW_)
		#define SBP_STL_STRING_VIEW
	#endif

	#if defined(_UNORDERED_MAP_)
		#define SBP_STL_UNORDERED_MAP
	#endif

	#if defined(_VECTOR_)
		#define SBP_STL_VECTOR
	#endif
#endif

namespace sbp::detail {

#if defined(SBP_STL_ARRAY)
//---------------------------------------------------------------------------------------------------------------------
template <typename T, size_t NumValues>
SBP_FORCE_INLINE void write( buffer &b, const std::array<T, NumValues> &value ) SBP_NOEXCEPT { write_array( b, value.data(), NumValues ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T, size_t NumValues>
SBP_FORCE_INLINE error read( buffer &b, std::array<T, NumValues> &value ) SBP_NOEXCEPT
{
	size_t numValues = 0;
	if ( auto err = read_array_length( b, numValues ) )
		return err;

	if ( numValues != NumValues )
		return { error::corrupted_data };

	for ( size_t i = 0; i < NumValues; ++i )
	{
		if ( auto err = read( b, value[i] ) )
			return err;
	}

	return b.valid();
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(SBP_STL_MAP)
//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename P, typename A>
SBP_FORCE_INLINE void write( buffer &b, const std::map<K, T, P, A> &value ) SBP_NOEXCEPT { write_map( b, value ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename P, typename A>
SBP_FORCE_INLINE error read( buffer &b, std::map<K, T, P, A> &value ) SBP_NOEXCEPT
{
	using Type = std::remove_reference_t<decltype( value )>;
	return read_map<Type, typename Type::key_type, typename Type::mapped_type>( b, value );
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(SBP_STL_UNORDERED_MAP)
//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename H, typename EQ, typename A>
SBP_FORCE_INLINE void write( buffer &b, const std::unordered_map<K, T, H, EQ, A> &value ) SBP_NOEXCEPT { write_map( b, value ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename K, typename T, typename H, typename EQ, typename A>
SBP_FORCE_INLINE error read( buffer &b, std::unordered_map<K, T, H, EQ, A> &value ) SBP_NOEXCEPT { return read_map( b, value ); }
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(SBP_STL_STRING)
//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write( buffer &b, const std::string &value ) SBP_NOEXCEPT { write_str( b, value.c_str(), value.length() ); }

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, std::string &value ) SBP_NOEXCEPT
{
	size_t length = 0;
	if ( auto err = read_string_length( b, length ) )
		return err;

	value.resize( length );
	b.read( value.data(), length );
	return b.valid();
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(SBP_STL_STRING_VIEW)
//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE void write( buffer &b, std::string_view value ) SBP_NOEXCEPT { write_str( b, value.data(), value.length() ); }

//---------------------------------------------------------------------------------------------------------------------
SBP_FORCE_INLINE error read( buffer &b, std::string_view &value ) SBP_NOEXCEPT
{
	size_t length = 0;
	if ( auto err = read_string_length( b, length ) )
		return err;

	value = std::string_view( reinterpret_cast<const char *>( b.seek( b.tell() + length ) ), length );
	return b.valid();
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(SBP_STL_VECTOR)
//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename A>
SBP_FORCE_INLINE void write( buffer &b, const std::vector<T, A> &value ) SBP_NOEXCEPT { write_array( b, value.data(), value.size() ); }

//---------------------------------------------------------------------------------------------------------------------
template <typename T, typename A>
SBP_FORCE_INLINE error read( buffer &b, std::vector<T, A> &value ) SBP_NOEXCEPT
{
	size_t numValues = 0;
	if ( auto err = read_array_length( b, numValues ) )
		return err;

	value.clear();
	value.reserve( value.size() + numValues );
	for ( size_t i = 0; i < numValues; ++i )
	{
		if ( auto err = read( b, value.emplace_back() ) )
			return err;
	}

	return b.valid();
}
#endif

} // namespace sbp::detail

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace sbp {

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void write( buffer &b, const T &msg ) SBP_NOEXCEPT
{
	if constexpr ( detail::has_n_members_v<T, 10> )
	{
		const auto &[m0, m1, m2, m3, m4, m5, m6, m7, m8, m9] = msg;
		detail::write_multiple( b, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9 );
	}
	else if constexpr ( detail::has_n_members_v<T, 9> )
	{
		const auto &[m0, m1, m2, m3, m4, m5, m6, m7, m8] = msg;
		detail::write_multiple( b, m0, m1, m2, m3, m4, m5, m6, m7, m8 );
	}
	else if constexpr ( detail::has_n_members_v<T, 8> )
	{
		const auto &[m0, m1, m2, m3, m4, m5, m6, m7] = msg;
		detail::write_multiple( b, m0, m1, m2, m3, m4, m5, m6, m7 );
	}
	else if constexpr ( detail::has_n_members_v<T, 7> )
	{
		const auto &[m0, m1, m2, m3, m4, m5, m6] = msg;
		detail::write_multiple( b, m0, m1, m2, m3, m4, m5, m6 );
	}
	else if constexpr ( detail::has_n_members_v<T, 6> )
	{
		const auto &[m0, m1, m2, m3, m4, m5] = msg;
		detail::write_multiple( b, m0, m1, m2, m3, m4, m5 );
	}
	else if constexpr ( detail::has_n_members_v<T, 5> )
	{
		const auto &[m0, m1, m2, m3, m4] = msg;
		detail::write_multiple( b, m0, m1, m2, m3, m4 );
	}
	else if constexpr ( detail::has_n_members_v<T, 4> )
	{
		const auto &[m0, m1, m2, m3] = msg;
		detail::write_multiple( b, m0, m1, m2, m3 );
	}
	else if constexpr ( detail::has_n_members_v<T, 3> )
	{
		const auto &[m0, m1, m2] = msg;
		detail::write_multiple( b, m0, m1, m2 );
	}
	else if constexpr ( detail::has_n_members_v<T, 2> )
	{
		const auto &[m0, m1] = msg;
		detail::write_multiple( b, m0, m1 );
	}
	else if constexpr ( detail::has_n_members_v<T, 1> )
	{
		const auto &[m0] = msg;
		detail::write_multiple( b, m0 );
	}
}

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
error read( buffer &b, T &msg ) SBP_NOEXCEPT
{
	if constexpr ( detail::has_n_members_v<T, 10> )
	{
		auto &[m0, m1, m2, m3, m4, m5, m6, m7, m8, m9] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3, m4, m5, m6, m7, m8, m9 );
	}
	else if constexpr ( detail::has_n_members_v<T, 9> )
	{
		auto &[m0, m1, m2, m3, m4, m5, m6, m7, m8] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3, m4, m5, m6, m7, m8 );
	}
	else if constexpr ( detail::has_n_members_v<T, 8> )
	{
		auto &[m0, m1, m2, m3, m4, m5, m6, m7] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3, m4, m5, m6, m7 );
	}
	else if constexpr ( detail::has_n_members_v<T, 7> )
	{
		auto &[m0, m1, m2, m3, m4, m5, m6] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3, m4, m5, m6 );
	}
	else if constexpr ( detail::has_n_members_v<T, 6> )
	{
		auto &[m0, m1, m2, m3, m4, m5] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3, m4, m5 );
	}
	else if constexpr ( detail::has_n_members_v<T, 5> )
	{
		auto &[m0, m1, m2, m3, m4] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3, m4 );
	}
	else if constexpr ( detail::has_n_members_v<T, 4> )
	{
		auto &[m0, m1, m2, m3] = msg;
		return detail::read_multiple( b, m0, m1, m2, m3 );
	}
	else if constexpr ( detail::has_n_members_v<T, 3> )
	{
		auto &[m0, m1, m2] = msg;
		return detail::read_multiple( b, m0, m1, m2 );
	}
	else if constexpr ( detail::has_n_members_v<T, 2> )
	{
		auto &[m0, m1] = msg;
		return detail::read_multiple( b, m0, m1 );
	}
	else if constexpr ( detail::has_n_members_v<T, 1> )
	{
		auto &[m0] = msg;
		return detail::read_multiple( b, m0 );
	}

	return b.valid();
}

} // namespace sbp
