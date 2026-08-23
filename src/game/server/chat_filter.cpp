#include "chat_filter.h"

#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/tl/ic_enum.h>

#include <ranges>

static const char *gs_aBehaviorNames[] = {
	"none",
	"filter",
	"kick",
	"ban",
	"invalid",
};

const char *toString(const CChatFilter::HitBehavior Val)
{
	return toStringImpl(Val, gs_aBehaviorNames);
}

std::u32string IntArrayToU32String(const int *pArray, const int Length)
{
	std::u32string Result;
	Result.reserve(Length);

	for(int i = 0; i < Length; i++)
	{
		if(pArray[i] < 0)
			continue;
		Result.push_back(static_cast<char32_t>(pArray[i]));
	}

	return Result;
}

std::u32string CStringToU32SkeletonString(const char *String)
{
	int aSkeleton[256 * 4 + 1];
	const int SkeletonLength = str_utf8_to_skeleton(String, aSkeleton, std::size(aSkeleton));
	auto Skeleton = IntArrayToU32String(aSkeleton, SkeletonLength);

	return Skeleton;
}

void CChatFilter::SetChatFilter(const char *pWord, HitBehavior Behavior, int BanSeconds)
{
	if(!str_utf8_check(pWord))
	{
		log_error("filter_chat", "Not a valid utf-8 string");
		return;
	}
	if(str_length(pWord) > 256)
		return;

	char aTrimmed[256 + 1];
	str_copy(aTrimmed, str_utf8_skip_whitespaces(pWord));
	str_utf8_trim_right(aTrimmed);
	auto Skeleton = CStringToU32SkeletonString(aTrimmed);

	if(Skeleton.size() == 0)
	{
		log_error("filter_chat", "String is empty");
		return;
	}

	if(const auto Found = m_Filters.find(Skeleton); Found != m_Filters.end())
	{
		Found->second = std::make_tuple(Behavior, BanSeconds, aTrimmed);
		if(Behavior == HitBehavior::BAN)
		{
			log_info("filter_chat", "changed word='%s' behavior=%s ban_seconds=%d", pWord, toString(Behavior), BanSeconds);
		}
		else
		{
			log_info("filter_chat", "changed word='%s' behavior=%s", pWord, toString(Behavior));
		}
		return;
	}
	m_Filters.emplace(Skeleton, std::make_tuple(Behavior, BanSeconds, aTrimmed));
	m_FilterTrie.insert(Skeleton);

	if(Behavior == HitBehavior::BAN)
	{
		log_info("filter_chat", "added word='%s' behavior=%s ban_seconds=%d", pWord, toString(Behavior), BanSeconds);
	}
	else
	{
		log_info("filter_chat", "added word='%s' behavior=%s", pWord, toString(Behavior));
	}
}

std::tuple<CChatFilter::HitBehavior, int, std::vector<std::string>> CChatFilter::CheckMessage(const char *pMessage)
{
	const auto Skeleton = CStringToU32SkeletonString(pMessage);
	auto Behavior = HitBehavior::NONE;
	bool GotBanSeconds = false;
	int BanSeconds = 0;
	std::vector<std::string> vWordsHit;
	for(auto &WordOuter : m_FilterTrie.parse_text(Skeleton))
	{
		auto Word = WordOuter.get_keyword();
		if(auto EntryOuter = m_Filters.find(Word); EntryOuter != m_Filters.end())
		{
			auto [EntryBehavior, EntryBanSeconds, EntryKeyWordUtf8] = EntryOuter->second;
			vWordsHit.push_back(EntryKeyWordUtf8);
			if(EntryBehavior > Behavior)
				Behavior = EntryBehavior;
			if(EntryBehavior == HitBehavior::BAN)
			{
				if(!GotBanSeconds)
				{
					BanSeconds = EntryBanSeconds;
					GotBanSeconds = true;
				}
				else if(BanSeconds <= 0 || EntryBanSeconds <= 0)
				{
					BanSeconds = 0;
				}
				else
				{
					BanSeconds = maximum(BanSeconds, EntryBanSeconds);
				}
			}
		}
	}

	return std::make_tuple(Behavior, BanSeconds, vWordsHit);
}

void CChatFilter::ListChatFilters()
{
	for(const auto &[Behavior, BanSeconds, Word] : m_Filters | std::views::values)
	{
		if(Behavior == HitBehavior::BAN)
		{
			log_info("chat_filters", "word='%s' behavior=%s ban_seconds=%d", Word.c_str(), toString(Behavior), BanSeconds);
		}
		else
		{
			log_info("chat_filters", "word='%s' behavior=%s", Word.c_str(), toString(Behavior));
		}
	}
}
