#ifndef INFCLASS_CHAT_FILTER_H
#define INFCLASS_CHAT_FILTER_H
#include "engine/external/aho_corasick/aho_corasick.hpp"

#include <unordered_map>

class IConsole;

class CChatFilter
{
public:
	enum class HitBehavior
	{
		NONE, // Only used in result
		FILTER,
		KICK,
		BAN,
		COUNT,
		INVALID = COUNT,
	};

	void SetChatFilter(const char *Word, HitBehavior Behavior, int BanSeconds, IConsole *pConsole = nullptr);
	std::tuple<HitBehavior, int, std::vector<std::string>> CheckMessage(const char *pMessage);
	void ListChatFilters(IConsole *pConsole);

private:
	std::unordered_map<std::u32string, std::tuple<HitBehavior, int, std::string>> m_Filters;
	aho_corasick::basic_trie<char32_t> m_FilterTrie;
};

const char *toString(CChatFilter::HitBehavior Val);

#endif // INFCLASS_CHAT_FILTER_H
