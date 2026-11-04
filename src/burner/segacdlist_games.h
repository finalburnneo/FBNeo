// sega cd / mega-cd game detection via SHA1 of toc (toc-hash, printed in the log as "TOC Hash")
// discs not listed here fall back to the disc header title and a short name made from the file name

struct sega_cd_game {
	TCHAR *sha1;
	TCHAR *name;
	TCHAR *id;
	TCHAR *year;
	TCHAR *company;
};

static sega_cd_game segacd_games[] = {
	{ _T("1ba98761123bc815d3b4eecd072438b54e9dbd01"), _T("Dune (USA)"), _T("duneu"), _T("1993"), _T("Virgin Games / Cryo") },
	{ _T("4c77e0b81a72aeb763c80dfa168a7700f39ab21f"), _T("Keio Flying Squadron (USA)"), _T("keiofsu"), _T("1994"), _T("JVC / Victor Entertainment") },
	{ _T("cfea7696b9280bef7f82bb693435e86fd6c7c5f3"), _T("Lunar: The Silver Star (USA)"), _T("lunaru"), _T("1993"), _T("Working Designs / Game Arts") },
	{ _T("ce0b4c6cf602cc471451e8a451792112bc4e40e1"), _T("Lunar: Eternal Blue (USA)"), _T("lunar2u"), _T("1994"), _T("Working Designs / Game Arts") },
	{ _T("f321fffaf0e8c3bcd42af118918cf0a73e83f806"), _T("Mortal Kombat Kanzenban (Japan)"), _T("mkkanzen"), _T("1994"), _T("Acclaim / Probe Software") },
	{ _T("2d252bd6afc1ef76db0769505a7bcdd4cec77cec"), _T("Puggsy (USA)"), _T("puggsyu"), _T("1994"), _T("Psygnosis") },
	{ _T("7c391316df9cb6b22d1537393ce711a94f036130"), _T("Sol-Feace (Europe)"), _T("solfeacee"), _T("1992"), _T("Wolf Team") },
	{ _T("46eff97ebe56033ede2a27492541b3e597023fd8"), _T("SoulStar + Battlecorps (Europe) (Demo)"), _T("soulstbcdemo"), _T("1994"), _T("Core Design") },
};
