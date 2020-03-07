#include <sbp/sbp.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <iostream>

//---------------------------------------------------------------------------------------------------------------------
void PrintBuffer(const sbp::buffer& buff)
{
	const size_t bytesPerLine = 16;

	size_t offset = 0;
	const auto* cursor = reinterpret_cast<const uint8_t*>(buff.data());

	while (offset < buff.size())
	{
		printf("0x%08hhx | ", static_cast<int>(offset));

		size_t lineLength = (offset + bytesPerLine <= buff.size())
			? bytesPerLine
			: (buff.size() - offset);
		
		for (size_t i = 0; i < lineLength; ++i)
			printf("%02hhx ", cursor[i]);

		for (size_t i = 0; i < bytesPerLine - lineLength; ++i)
			printf("   ");

		printf("| ");

		for (size_t i = 0; i < lineLength; ++i)
		{
			auto b = cursor[i];
			if (b >= 32 && b < 128)
				printf("%c", b);
			else
				printf(".");
		}

		printf("\n");
		offset += bytesPerLine;
		cursor += bytesPerLine;
	}
}

//---------------------------------------------------------------------------------------------------------------------
struct Stopwatch
{
	const char* name = "";
	std::chrono::nanoseconds start = std::chrono::high_resolution_clock::now().time_since_epoch();

	~Stopwatch()
	{
		auto duration = std::chrono::high_resolution_clock::now().time_since_epoch() - start;
		std::cout << name << ": " << duration.count() / 1000000 << " ms" << std::endl;
	}
};

//---------------------------------------------------------------------------------------------------------------------
template <typename T>
void TestWriteReadPerformance(std::string_view text, sbp::buffer& b, size_t cycles, size_t opsPerCycle)
{
	T msg;

	// Write
	{
		std::string str = std::string(text) + " W";
		Stopwatch sw{ str.c_str() };

		for (size_t j = 0; j < 10; ++j)
		{
			b.reset(false);
			for (size_t i = 0; i < 1000000; ++i) sbp::write(b, msg);
		}
	}

	// Read
	{
		std::string str = std::string(text) + " R";
		Stopwatch sw{ str.c_str() };

		bool error = false;
		for (size_t j = 0; j < 10 && !error; ++j)
		{
			b.reset_read_pos();
			for (size_t i = 0; i < 1000000; ++i)
			{
				if (sbp::read(b, msg) != sbp::error::none)
				{
					error = true;
					break;
				}
			}
		}

		if (error)
			std::cout << "deserialization error!" << std::endl;
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TestPerformance()
{
	sbp::buffer buffer;
	buffer.reserve(1024 * 1024 * 128); // Reserve 128 MB

	std::cout << "write/read 10 milion messages:" << std::endl;

	/// Message with int
	{
		struct Message final
		{
			int value = 1234567890;
		};

		TestWriteReadPerformance<Message>("    int", buffer, 10, 1000000);
	}

	/// Complex message
	{
		struct Message final
		{
			int age = 32;
			float height = 1.75f;
			std::string name = "Someone Unknown";
			const char* c_str = "Will this work?!";
			uint64_t password_hash = 12345;
			std::array<int, 5> lucky_numbers = { 1, 2, 3, 4, 5 };
		};

		TestWriteReadPerformance<Message>("complex", buffer, 10, 1000000);
	}

	/// Extensions
	{
		struct Matrix3x3
		{
			float m[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		};

		struct Message final
		{
			sbp::ext<Matrix3x3> matrix;
		};

		TestWriteReadPerformance<Message>("    ext", buffer, 10, 1000000);
	}
}

//---------------------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	TestPerformance();
	return 0;
}
