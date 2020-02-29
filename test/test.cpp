#include <sbp/sbp.hpp>

#include <cassert>

enum class InstanceType
{
	Unknown = 0,
	IG,
	Sim,
	Sound
};

struct Heartbeat final
{
	std::string name;
	uint16_t port = 0;
	uint32_t flags = 0;
	InstanceType type = InstanceType::Unknown;
};

struct Lines final
{
	std::vector<std::string> lines;
};

struct TextCommand final
{
	uint64_t timestamp = 0;
	std::string text;
};

struct ListOfCommands final
{
	std::vector<TextCommand> commands;
};

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
int main(int argc, char* argv[])
{
	sbp::buffer buffer;

	Heartbeat heartbeatIn;
	heartbeatIn.name = "My machine";
	heartbeatIn.port = 55555;
	heartbeatIn.flags = 123;
	heartbeatIn.type = InstanceType::IG;
	sbp::write(buffer, heartbeatIn);

	TextCommand cmdIn;
	cmdIn.timestamp = 1;
	cmdIn.text = "Hello, world!";
	sbp::write(buffer, cmdIn);

	Lines linesIn;
	linesIn.lines.push_back("One");
	linesIn.lines.push_back("Two");
	linesIn.lines.push_back("Three");
	linesIn.lines.push_back("Four");
	linesIn.lines.push_back("Five");
	sbp::write(buffer, linesIn);

	ListOfCommands cmdsIn;
	cmdsIn.commands.push_back({ 1, "First" });
	cmdsIn.commands.push_back({ 2, "Second" });
	cmdsIn.commands.push_back({ 3, "Third" });
	sbp::write(buffer, cmdsIn);

	PrintBuffer(buffer);

	Heartbeat heartbeatOut;
	sbp::read(buffer, heartbeatOut);
	assert(sbp::detail::destructure(heartbeatIn) == sbp::detail::destructure(heartbeatOut));

	TextCommand cmdOut;
	sbp::read(buffer, cmdOut);
	assert(sbp::detail::destructure(cmdIn) == sbp::detail::destructure(cmdOut));

	Lines linesOut;
	sbp::read(buffer, linesOut);
	assert(sbp::detail::destructure(linesIn) == sbp::detail::destructure(linesOut));

	ListOfCommands cmdsOut;
	sbp::read(buffer, cmdsOut);
	//assert(sbp::detail::destructure(cmdsIn) == sbp::detail::destructure(cmdsOut));

	return 0;
}
