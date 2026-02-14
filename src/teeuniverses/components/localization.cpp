#include "localization.h"

#include <engine/external/json-parser/json.h>
#include <engine/storage.h>
#include <unicode/ubidi.h>
#include <unicode/ushape.h>

/* LANGUAGE ***********************************************************/

auto MainLanguage = "en";

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
	if(str_comp(GetFilename(), MainLanguage) == 0)
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
		int Length;
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			const char *pKey = rStart[i]["key"];
			if(pKey && pKey[0])
			{
				auto &Entry = m_Translations.try_emplace(pKey).first->second;

				const char *pSingular = rStart[i]["value"];
				if(pSingular && pSingular[0])
				{
					Length = str_length(pSingular) + 1;
					Entry.m_apVersions[PLURALTYPE_NONE] = new char[Length];
					str_copy(Entry.m_apVersions[PLURALTYPE_NONE], pSingular, Length);
				}
				else
				{
					// Zero
					const char *pPlural = rStart[i]["zero"];
					if(pPlural && pPlural[0])
					{
						Length = str_length(pPlural) + 1;
						Entry.m_apVersions[PLURALTYPE_ZERO] = new char[Length];
						str_copy(Entry.m_apVersions[PLURALTYPE_ZERO], pPlural, Length);
					}
					// One
					pPlural = rStart[i]["one"];
					if(pPlural && pPlural[0])
					{
						Length = str_length(pPlural) + 1;
						Entry.m_apVersions[PLURALTYPE_ONE] = new char[Length];
						str_copy(Entry.m_apVersions[PLURALTYPE_ONE], pPlural, Length);
					}
					// Two
					pPlural = rStart[i]["two"];
					if(pPlural && pPlural[0])
					{
						Length = str_length(pPlural) + 1;
						Entry.m_apVersions[PLURALTYPE_TWO] = new char[Length];
						str_copy(Entry.m_apVersions[PLURALTYPE_TWO], pPlural, Length);
					}
					// Few
					pPlural = rStart[i]["few"];
					if(pPlural && pPlural[0])
					{
						Length = str_length(pPlural) + 1;
						Entry.m_apVersions[PLURALTYPE_FEW] = new char[Length];
						str_copy(Entry.m_apVersions[PLURALTYPE_FEW], pPlural, Length);
					}
					// Many
					pPlural = rStart[i]["many"];
					if(pPlural && pPlural[0])
					{
						Length = str_length(pPlural) + 1;
						Entry.m_apVersions[PLURALTYPE_MANY] = new char[Length];
						str_copy(Entry.m_apVersions[PLURALTYPE_MANY], pPlural, Length);
					}
					// Other
					pPlural = rStart[i]["other"];
					if(pPlural && pPlural[0])
					{
						Length = str_length(pPlural) + 1;
						Entry.m_apVersions[PLURALTYPE_OTHER] = new char[Length];
						str_copy(Entry.m_apVersions[PLURALTYPE_OTHER], pPlural, Length);
					}
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

const char *CLocalization::CLanguage::Localize(const char *pText) const
{
	if(str_comp(GetFilename(), MainLanguage) == 0)
		return pText;

	const auto &Entry = m_Translations.find(pText);
	if(Entry == m_Translations.end())
		return nullptr;

	return Entry->second.m_apVersions[PLURALTYPE_NONE];
}

const char *CLocalization::CLanguage::Localize_P(int Number, const char *pText) const
{
	if(str_comp(GetFilename(), MainLanguage) == 0)
		return pText;

	const auto &Entry = m_Translations.find(pText);
	if(Entry == m_Translations.end())
		return nullptr;

	UChar aPluralKeyWord[6];
	UErrorCode Status = U_ZERO_ERROR;
	uplrules_select(m_pPluralRules, static_cast<double>(Number), aPluralKeyWord, 6, &Status);

	if(U_FAILURE(Status))
		return nullptr;

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
	m_pStorage(pStorage),
	m_pUtf8Converter(nullptr)
{
}

CLocalization::~CLocalization()
{
	if(m_pUtf8Converter)
		ucnv_close(m_pUtf8Converter);
}

bool CLocalization::Init()
{
	UErrorCode Status = U_ZERO_ERROR;
	m_pUtf8Converter = ucnv_open("utf8", &Status);
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

const char *CLocalization::LanguageCodeByCountryCode(int CountryCode)
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

const char *CLocalization::FallbackLanguageForIpCountryCode(const int Country)
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

const char *CLocalization::LocalizeWithDepth(const char *pLanguageCode, const char *pText, const int Depth)
{
	CLanguage *pLanguage = GetLanguageByCode(MainLanguage);
	if(pLanguageCode)
	{
		if(auto *pFoundLanguage = GetLanguageByCode(pLanguageCode); pFoundLanguage)
			pLanguage = pFoundLanguage;
	}

	if(!pLanguage)
		return pText;

	if(!pLanguage->IsLoaded())
		pLanguage->Load(Storage());

	if(const char *pResult = pLanguage->Localize(pText))
		return pResult;
	else if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth(pLanguage->GetParentFilename(), pText, Depth + 1);
	else
		return pText;
}

/**
 *
 * @param pLanguageCode language code
 * @param pText source string
 * @return Ptr to the translated string.
 * The returned ptr should not be held for a long time,
 * nor should items be inserted into 'm_pLanguages' while holding it,
 * as this may lead to a dangling pointer.
 */
const char *CLocalization::Localize(const char *pLanguageCode, const char *pText)
{
	return LocalizeWithDepth(pLanguageCode, pText, 0);
}

const char *CLocalization::LocalizeWithDepth_P(const char *pLanguageCode, const int Number, const char *pText, const int Depth)
{
	CLanguage *pLanguage = GetLanguageByCode(MainLanguage);
	if(pLanguageCode)
	{
		if(auto *pFoundLanguage = GetLanguageByCode(pLanguageCode); pFoundLanguage)
			pLanguage = pFoundLanguage;
	}

	if(!pLanguage)
		return pText;

	if(!pLanguage->IsLoaded())
		pLanguage->Load(Storage());

	if(const char *pResult = pLanguage->Localize_P(Number, pText))
		return pResult;
	else if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth_P(pLanguage->GetParentFilename(), Number, pText, Depth + 1);
	else
		return pText;
}

const char *CLocalization::Localize_P(const char *pLanguageCode, int Number, const char *pText)
{
	return LocalizeWithDepth_P(pLanguageCode, Number, pText, 0);
}

void CLocalization::AppendNumber(dynamic_string &Buffer, int &BufferIter, const CLanguage *pLanguage, const int Number) const
{
	UChar aBufUtf16[128];

	UErrorCode Status = U_ZERO_ERROR;
	unum_format(pLanguage->m_pNumberFormater, Number, aBufUtf16, sizeof(aBufUtf16), nullptr, &Status);
	if(U_FAILURE(Status))
		BufferIter = Buffer.append_at(BufferIter, "_NUMBER_");
	else
	{
		// Update buffer size
		const int SrcLength = u_strlen(aBufUtf16);
		const int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(SrcLength, ucnv_getMaxCharSize(m_pUtf8Converter));

		while(Buffer.maxsize() - BufferIter <= NeededSize)
			Buffer.resize_buffer(Buffer.maxsize() * 2);

		const int Length = ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer() + BufferIter, Buffer.maxsize() - BufferIter, aBufUtf16, SrcLength, &Status);
		if(U_FAILURE(Status))
			BufferIter = Buffer.append_at(BufferIter, "_NUMBER_");
		else
			BufferIter += Length;
	}
}

void CLocalization::AppendPercent(dynamic_string &Buffer, int &BufferIter, const CLanguage *pLanguage, const double Number) const
{
	UChar aBufUtf16[128];

	UErrorCode Status = U_ZERO_ERROR;
	unum_formatDouble(pLanguage->m_pPercentFormater, Number, aBufUtf16, sizeof(aBufUtf16), nullptr, &Status);
	if(U_FAILURE(Status))
		BufferIter = Buffer.append_at(BufferIter, "_PERCENT_");
	else
	{
		// Update buffer size
		const int SrcLength = u_strlen(aBufUtf16);
		const int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(SrcLength, ucnv_getMaxCharSize(m_pUtf8Converter));

		while(Buffer.maxsize() - BufferIter <= NeededSize)
			Buffer.resize_buffer(Buffer.maxsize() * 2);

		const int Length = ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer() + BufferIter, Buffer.maxsize() - BufferIter, aBufUtf16, SrcLength, &Status);
		if(U_FAILURE(Status))
			BufferIter = Buffer.append_at(BufferIter, "_PERCENT_");
		else
			BufferIter += Length;
	}
}

void CLocalization::AppendDuration(dynamic_string &Buffer, int &BufferIter, const CLanguage *pLanguage, int Number, icu::TimeUnit::UTimeUnitFields Type) const
{
	UErrorCode Status = U_ZERO_ERROR;
	icu::UnicodeString BufUTF16;

	const auto pAmount = new icu::TimeUnitAmount(static_cast<double>(Number), Type, Status);
	icu::Formattable Formattable;
	Formattable.adoptObject(pAmount);
	pLanguage->m_pTimeUnitFormater->format(Formattable, BufUTF16, Status);

	if(U_FAILURE(Status))
		BufferIter = Buffer.append_at(BufferIter, "_DURATION_");
	else
	{
		const int SrcLength = BufUTF16.length();

		const int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(SrcLength, ucnv_getMaxCharSize(m_pUtf8Converter));

		while(Buffer.maxsize() - BufferIter <= NeededSize)
			Buffer.resize_buffer(Buffer.maxsize() * 2);

		Status = U_ZERO_ERROR;
		const int Length = ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer() + BufferIter, Buffer.maxsize() - BufferIter, BufUTF16.getBuffer(), SrcLength, &Status);

		if(U_FAILURE(Status))
			BufferIter = Buffer.append_at(BufferIter, "_DURATION_");
		else
			BufferIter += Length;
	}
}

void CLocalization::Format_V(dynamic_string &Buffer, const char *pLanguageCode, const char *pText, va_list VarArgs)
{
	const CLanguage *pLanguage = GetLanguageByCode(MainLanguage);
	if(pLanguageCode)
	{
		if(auto *pFoundLanguage = GetLanguageByCode(pLanguageCode); pFoundLanguage)
			pLanguage = pFoundLanguage;
	}
	if(!pLanguage)
	{
		Buffer.append(pText);
		return;
	}

	const char *pVarArgName = nullptr;
	const void *pVarArgValue = nullptr;

	int Iter = 0;
	int Start = Iter;
	int ParamTypeStart = -1;
	int ParamNameStart = -1;
	int ParamNameLength = 0;

	const int BufferStart = Buffer.length();
	int BufferIter = BufferStart;

	while(pText[Iter])
	{
		if(ParamNameStart >= 0)
		{
			if(pText[Iter] == '}') // End of the macro, try to apply it
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
					if(str_comp_num(pText + ParamNameStart, pVarArgName, ParamNameLength) == 0)
					{
						// Get argument type
						if(str_comp_num("str:", pText + ParamTypeStart, 4) == 0)
						{
							BufferIter = Buffer.append_at(BufferIter, static_cast<const char *>(pVarArgValue));
						}
						else if(str_comp_num("int:", pText + ParamTypeStart, 4) == 0)
						{
							const int Number = *static_cast<const int *>(pVarArgValue);
							AppendNumber(Buffer, BufferIter, pLanguage, Number);
						}
						else if(str_comp_num("percent:", pText + ParamTypeStart, 4) == 0)
						{
							const float Number = (*static_cast<const float *>(pVarArgValue));
							AppendPercent(Buffer, BufferIter, pLanguage, Number);
						}
						else if(str_comp_num("sec:", pText + ParamTypeStart, 4) == 0)
						{
							const int Duration = *static_cast<const int *>(pVarArgValue);
							const int Minutes = Duration / 60;
							const int Seconds = Duration - Minutes * 60;
							if(Minutes > 0)
							{
								AppendDuration(Buffer, BufferIter, pLanguage, Minutes, icu::TimeUnit::UTIMEUNIT_MINUTE);
								if(Seconds > 0)
								{
									BufferIter = Buffer.append_at(BufferIter, ", ");
									AppendDuration(Buffer, BufferIter, pLanguage, Seconds, icu::TimeUnit::UTIMEUNIT_SECOND);
								}
							}
							else
								AppendDuration(Buffer, BufferIter, pLanguage, Seconds, icu::TimeUnit::UTIMEUNIT_SECOND);
						}
						break;
					}

					pVarArgName = va_arg(VarArgsIter, const char *);
				}
				va_end(VarArgsIter);

				// Close the macro
				Start = Iter + 1;
				ParamTypeStart = -1;
				ParamNameStart = -1;
			}
			else
				ParamNameLength++;
		}
		else if(ParamTypeStart >= 0)
		{
			if(pText[Iter] == ':') // End of the type, start of the name
			{
				ParamNameStart = Iter + 1;
				ParamNameLength = 0;
			}
			else if(pText[Iter] == '}') // Invalid: no name found
			{
				// Close the macro
				Start = Iter + 1;
				ParamTypeStart = -1;
				ParamNameStart = -1;
			}
		}
		else
		{
			if(pText[Iter] == '{')
			{
				// Flush the content of pText in the buffer
				BufferIter = Buffer.append_at_num(BufferIter, pText + Start, Iter - Start);
				Iter++;
				ParamTypeStart = Iter;
			}
		}

		Iter = str_utf8_forward(pText, Iter);
	}

	if(Iter > 0 && ParamTypeStart == -1 && ParamNameStart == -1)
	{
		Buffer.append_at_num(BufferIter, pText + Start, Iter - Start);
	}

	if(pLanguage->GetWritingDirection() == DIRECTION_RTL)
		ArabicShaping(Buffer, BufferStart);
}

void CLocalization::Format(dynamic_string &Buffer, const char *pLanguageCode, const char *pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);

	Format_V(Buffer, pLanguageCode, pText, VarArgs);

	va_end(VarArgs);
}

void CLocalization::Format_VL(dynamic_string &Buffer, const char *pLanguageCode, const char *pText, va_list VarArgs)
{
	const char *pLocalText = Localize(pLanguageCode, pText);

	Format_V(Buffer, pLanguageCode, pLocalText, VarArgs);
}

void CLocalization::Format_L(dynamic_string &Buffer, const char *pLanguageCode, const char *pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);

	Format_VL(Buffer, pLanguageCode, pText, VarArgs);

	va_end(VarArgs);
}

void CLocalization::Format_VLP(dynamic_string &Buffer, const char *pLanguageCode, int Number, const char *pText, va_list VarArgs)
{
	const char *pLocalText = Localize_P(pLanguageCode, Number, pText);

	Format_V(Buffer, pLanguageCode, pLocalText, VarArgs);
}

void CLocalization::Format_LP(dynamic_string &Buffer, const char *pLanguageCode, int Number, const char *pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);

	Format_VLP(Buffer, pLanguageCode, Number, pText, VarArgs);

	va_end(VarArgs);
}

void CLocalization::ArabicShaping(dynamic_string &Buffer, int BufferStart) const
{
	UErrorCode Status = U_ZERO_ERROR;

	const int Length = (Buffer.length() - BufferStart + 1);
	const int LengthUTF16 = Length * 2;
	const auto pBuf0 = new UChar[LengthUTF16];
	const auto pBuf1 = new UChar[LengthUTF16];

	ucnv_toUChars(m_pUtf8Converter, pBuf0, LengthUTF16, Buffer.buffer() + BufferStart, Length, &Status);

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
	const int NeededSize = UCNV_GET_MAX_BYTES_FOR_STRING(ShapedLength, ucnv_getMaxCharSize(m_pUtf8Converter));

	while(Buffer.maxsize() - BufferStart <= NeededSize)
		Buffer.resize_buffer(Buffer.maxsize() * 2);

	ucnv_fromUChars(m_pUtf8Converter, Buffer.buffer() + BufferStart, Buffer.maxsize() - BufferStart, pBuf0, ShapedLength, &Status);

	delete[] pBuf0;
	delete[] pBuf1;
}

std::string CLocalization::GetLangaugeNameByCode(const char *pLanguageCode)
{
	if(const auto *pLanguage = GetLanguageByCode(pLanguageCode); pLanguage)
	{
		if(pLanguage->GetName()[0])
			return std::string(pLanguage->GetName());
	}
	return std::string("Unknown");
}

/**
 * @param pLanguageCode language code
 * @return Ptr to the CLanguage obj.
 * The returned ptr should not be held for a long time,
 * nor should items be inserted into 'm_pLanguages' while holding it,
 * as this may lead to a dangling pointer.
 */
CLocalization::CLanguage *CLocalization::GetLanguageByCode(const char *pLanguageCode)
{
	if(const auto LanguageIter = m_pLanguages.find(pLanguageCode); LanguageIter != m_pLanguages.end())
		return &LanguageIter->second;
	return nullptr;
}
