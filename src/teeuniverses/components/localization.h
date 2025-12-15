#ifndef __SHARED_LOCALIZATION__
#define __SHARED_LOCALIZATION__

#define CStorage IStorage

#include "teeuniverses/system/string.h"

#include <string>
#include <string_view>
#include <unicode/tmutfmt.h>
#include <unicode/upluralrules.h>
#include <unordered_map>

struct StringTransparentHasher
{
	using is_transparent = void;
	size_t operator()(const std::string &Key) const { return std::hash<std::string>{}(Key); };
	size_t operator()(const std::string_view Key) const { return std::hash<std::string_view>{}(Key); };
	size_t operator()(const char *Key) const { return std::hash<std::string_view>{}(Key); };
};

template<typename T>
using StringHashMap = std::unordered_map<
	std::string,
	T,
	StringTransparentHasher,
	std::equal_to<>>;

struct CLocalizableString
{
	const char *m_pText;

	CLocalizableString(const char *pText) :
		m_pText(pText)
	{
	}
};

/* BEGIN EDIT *********************************************************/
#define _(TEXT) TEXT
#define _P(TEXT_SINGULAR, TEXT_PLURAL, NUMBER) (NUMBER == 1 ? TEXT_SINGULAR : TEXT_PLURAL)
#define _C(CONTEXT, TEXT) TEXT
#define _CP(CONTEXT, TEXT_SINGULAR, TEXT_PLURAL, NUMBER) (NUMBER == 1 ? TEXT_SINGULAR : TEXT_PLURAL)
/* END EDIT ***********************************************************/

/* BEGIN EDIT *********************************************************/
class CLocalization
{
private:
	class CStorage *m_pStorage;
	inline class CStorage *Storage() const { return m_pStorage; }
	/* END EDIT ***********************************************************/
public:
	enum
	{
		PLURALTYPE_NONE = 0,
		PLURALTYPE_ZERO,
		PLURALTYPE_ONE,
		PLURALTYPE_TWO,
		PLURALTYPE_FEW,
		PLURALTYPE_MANY,
		PLURALTYPE_OTHER,
		NUM_PLURALTYPES,
	};

	static std::string_view LanguageCodeByCountryCode(int country);
	static std::string_view FallbackLanguageForIpCountryCode(int Country);

	class CLanguage
	{
	protected:
		class CEntry
		{
		public:
			std::optional<std::string> m_apVersions[NUM_PLURALTYPES];

			CEntry()
			{
				for(int i = 0; i < NUM_PLURALTYPES; i++)
					m_apVersions[i] = std::nullopt;
			}
		};

	protected:
		char m_aName[64];
		char m_aFilename[64];
		char m_aParentFilename[64];
		bool m_Loaded;
		int m_Direction;

		StringHashMap<CEntry> m_Translations;

	public:
		UPluralRules *m_pPluralRules;
		UNumberFormat *m_pNumberFormater;
		UNumberFormat *m_pPercentFormater;
		icu::TimeUnitFormat *m_pTimeUnitFormater;

		CLanguage();
		CLanguage(const char *pName, const char *pFilename, const char *pParentFilename);
		~CLanguage();

		inline const char *GetParentFilename() const { return m_aParentFilename; }
		inline const char *GetFilename() const { return m_aFilename; }
		inline const char *GetName() const { return m_aName; }
		inline int GetWritingDirection() const { return m_Direction; }
		inline void SetWritingDirection(int Direction) { m_Direction = Direction; }
		inline bool IsLoaded() const { return m_Loaded; }
		bool Load(CStorage *pStorage);
		[[nodiscard]] std::optional<std::string_view> Localize(std::string_view Text) const;
		std::optional<std::string_view> Localize_P(int Number, std::string_view Text) const;
	};

	enum
	{
		DIRECTION_LTR = 0,
		DIRECTION_RTL,
		NUM_DIRECTIONS,
	};
	StringHashMap<CLanguage> m_pLanguages;

protected:
	std::string_view LocalizeWithDepth(std::string_view LanguageCode, std::string_view Text, int Depth);
	[[nodiscard]] std::string_view LocalizeWithDepth_P(std::string_view LanguageCode, int Number, std::string_view Text, int Depth);

	static void AppendNumber(std::string &Buffer, const CLanguage *pLanguage, int Number);
	static void AppendPercent(std::string &Buffer, const CLanguage *pLanguage, double Number);
	static void AppendDuration(std::string &Buffer, const CLanguage *pLanguage, int Number, icu::TimeUnit::UTimeUnitFields Type);

public:
	/* BEGIN EDIT *********************************************************/
	explicit CLocalization(class CStorage *pStorage);
	/* END EDIT ***********************************************************/

	/* BEGIN EDIT *********************************************************/
	char m_ArgNumberColor[5] = "\0"; // useful for colorizing numbers in broadcasts for 0.7 clients
	/* END EDIT ***********************************************************/
	bool Init();

	// inline bool GetWritingDirection() const { return (!m_pMainLanguage ? DIRECTION_LTR : m_pMainLanguage->GetWritingDirection()); }
	static bool GetWritingDirection() { return DIRECTION_LTR; }

	// localize
	[[nodiscard]] std::string_view Localize(std::string_view LanguageCode, std::string_view Text);
	// localize and find the appropriate plural form based on Number
	[[nodiscard]] std::string_view Localize_P(std::string_view LanguageCode, int Number, std::string_view Text);

	// format
	[[nodiscard]] std::string Format_V(std::string_view LanguageCode, std::string_view Text, va_list VarArgs);
	[[nodiscard]] std::string Format(std::string_view LanguageCode, const char *Text, ...);
	// localize, format
	[[nodiscard]] std::string Format_VL(std::string_view LanguageCode, std::string_view Text, va_list VarArgs);
	[[nodiscard]] std::string Format_L(std::string_view LanguageCode, std::string_view Text, ...);
	// localize, find the appropriate plural form based on Number and format
	[[nodiscard]] std::string Format_VLP(std::string_view LanguageCode, int Number, std::string_view Text, va_list VarArgs);
	[[nodiscard]] std::string Format_LP(std::string_view LanguageCode, int Number, std::string_view Text, ...);

	static void ArabicShaping(std::string &Buffer);

	[[nodiscard]] std::string GetLangaugeNameByCode(std::string_view LanguageCode);
	[[nodiscard]] CLanguage *GetLanguageByCode(std::string_view LanguageCode);
};

#endif
