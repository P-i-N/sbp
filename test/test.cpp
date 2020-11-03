#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <map>
#include <unordered_map>

#include <sbp/sbp.hpp>

//---------------------------------------------------------------------------------------------------------------------
void PrintBuffer( const sbp::buffer &buff, size_t size = size_t( -1 ) )
{
	const size_t bytesPerLine = 16;

	if ( size > buff.size() )
		size = buff.size();

	size_t offset = 0;
	const auto *cursor = reinterpret_cast<const uint8_t *>( buff.data() );

	while ( offset < size )
	{
		printf( "0x%08hhx | ", static_cast<int>( offset ) );

		size_t lineLength = ( offset + bytesPerLine <= size )
		                    ? bytesPerLine
		                    : ( size - offset );

		for ( size_t i = 0; i < lineLength; ++i )
			printf( "%02hhx ", cursor[i] );

		for ( size_t i = 0; i < bytesPerLine - lineLength; ++i )
			printf( "   " );

		printf( "| " );

		for ( size_t i = 0; i < lineLength; ++i )
		{
			auto b = cursor[i];
			if ( b >= 32 && b < 128 )
				printf( "%c", b );
			else
				printf( "." );
		}

		printf( "\n" );
		offset += bytesPerLine;
		cursor += bytesPerLine;
	}
}

//---------------------------------------------------------------------------------------------------------------------
struct Stopwatch
{
	const char *name = "";
	std::chrono::nanoseconds start = std::chrono::high_resolution_clock::now().time_since_epoch();

	~Stopwatch()
	{
		auto duration = std::chrono::high_resolution_clock::now().time_since_epoch() - start;
		std::cout << name << ": " << duration.count() / 1000000 << " ms" << std::endl;
	}
};

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void TestWriteReadPerformance( std::string_view text, sbp::buffer &b, size_t cycles, size_t opsPerCycle )
{
	T msg;

	// Print buffer
	{
		b.reset( false );
		sbp::write( b, msg );
		PrintBuffer( b );
	}

	// Write
	{
		std::string str = std::string( text ) + " W";
		Stopwatch sw{ str.c_str() };

		for ( size_t j = 0; j < cycles; ++j )
		{
			b.reset( false );
			for ( size_t i = 0; i < opsPerCycle; ++i ) sbp::write( b, msg );
		}
	}

	// Read
	{
		std::string str = std::string( text ) + " R";
		Stopwatch sw{ str.c_str() };

		bool error = false;
		for ( size_t j = 0; j < cycles && !error; ++j )
		{
			b.seek( 0 );
			for ( size_t i = 0; i < opsPerCycle; ++i )
			{
				if ( sbp::read( b, msg ) != sbp::error::none )
				{
					error = true;
					break;
				}
			}
		}

		if ( error )
			std::cout << "deserialization error!" << std::endl;
	}
}

struct Matrix3x3
{
	float m[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
};

SBP_EXTENSION( Matrix3x3, 0 )

//---------------------------------------------------------------------------------------------------------------------
void TestPerformance()
{
	size_t cycles = 100;
	size_t opsPerCycle = 1000000;

	sbp::buffer buffer;
	buffer.reserve( 1024 * 1024 * 128 ); // Reserve 128 MB

	std::cout << "write/read " << ( cycles * opsPerCycle ) << " messages:" << std::endl;

	/// Message with int
	{
		struct Message final
		{
			int value = 1234567890;
		};

		TestWriteReadPerformance<Message>( "    int", buffer, cycles, opsPerCycle );
	}

	/// Complex message
	{
		struct Message final
		{
			int age = 32;
			float height = 1.75f;
			std::string name = "Someone Unknown";
			const char *c_str = "Will this work?!";
			uint64_t password_hash = 12345;
			std::array<int, 5> lucky_numbers = { 1, 2, 3, 4, 5 };
		};

		TestWriteReadPerformance<Message>( "complex", buffer, cycles, opsPerCycle );
	}

	/// Extensions
	{
		struct Message final
		{
			Matrix3x3 matrix;
		};

		TestWriteReadPerformance<Message>( "    ext", buffer, cycles, opsPerCycle );
	}
}

//---------------------------------------------------------------------------------------------------------------------
int main( int argc, char *argv[] )
{
	TestPerformance();
	return 0;
}
