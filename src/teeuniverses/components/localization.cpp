#include "localization.h"

#include <engine/external/json-parser/json.h>
#include <engine/storage.h>
#include <unicode/ubidi.h>
#include <unicode/ushape.h>
#include <unicode/ustring.h>

/* LANGUAGE ***********************************************************/

constexpr std::string_view MainLanguage = "en";
constexpr int MaxUtf8BytesPerUCharFull = 4;

CLocalization::CLanguage::CLanguage() :
	m_Loaded(false),
	m_Direction(CLocalization::DIRECTION_LTR),
	m_pPluralRules(nullptr),
	m_pNumberFormater(nullptr),
	m_pPercentFormater(nullptr),
	m_pTimeUnitFormater(nullptr)
{
	m_aName[0] = 0;
	m_aFilename[0] = 0;
	m_aParentFilename[0] = 0;
}

CLocalization::CLanguage::CLanguage(const char *pName, const char *pFilename, const char *pParentFilename) :
	m_Loaded(false),
	m_Direction(CLocalization::DIRECTION_LTR),
	m_pPluralRules(nullptr),
	m_pNumberFormater(nullptr),
	m_pPercentFormater(nullptr)
{
	str_copy(m_aName, pName, sizeof(m_aName));
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
	str_copy(m_aParentFilename, pParentFilename, sizeof(m_aParentFilename));

	UErrorCode Status;

	Status = U_ZERO_ERROR;
	m_pNumberFormater = unum_open(UNUM_DECIMAL, nullptr, -1, m_aFilename, nullptr, &Status);
	if(U_FAILURE(Status))
	{
		if(m_pNumberFormater)
		{
			unum_close(m_pNumberFormater);
			m_pNumberFormater = nullptr;
		}
		dbg_msg("Localization", "Can't create number formater for %s (error #%d)", m_aFilename, Status);
	}

	Status = U_ZERO_ERROR;
	m_pPercentFormater = unum_open(UNUM_PERCENT, nullptr, -1, m_aFilename, nullptr, &Status);
	if(U_FAILURE(Status))
	{
		if(m_pPercentFormater)
		{
			unum_close(m_pPercentFormater);
			m_pPercentFormater = nullptr;
		}
		dbg_msg("Localization", "Can't create percent formater for %s (error #%d)", m_aFilename, Status);
	}

	Status = U_ZERO_ERROR;
	m_pPluralRules = uplrules_openForType(m_aFilename, UPLURAL_TYPE_CARDINAL, &Status);
	if(U_FAILURE(Status))
	{
		if(m_pPluralRules)
		{
			uplrules_close(m_pPluralRules);
			m_pPluralRules = nullptr;
		}
		dbg_msg("Localization", "Can't create plural rules for %s (error #%d)", m_aFilename, Status);
	}

	// Time unit for second formater
	Status = U_ZERO_ERROR;
	m_pTimeUnitFormater = new icu::TimeUnitFormat(m_aFilename, UTMUTFMT_ABBREVIATED_STYLE, Status);
	if(U_FAILURE(Status))
	{
		dbg_msg("Localization", "Can't create timeunit formater %s (error #%d)", pFilename, Status);
		delete m_pTimeUnitFormater;
		m_pTimeUnitFormater = nullptr;
	}
}

CLocalization::CLanguage::~CLanguage()
{
	if(m_pNumberFormater)
		unum_close(m_pNumberFormater);

	if(m_pPercentFormater)
		unum_close(m_pPercentFormater);

	if(m_pPluralRules)
		uplrules_close(m_pPluralRules);

	if(m_pTimeUnitFormater)
		delete m_pTimeUnitFormater;
}

bool CLocalization::CLanguage::Load(CStorage *pStorage)
{
	if(GetFilename() == MainLanguage)
	{
		m_Loaded = true;
		return true;
	}

	// read file data into buffer
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "languages/%s.json", m_aFilename);

	const IOHANDLE File = pStorage->OpenFile(aBuf, IOFLAG_READ, CStorage::TYPE_ALL);
	if(!File)
		return false;

	// load the file as a string
	const int FileSize = static_cast<int>(io_length(File));
	const int FileDataSize = FileSize + 1;
	const auto pFileData = new char[FileDataSize];
	io_read(File, pFileData, FileSize);
	pFileData[FileSize] = 0;
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileDataSize, aError);
	if(pJsonData == nullptr)
	{
		dbg_msg("Localization", "Can't load the localization file %s : %s", aBuf, aError);
		delete[] pFileData;
		return false;
	}

	dynamic_string Buffer;

	// extract data
	const json_value &rStart = (*pJsonData)["translation"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			const char *pKey = rStart[i]["key"];
			if(pKey && pKey[0])
			{
				auto &Entry = m_Translations.try_emplace(pKey).first->second;

				const char *pSingular = rStart[i]["value"];
				if(pSingular && pSingular[0])
					Entry.m_apVersions[PLURALTYPE_NONE].emplace(pSingular);
				else
				{
					// Zero
					const char *pPlural = rStart[i]["zero"];
					if(pPlural && pPlural[0])
						Entry.m_apVersions[PLURALTYPE_ZERO].emplace(pPlural);
					// One
					pPlural = rStart[i]["one"];
					if(pPlural && pPlural[0])
						Entry.m_apVersions[PLURALTYPE_ONE].emplace(pPlural);
					// Two
					pPlural = rStart[i]["two"];
					if(pPlural && pPlural[0])
						Entry.m_apVersions[PLURALTYPE_TWO].emplace(pPlural);
					// Few
					pPlural = rStart[i]["few"];
					if(pPlural && pPlural[0])
						Entry.m_apVersions[PLURALTYPE_FEW].emplace(pPlural);
					// Many
					pPlural = rStart[i]["many"];
					if(pPlural && pPlural[0])
						Entry.m_apVersions[PLURALTYPE_MANY].emplace(pPlural);
					// Other
					pPlural = rStart[i]["other"];
					if(pPlural && pPlural[0])
						Entry.m_apVersions[PLURALTYPE_OTHER].emplace(pPlural);
				}
			}
		}
	}

	// clean up
	json_value_free(pJsonData);
	delete[] pFileData;

	m_Loaded = true;

	return true;
}

std::optional<std::string_view> CLocalization::CLanguage::Localize(const std::string_view Text) const
{
	if(GetFilename() == MainLanguage)
		return Text;

	const auto &Entry = m_Translations.find(Text);
	if(Entry == m_Translations.end())
		return std::nullopt;

	return Entry->second.m_apVersions[PLURALTYPE_NONE];
}

std::optional<std::string_view> CLocalization::CLanguage::Localize_P(int Number, const std::string_view Text) const
{
	if(GetFilename() == MainLanguage)
		return Text;

	const auto &Entry = m_Translations.find(Text);
	if(Entry == m_Translations.end())
		return std::nullopt;

	UChar aPluralKeyWord[6];
	UErrorCode Status = U_ZERO_ERROR;
	uplrules_select(m_pPluralRules, static_cast<double>(Number), aPluralKeyWord, 6, &Status);

	if(U_FAILURE(Status))
		return std::nullopt;

	int PluralCode = PLURALTYPE_NONE;

	if(aPluralKeyWord[0] == 0x007A) // z
		PluralCode = PLURALTYPE_ZERO;
	else if(aPluralKeyWord[0] == 0x0074) // t
		PluralCode = PLURALTYPE_TWO;
	else if(aPluralKeyWord[0] == 0x0066) // f
		PluralCode = PLURALTYPE_FEW;
	else if(aPluralKeyWord[0] == 0x006D) // m
		PluralCode = PLURALTYPE_MANY;
	else if(aPluralKeyWord[0] == 0x006F) // o
	{
		if(aPluralKeyWord[1] == 0x0074) // t
			PluralCode = PLURALTYPE_OTHER;
		else if(aPluralKeyWord[1] == 0x006E) // n
			PluralCode = PLURALTYPE_ONE;
	}

	return Entry->second.m_apVersions[PluralCode];
}

CLocalization::CLocalization(class CStorage *pStorage) :
	m_pStorage(pStorage)
{
}

bool CLocalization::Init()
{
	UErrorCode Status = U_ZERO_ERROR;
	if(U_FAILURE(Status))
	{
		dbg_msg("Localization", "Can't create UTF8/UTF16 convertert");
		return false;
	}

	// read file data into buffer
	const auto pFilename = "languages/index.json";
	const IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, CStorage::TYPE_ALL);
	if(!File)
	{
		dbg_msg("Localization", "can't open languages/index.json");
		return true; // return true because it's not a critical error
	}

	const int FileSize = static_cast<int>(io_length(File));
	const int FileDataSize = FileSize + 1;
	const auto pFileData = new char[FileDataSize];
	io_read(File, pFileData, FileSize);
	pFileData[FileSize] = 0;
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, FileDataSize, aError);
	if(pJsonData == nullptr)
	{
		delete[] pFileData;
		return true; // return true because it's not a critical error
	}

	// extract data
	const json_value &rStart = (*pJsonData)["language indices"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			auto &Language = m_pLanguages.try_emplace(
											 static_cast<const char *>(rStart[i]["file"]),
											 static_cast<const char *>(rStart[i]["name"]),
											 static_cast<const char *>(rStart[i]["file"]),
											 static_cast<const char *>(rStart[i]["parent"]))
								 .first->second;

			if(static_cast<const char *>(rStart[i]["direction"]) && str_comp((const char *)rStart[i]["direction"], "rtl") == 0)
				Language.SetWritingDirection(DIRECTION_RTL);
		}
	}

	// clean up
	json_value_free(pJsonData);
	delete[] pFileData;

	return true;
}

std::string_view CLocalization::LanguageCodeByCountryCode(int CountryCode)
{
	// Constants from 'data/countryflags/index.txt'
	switch(CountryCode)
	{
	/* ar - Arabic ************************************/
	case 12: // Algeria
	case 48: // Bahrain
	case 262: // Djibouti
	case 818: // Egypt
	case 368: // Iraq
	case 400: // Jordan
	case 414: // Kuwait
	case 422: // Lebanon
	case 434: // Libya
	case 478: // Mauritania
	case 504: // Morocco
	case 512: // Oman
	case 275: // Palestine
	case 634: // Qatar
	case 682: // Saudi Arabia
	case 706: // Somalia
	case 729: // Sudan
	case 760: // Syria
	case 788: // Tunisia
	case 784: // United Arab Emirates
	case 887: // Yemen
		return "ar";
	/* bg - Bosnian *************************************/
	case 100: // Bulgaria
		return "bg";
	/* bs - Bosnian *************************************/
	case 70: // Bosnia and Hercegovina
		return "bs";
	/* cs - Czech *************************************/
	case 203: // Czechia
		return "cs";
	/* de - German ************************************/
	case 40: // Austria
	case 276: // Germany
	case 438: // Liechtenstein
	case 756: // Switzerland
		return "de";
	/* el - Greek ***********************************/
	case 300: // Greece
	case 196: // Cyprus
		return "el";
	/* es - Spanish ***********************************/
	case 32: // Argentina
	case 68: // Bolivia
	case 152: // Chile
	case 170: // Colombia
	case 188: // Costa Rica
	case 192: // Cuba
	case 214: // Dominican Republic
	case 218: // Ecuador
	case 222: // El Salvador
	case 226: // Equatorial Guinea
	case 320: // Guatemala
	case 340: // Honduras
	case 484: // Mexico
	case 558: // Nicaragua
	case 591: // Panama
	case 600: // Paraguay
	case 604: // Peru
	case 630: // Puerto Rico
	case 724: // Spain
	case 858: // Uruguay
	case 862: // Venezuela
		return "es";
	/* fa - Farsi ************************************/
	case 364: // Islamic Republic of Iran
	case 4: // Afghanistan
		return "fa";
	/* fr - French ************************************/
	case 204: // Benin
	case 854: // Burkina Faso
	case 178: // Republic of the Congo
	case 384: // Cote d’Ivoire
	case 266: // Gabon
	case 324: // Ginea
	case 466: // Mali
	case 562: // Niger
	case 686: // Senegal
	case 768: // Togo
	case 250: // France
	case 492: // Monaco
		return "fr";
	/* hr - Croatian **********************************/
	case 191: // Croatia
		return "hr";
	/* hu - Hungarian *********************************/
	case 348: // Hungary
		return "hu";
	/* it - Italian ***********************************/
	case 380: // Italy
		return "it";
	/* ja - Japanese **********************************/
	case 392: // Japan
		return "ja";
	/* la - Latin *************************************/
	case 336: // Vatican
		return "la";
	/* nl - Dutch *************************************/
	case 533: // Aruba
	case 531: // Curaçao
	case 534: // Sint Maarten
	case 528: // Netherland
	case 740: // Suriname
	case 56: // Belgique
		return "nl";
	/* pl - Polish *************************************/
	case 616: // Poland
		return "pl";
	/* pt - Portuguese ********************************/
	case 24: // Angola
	case 132: // Cape Verde
	// case 226: //Equatorial Guinea: official language, but not national language
	// case 446: //Macao: official language, but spoken by less than 1% of the population
	case 508: // Mozambique
	case 626: // Timor-Leste
	case 678: // São Tomé and Príncipe
		return "pt";
	/* pt-BR - Portuguese (Brazil) ********************************/
	case 76: // Brazil
		return "pt-BR";
	/* ru - Russian ***********************************/
	case 112: // Belarus
	case 643: // Russia
	case 398: // Kazakhstan
		return "ru";
	/* sk - Slovak ************************************/
	case 703: // Slovakia
		return "sk";
	/* sr - Serbian ************************************/
	case 688: // Serbia
		return "sr";
	/* tl - Tagalog ************************************/
	case 608: // Philippines
		return "tl";
	/* tr - Turkish ************************************/
	case 31: // Azerbaijan
	case 792: // Turkey
		return "tr";
	/* uk - Ukrainian **********************************/
	case 804: // Ukraine
		return "uk";
	/* zh-CN - Chinese (Simplified) **********************************/
	case 156: // People’s Republic of China
	case 344: // Hong Kong
	case 446: // Macau
		return "zh-CN";
	case 826: // United Kingdom of Great Britain and Northern Ireland
	case 840: // United States of America
		return "en";
	default:
		return MainLanguage;
	}
}

std::string_view CLocalization::FallbackLanguageForIpCountryCode(const int Country)
{
	switch(Country)
	{
	case 364: // Islamic Republic of Iran
	case 4: // Afghanistan
		return "fa";
	case 112: // Belarus
	case 643: // Russia
	case 398: // Kazakhstan
		return "ru";
	default:
		return "en";
	}
}

std::string_view CLocalization::LocalizeWithDepth(const std::string_view LanguageCode, const std::string_view Text, const int Depth)
{
	CLanguage *pLanguage = GetLanguageByCode(MainLanguage);
	if(auto *pFoundLanguage = GetLanguageByCode(LanguageCode); pFoundLanguage)
		pLanguage = pFoundLanguage;

	if(!pLanguage)
		// ReSharper disable once CppDFALocalValueEscapesFunction
		return Text;

	if(!pLanguage->IsLoaded())
		pLanguage->Load(Storage());

	if(const auto optResult = pLanguage->Localize(Text); optResult.has_value())
		return optResult.value();
	else if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth(pLanguage->GetParentFilename(), Text, Depth + 1);
	else
		// ReSharper disable once CppDFALocalValueEscapesFunction
		return Text;
}

/**
 *
 * @param LanguageCode language code
 * @param Text source string
 * @return View to the translated string.
 * The returned view should not be held for a long time,
 * nor should items be inserted into 'm_pLanguages' while holding it,
 * as this may lead to a dangling pointer.
 */
std::string_view CLocalization::Localize(const std::string_view LanguageCode, const std::string_view Text)
{
	return LocalizeWithDepth(LanguageCode, Text, 0);
}

std::string_view CLocalization::LocalizeWithDepth_P(const std::string_view LanguageCode, const int Number, const std::string_view Text, const int Depth)
{
	CLanguage *pLanguage = GetLanguageByCode(MainLanguage);
	if(auto *pFoundLanguage = GetLanguageByCode(LanguageCode); pFoundLanguage)
		pLanguage = pFoundLanguage;

	if(!pLanguage)
		// ReSharper disable once CppDFALocalValueEscapesFunction
		return Text;

	if(!pLanguage->IsLoaded())
		pLanguage->Load(Storage());

	if(const auto optResult = pLanguage->Localize_P(Number, Text); optResult.has_value())
		return optResult.value();
	else if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth_P(pLanguage->GetParentFilename(), Number, Text, Depth + 1);
	else
		// ReSharper disable once CppDFALocalValueEscapesFunction
		return Text;
}

std::string_view CLocalization::Localize_P(const std::string_view LanguageCode, int Number, const std::string_view Text)
{
	return LocalizeWithDepth_P(LanguageCode, Number, Text, 0);
}

void CLocalization::AppendNumber(std::string &Buffer, const CLanguage *pLanguage, const int Number)
{
	UChar aBufUtf16[128];
	char aBufUtf8[128 * MaxUtf8BytesPerUCharFull];

	UErrorCode Status = U_ZERO_ERROR;
	unum_format(pLanguage->m_pNumberFormater, Number, aBufUtf16, sizeof(aBufUtf16), nullptr, &Status);
	if(U_FAILURE(Status))
		Buffer.append("_NUMBER_");
	else
	{
		// Update buffer size
		const int SrcLength = u_strlen(aBufUtf16);

		u_strToUTF8(aBufUtf8,
			128 * MaxUtf8BytesPerUCharFull,
			nullptr,
			aBufUtf16,
			SrcLength,
			&Status);
		Buffer.append(aBufUtf8);

		if(U_FAILURE(Status))
			Buffer.append("_NUMBER_");
	}
}

void CLocalization::AppendPercent(std::string &Buffer, const CLanguage *pLanguage, const double Number)
{
	UChar aBufUtf16[128];
	char aBufUtf8[128 * MaxUtf8BytesPerUCharFull];

	UErrorCode Status = U_ZERO_ERROR;
	unum_formatDouble(pLanguage->m_pPercentFormater, Number, aBufUtf16, sizeof(aBufUtf16), nullptr, &Status);
	if(U_FAILURE(Status))
		Buffer.append("_PERCENT_");
	else
	{
		// Update buffer size
		const int SrcLength = u_strlen(aBufUtf16);

		u_strToUTF8(aBufUtf8,
			128 * MaxUtf8BytesPerUCharFull,
			nullptr,
			aBufUtf16,
			SrcLength,
			&Status);
		Buffer.append(aBufUtf8);
		if(U_FAILURE(Status))
			Buffer.append("_PERCENT_");
	}
}

void CLocalization::AppendDuration(std::string &Buffer, const CLanguage *pLanguage, int Number, icu::TimeUnit::UTimeUnitFields Type)
{
	UErrorCode Status = U_ZERO_ERROR;
	icu::UnicodeString BufUTF16;

	const auto pAmount = new icu::TimeUnitAmount(static_cast<double>(Number), Type, Status);
	icu::Formattable Formattable;
	Formattable.adoptObject(pAmount);
	pLanguage->m_pTimeUnitFormater->format(Formattable, BufUTF16, Status);

	if(U_FAILURE(Status))
		Buffer.append("_DURATION_");
	else
	{
		const int SrcLength = BufUTF16.length();
		const int NeededSize = MaxUtf8BytesPerUCharFull * SrcLength + 1;

		const auto paBufUtf8 = new char[NeededSize];

		Status = U_ZERO_ERROR;
		u_strToUTF8(paBufUtf8,
			NeededSize,
			nullptr,
			BufUTF16.getBuffer(),
			SrcLength,
			&Status);
		Buffer.append(paBufUtf8);

		delete[] paBufUtf8;
		if(U_FAILURE(Status))
			Buffer.append("_DURATION_");
	}
}

std::string CLocalization::Format_V(const std::string_view LanguageCode, const std::string_view Text, va_list VarArgs)
{
	std::string Buffer;

	const CLanguage *pLanguage = GetLanguageByCode(MainLanguage);
	if(const auto *pFoundLanguage = GetLanguageByCode(LanguageCode); pFoundLanguage)
		pLanguage = pFoundLanguage;

	if(!pLanguage)
	{
		Buffer.append(Text);
		return Buffer;
	}

	const char *pVarArgName = nullptr;
	const void *pVarArgValue = nullptr;

	size_t Iter = 0;
	int Start = Iter;
	size_t ParamTypeStart = 0;
	bool ParamTypeStarted = false;
	size_t ParamNameStart = 0;
	bool ParamNameStarted = false;
	int ParamNameLength = 0;

	while(Iter < Text.size())
	{
		if(ParamNameStarted)
		{
			if(Text[Iter] == '}') // End of the macro, try to apply it
			{
				// Try to find an argument with this name
				va_list VarArgsIter;

// windows
#if defined(CONF_FAMILY_WINDOWS)
#define va_copy(d, s) ((d) = (s))
#endif

				va_copy(VarArgsIter, VarArgs);
				pVarArgName = va_arg(VarArgsIter, const char *);
				while(pVarArgName)
				{
					pVarArgValue = va_arg(VarArgsIter, const void *);
					if(Text.substr(ParamNameStart, ParamNameLength) == pVarArgName)
					{
						// Get argument type
						if(Text.length() >= ParamTypeStart + 4 && Text.substr(ParamTypeStart, 4) == "str:")
						{
							Buffer.append(static_cast<const char *>(pVarArgValue));
						}
						else if(Text.length() >= ParamTypeStart + 4 && Text.substr(ParamTypeStart, 4) == "int:")
						{
							const int Number = *static_cast<const int *>(pVarArgValue);
							AppendNumber(Buffer, pLanguage, Number);
						}
						else if(Text.length() >= ParamTypeStart + 8 && Text.substr(ParamTypeStart, 8) == "percent:")
						{
							const float Number = (*static_cast<const float *>(pVarArgValue));
							AppendPercent(Buffer, pLanguage, Number);
						}
						else if(Text.length() >= ParamTypeStart + 4 && Text.substr(ParamTypeStart, 4) == "sec:")
						{
							const int Duration = *static_cast<const int *>(pVarArgValue);
							const int Minutes = Duration / 60;
							const int Seconds = Duration - Minutes * 60;
							if(Minutes > 0)
							{
								AppendDuration(Buffer, pLanguage, Minutes, icu::TimeUnit::UTIMEUNIT_MINUTE);
								if(Seconds > 0)
								{
									Buffer.append(", ");
									AppendDuration(Buffer, pLanguage, Seconds, icu::TimeUnit::UTIMEUNIT_SECOND);
								}
							}
							else
								AppendDuration(Buffer, pLanguage, Seconds, icu::TimeUnit::UTIMEUNIT_SECOND);
						}
						break;
					}

					pVarArgName = va_arg(VarArgsIter, const char *);
				}
				va_end(VarArgsIter);

				// Close the macro
				Start = Iter + 1;
				ParamTypeStarted = false;
				ParamNameStarted = false;
			}
			else
				ParamNameLength++;
		}
		else if(ParamTypeStarted)
		{
			if(Text[Iter] == ':') // End of the type, start of the name
			{
				ParamNameStart = Iter + 1;
				ParamNameStarted = true;
				ParamNameLength = 0;
			}
			else if(Text[Iter] == '}') // Invalid: no name found
			{
				// Close the macro
				Start = Iter + 1;
				ParamTypeStarted = false;
				ParamNameStarted = false;
			}
		}
		else
		{
			if(Text[Iter] == '{')
			{
				// Flush the content of Text in the buffer
				Buffer.append(Text.substr(Start, Iter - Start));
				Iter++;
				ParamTypeStart = Iter;
				ParamTypeStarted = true;
			}
		}

		Iter = str_utf8_forward(Text.data(), Iter);
	}

	if(Iter > 0 && !ParamTypeStarted && !ParamNameStarted)
	{
		Buffer.append(Text.substr(Start, Iter - Start));
	}

	if(pLanguage->GetWritingDirection() == DIRECTION_RTL)
		ArabicShaping(Buffer);

	return Buffer;
}

std::string CLocalization::Format(const std::string_view LanguageCode, const char *Text, ...)
{
	va_list VarArgs;
	va_start(VarArgs, Text);

	return Format_V(LanguageCode, Text, VarArgs);

	va_end(VarArgs);
}

std::string CLocalization::Format_VL(const std::string_view LanguageCode, const std::string_view Text, va_list VarArgs)
{
	const auto LocalText = Localize(LanguageCode, Text);

	return Format_V(LanguageCode, LocalText, VarArgs);
}

std::string CLocalization::Format_L(const std::string_view LanguageCode, const std::string_view Text, ...)
{
	va_list VarArgs;
	va_start(VarArgs, Text);

	return Format_VL(LanguageCode, Text, VarArgs);

	va_end(VarArgs);
}

std::string CLocalization::Format_VLP(const std::string_view LanguageCode, int Number, const std::string_view Text, va_list VarArgs)
{
	const auto LocalText = Localize_P(LanguageCode, Number, Text);

	return Format_V(LanguageCode, LocalText, VarArgs);
}

std::string CLocalization::Format_LP(const std::string_view LanguageCode, int Number, const std::string_view Text, ...)
{
	va_list VarArgs;
	va_start(VarArgs, Text);

	return Format_VLP(LanguageCode, Number, Text, VarArgs);

	va_end(VarArgs);
}

void CLocalization::ArabicShaping(std::string &Buffer)
{
	UErrorCode Status = U_ZERO_ERROR;

	const int Length = Buffer.size() + 1;
	const int LengthUTF16 = Length * 2;
	const auto pBuf0 = new UChar[LengthUTF16];
	const auto pBuf1 = new UChar[LengthUTF16];

	u_strFromUTF8(
		pBuf0,
		LengthUTF16,
		nullptr,
		Buffer.data(),
		Buffer.size(),
		&Status);

	UBiDi *pBiDi = ubidi_openSized(LengthUTF16, 0, &Status);
	ubidi_setPara(pBiDi, pBuf0, -1, UBIDI_DEFAULT_LTR, nullptr, &Status);
	ubidi_writeReordered(pBiDi, pBuf1, LengthUTF16, UBIDI_DO_MIRRORING, &Status);
	ubidi_close(pBiDi);

	u_shapeArabic(
		pBuf1, LengthUTF16,
		pBuf0, LengthUTF16,
		U_SHAPE_LETTERS_SHAPE |
			U_SHAPE_PRESERVE_PRESENTATION |
			U_SHAPE_TASHKEEL_RESIZE |
			U_SHAPE_LENGTH_GROW_SHRINK |
			U_SHAPE_TEXT_DIRECTION_VISUAL_LTR |
			U_SHAPE_LAMALEF_RESIZE,
		&Status);

	const int ShapedLength = u_strlen(pBuf0);
	const int NeededSize = MaxUtf8BytesPerUCharFull * ShapedLength + 1;

	const auto paResult = new char[NeededSize];

	u_strToUTF8(
		paResult,
		NeededSize,
		nullptr,
		pBuf0,
		ShapedLength,
		&Status);

	Buffer.clear();
	Buffer.append(paResult);

	delete[] pBuf0;
	delete[] pBuf1;
	delete[] paResult;
}

std::string CLocalization::GetLangaugeNameByCode(const std::string_view LanguageCode)
{
	if(const auto *pLanguage = GetLanguageByCode(LanguageCode); pLanguage)
	{
		if(pLanguage->GetName()[0])
			return std::string(pLanguage->GetName());
	}
	return std::string("Unknown");
}

/**
 * @param LanguageCode language code
 * @return Ptr to the CLanguage obj.
 * The returned ptr should not be held for a long time,
 * nor should items be inserted into 'm_pLanguages' while holding it,
 * as this may lead to a dangling pointer.
 */
CLocalization::CLanguage *CLocalization::GetLanguageByCode(const std::string_view LanguageCode)
{
	if(const auto LanguageIter = m_pLanguages.find(LanguageCode); LanguageIter != m_pLanguages.end())
		return &LanguageIter->second;
	return nullptr;
}
