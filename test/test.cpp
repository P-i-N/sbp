#include <sbp/sbp.hpp>

#include <cassert>

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TestStruct final
{
	std::map<int, std::string> map;
};

//---------------------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	sbp::buffer buffer;

	TestStruct ts;
	ts.map[0] = "Zero";
	ts.map[1] = "One";
	ts.map[2] = "Two";
	ts.map[3] = "Three";
	ts.map[4] = "Four";
	ts.map[5] = "Five";
	ts.map[6] = "Six";

	sbp::write(buffer, ts);

	TestStruct ts2;
	sbp::read(buffer, ts2);

	{
		struct UserData final
		{
			int age;
			float height;
			std::string name;
			uint64_t password_hash;
			std::vector<int> lucky_numbers;
		};

		// Fill UserData struct with some random data
		UserData ud;
		ud.age = 32;
		ud.height = 1.75f;
		ud.name = "Jeff";
		ud.password_hash = 0xDEADBEEFDEADC0DEull;
		ud.lucky_numbers = { 69, 420, 1984 };

		// Serialize it into a buffer
		sbp::buffer buff;
		sbp::write(buff, ud);

		PrintBuffer(buff);
	}

	return 0;
}
