#ifndef INFCLASSR_GEOLOCATION_H
#define INFCLASSR_GEOLOCATION_H

#include <infclassr/GeoLite2PP/GeoLite2PP.hpp>

class CGeolocation
{
private:
	GeoLite2PP::DB *db;
	int get_iso_numeric_code(GeoLite2PP::MStr &m);
	CGeolocation(const char *path_to_mmdb);
	~CGeolocation();

public:
	static bool Initialize(const char *pPathToDB);
	static void Shutdown();

	static int get_country_iso_numeric_code(std::string &ip);
};

#endif
