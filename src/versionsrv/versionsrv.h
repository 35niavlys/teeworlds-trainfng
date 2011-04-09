/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef VERSIONSRV_VERSIONSRV_H
#define VERSIONSRV_VERSIONSRV_H
static const int VERSIONSRV_PORT = 8302;

struct CMapVersion
{
	char m_aName[8];
	unsigned char m_aCrc[4];
	unsigned char m_aSize[4];
};

static CMapVersion s_aMapVersionList[] = {
	{"ctf1", {0xd2, 0xa8, 0x20, 0xbd}, {0x00, 0x00, 0x11, 0x9b}},
	{"ctf2", {0xb2, 0x19, 0xe7, 0x13}, {0x00, 0x00, 0x63, 0xfc}},
	{"ctf3", {0xe8, 0xd3, 0x39, 0xd0}, {0x00, 0x00, 0x11, 0x0e}},
	{"ctf4", {0x09, 0xd1, 0x70, 0x1d}, {0x00, 0x00, 0x20, 0x3f}},
	{"ctf5", {0x60, 0x7e, 0x6d, 0x7e}, {0x00, 0x00, 0x2f, 0x51}},
	{"dm1", {0xf3, 0x5c, 0x93, 0x09}, {0x00, 0x00, 0x15, 0x75}},
	{"dm2", {0x81, 0xe0, 0xa8, 0x9e}, {0x00, 0x00, 0x20, 0x11}},
	{"dm6", {0xae, 0x83, 0x06, 0x9f}, {0x00, 0x00, 0x17, 0x1d}},
	{"dm7", {0x42, 0x6d, 0xa1, 0x67}, {0x00, 0x00, 0x27, 0x2a}},
	{"dm8", {0x92, 0x23, 0x91, 0x82}, {0x00, 0x00, 0x3a, 0x33}},
	{"dm9", {0x48, 0xca, 0xfd, 0x11}, {0x00, 0x00, 0x18, 0x94}},
};
static const int s_NumMapVersionItems = sizeof(s_aMapVersionList)/sizeof(CMapVersion);

static const unsigned char VERSION_DATA[] = {0x00, 0, 6, 0};

static const unsigned char VERSIONSRV_GETVERSION[] = {255, 255, 255, 255, 'v', 'e', 'r', 'g'};
static const unsigned char VERSIONSRV_VERSION[] = {255, 255, 255, 255, 'v', 'e', 'r', 's'};

static const unsigned char VERSIONSRV_GETMAPLIST[] = {255, 255, 255, 255, 'v', 'm', 'l', 'g'};
static const unsigned char VERSIONSRV_MAPLIST[] = {255, 255, 255, 255, 'v', 'm', 'l', 's'};
#endif
