#include <gtest/gtest.h>

#include <game/server/infclass/bot_config_parser.h>

TEST(BotConfigParser, Tweaks)
{
	{
		TweaksArray Tweaks;
		ParseTweaks("tweaks=no-hook", &Tweaks);
		EXPECT_EQ(Tweaks.Size(), 1);
		EXPECT_EQ(Tweaks.At(0), EBotTweak::NoHook);
	}

	{
		TweaksArray Tweaks;
		ParseTweaks("tweaks=weak-hook", &Tweaks);
		EXPECT_EQ(Tweaks.Size(), 1);
		EXPECT_EQ(Tweaks.At(0), EBotTweak::WeakHook);
	}

	{
		TweaksArray Tweaks;
		ParseTweaks("tweaks=strong-hook", &Tweaks);
		EXPECT_EQ(Tweaks.Size(), 1);
		EXPECT_EQ(Tweaks.At(0), EBotTweak::StrongHook);
	}
}
