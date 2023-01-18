#include "burner.h"
#include "vid_overlay.h"
#include "vid_font.h"
#include "vid_dynrender.h"
#include "vid_memorybuffer.h"
#include "vid_detector.h"
#include <d3dx9.h>

//#define PRINT_DEBUG_INFO
//#define TEST_OVERLAY
//#define TEST_VERSION				L"RC7 v3"

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define TEXT_ANIM_FRAMES    		8
#define NUM_RANKS           		7
#define INFO_FRAMES					150
#define WARNING_FRAMES				180
#define CHAT_LINES					7
#define CHAT_FRAMES					200
#define CHAT_FRAMES_EXT 			350
#define START_FRAMES				300
#define END_FRAMES					500
#define DETECTOR_FRAMES				30
#define WARNING_THRESHOLD			260
#define WARNING_MAX					510
#define WARNING_MSGCOUNT			4
#define MAX_CHARACTERS				32

static const float FONT_SPACING = 0.6f;
static const float FNT_SMA = 0.10f;
static const float FNT_MED = 0.20f;
static const float FNT_CHAT = 0.24f;
static const float FNT_SCALE = 1.0f;
static const float FNT_SEPARATION = 0.09f;


//------------------------------------------------------------------------------------------------------------------------------
// render vars
//------------------------------------------------------------------------------------------------------------------------------
static IDirect3DDevice9Ex *pD3DDevice = 0;
static CDynRender renderer;
static CFont font;
static RECT frame_dest;
static bool debug_mode = false;
static bool debug_turbo = false;
static bool debug_freeze = false;
static float frame_scale;
static float frame_adjust;
static float frame_width;
static float frame_height;
static float frame_ratio = 1;
static int frame_time = 0;
static int stats_line1_warning = 0;
static int stats_line2_warning = 0;
static int stats_line3_warning = 0;

//------------------------------------------------------------------------------------------------------------------------------
// test inputs
//------------------------------------------------------------------------------------------------------------------------------
#define INPUT_DISPLAY 60
#define INPUT_BUTTONS 10
struct TInput {
	INT32 frame;
	INT32 values;
	int state;
};

static TInput display_inputs[2][2][INPUT_DISPLAY+1] = {};

enum {
	INPUT_UP,
	INPUT_DOWN,
	INPUT_LEFT,
	INPUT_RIGHT,
	INPUT_LP,
	INPUT_MP,
	INPUT_HP,
	INPUT_LK,
	INPUT_MK,
	INPUT_HK,
};

#define FPX(x)	((float)(x) * (float)(frame_dest.right - frame_dest.left) + frame_dest.left * frame_ratio)
#define FPY(x)	((float)(x) * (float)(frame_dest.right - frame_dest.left) + frame_dest.top * frame_ratio)
#define FS(x)	((float)(x) * (float)frame_scale)
#define FNT_SEP (FNT_SEPARATION * frame_adjust)

bool LoadD3DTextureFromFile(IDirect3DDevice9* device, const char* filename, IDirect3DTexture9*& texture, int& width, int& height);

//------------------------------------------------------------------------------------------------------------------------------
// overlay vars
//------------------------------------------------------------------------------------------------------------------------------
static int game_enabled = 0;
static int game_spectator = 0;
static int game_ranked = 0;
static int game_player = 0;
static bool show_spectators = false;
static bool show_chat_input = false;
static bool show_chat = false;
static int info_time = 0;
static int chat_time = 0;
static int volume_time = 0;

enum
{
	CMD_CHATMUTED = 1,
	CMD_DELAYRA = 2,
};

static bool CopyFileContents(const char *src, const char *dst)
{
	FILE * fs = NULL;
	if (fopen_s(&fs, src, "rb") == 0 && fs != NULL)
	{
		FILE * fd = NULL;
		if (fopen_s(&fd, dst, "wb") == 0 && fd != NULL)
		{
			fseek(fs, 0L, SEEK_END);
			int size = (int)ftell(fs);
			rewind(fs);

			char buffer[4096];
			while (size > 0)
			{
				fread (buffer, 1, MIN(4096, size), fs);
				fwrite(buffer, 1, MIN(4096, size), fd);
				size-= 4096;
			}
			fflush(fd);
			fclose(fd);
			fclose(fs);
			return true;
		}
		fclose(fs);
	}
	return false;
}

//------------------------------------------------------------------------------------------------------------------------------
// font helpers
//------------------------------------------------------------------------------------------------------------------------------
static void fontWrite(const wchar_t *text, float x, float y, unsigned int color, float alpha, float scale, unsigned int mode)
{
	unsigned int a = (unsigned int)(alpha*255.f) << 24;
	font.SetScale(FS(scale) * frame_ratio * FNT_SCALE, FS(scale) * FNT_SCALE);
	font.Write(FPX(x), FPY(y), 0, (color & 0x00FFFFFF) | a, text, 0, mode, FONT_SPACING);
}

static float fontGetTextWidth(const wchar_t *text, float scale)
{
	font.SetScale(FS(scale) * frame_ratio * FNT_SCALE, FS(scale) * FNT_SCALE);
	return font.GetTextWidth(text, 0, FONT_SPACING) / (float)(frame_dest.right - frame_dest.left);
}


//------------------------------------------------------------------------------------------------------------------------------
// Detector
//------------------------------------------------------------------------------------------------------------------------------
struct Detector {
	enum EType {
		None,
		Store,	// Just store value
		MemEq,  // Equal
		MemGts, // Greater than start
	};
  int type = None;
	int frames = 0;
	bool raw = false;
	char name[64] = {};
	char area[64] = {};
	unsigned int memory_ptr;
	unsigned int memory_value;
	unsigned int memory_bits;
	unsigned int memory_start;
	unsigned int memory_current;

	bool IsOk() const { return type != None; }
	void Load(const char *_name, const char *_area, unsigned int _ptr, unsigned int _type, unsigned int _value, unsigned int _bits);
	void Update(const BurnArea* ba, bool start_detected);
	bool Detected();
	static int GetType(const char *str) {
		if (!strcmp(str, "eq"))
			return MemEq;
		if (!strcmp(str, "gts"))
			return MemGts;
		return Store;
	}
};

void Detector::Load(const char *_name, const char *_area, unsigned int _ptr, unsigned int _type, unsigned int _value, unsigned int _bits)
{
	strcpy(name, _name);
	type = _type;
	raw = !strcmp(_area, "raw");
	// memory based
	strcpy(area, _area);
	memory_ptr = _ptr;
	memory_value = _value;
	memory_bits = _bits;
	memory_start = 0;
	memory_current = 0;
}

void Detector::Update(const BurnArea *ba, bool start_detected)
{
	if (ba) {
		if (!strcmp(ba->szName, area)) {
			// detection process
			memory_current = 0;
			switch (memory_bits) {
				// 32 bits
				case 32:
					memory_current = ((unsigned int *)ba->Data)[memory_ptr>>2];
					break;
				// 16 bits
				case 16:
					memory_current = ((unsigned short *)ba->Data)[memory_ptr>>1];
					break;
				// 8 bits or less
				default:
					memory_current = ((unsigned char *)ba->Data)[memory_ptr];
					break;
			}
			if (start_detected) {
				memory_start = memory_current;
			}

			bool found = false;
			switch (type) {
				case MemEq:
					// Equal to memory value
					found = (memory_current == memory_value);
					break;
				case MemGts:
					// Greater than value at start
					if (!start_detected) {
						found = (memory_current > memory_start);
					}
					break;
			}
			if (found) {
				frames++;
			}
			else {
				frames = 0;
			}
		}
	} else if (raw) {
		// detection process
		memory_current = 0;
		switch (memory_bits) {
			// 32 bits
			case 32:
				memory_current = ReadValueAtHardwareAddress(memory_ptr, 4, 0);
				break;
			// 16 bits
			case 16:
				memory_current = ReadValueAtHardwareAddress(memory_ptr, 2, 0);
				break;
			// 8 bits or less
			default:
				memory_current = ReadValueAtHardwareAddress(memory_ptr, 1, 0);
				break;
		}
		if (start_detected) {
			memory_start = memory_current;
		}

		bool found = false;
		switch (type) {
			case MemEq:
				// Equal to memory value
				found = (memory_current == memory_value);
				break;
			case MemGts:
				// Greater than value at start
				if (!start_detected) {
					found = (memory_current > memory_start);
				}
				break;
		}
		if (found) {
			frames++;
		}
		else {
			frames = 0;
		}
	}
}

bool Detector::Detected()
{
	return (frames >= DETECTOR_FRAMES);
}

//------------------------------------------------------------------------------------------------------------------------------
// game detector
//------------------------------------------------------------------------------------------------------------------------------
struct GameDetector {
	enum {
		ST_NONE,
		ST_WAIT_START,
		ST_WAIT_WINNER,
		ST_WAIT_END
	};
	int state = ST_NONE;
	int frame_start = 0;
	int frame_end = 0;
	bool ranked = false;
	bool raw_detector = false;
	bool run_detector = false;
	int score1 = 0;
	int score2 = 0;
	int winner = 0;
	int num_characters = 0;
	wchar_t characters[MAX_CHARACTERS][32] = {};
	wchar_t char1[32];
	wchar_t char2[32];
	std::vector<Detector> dStart;
	std::vector<Detector> dPlayer1;
	std::vector<Detector> dPlayer2;
	Detector dChar1;
	Detector dChar2;
	void Load(const char *game);
	void Save();
	void Update();
	void UpdateDetectors(struct BurnArea* pba, bool detect_start);
	void Render();
};

void GameDetector::Load(const char *game)
{
	MemoryBuffer buffer;
	char file[MAX_PATH];
	sprintf(file, "detector\\%s.inf", game);
	bool loaded = false;
	if (LoadMemoryBufferDetector(buffer, file, debug_mode)) {
		const char *ini = buffer.data;
		const char *end = buffer.data + buffer.len;
		while (ini > 0 && ini < end) {
			// get line
			char line[256];
			const char *pos = strstr(ini, "\n");
			sprintf(line, "%.*s", pos ? pos-ini : end-ini, ini);
			ini = pos ? pos+1 : 0;

			// scan
			char target[64];
			char name[64];
			char area[64];
			char op[16];
			unsigned int ptr;
			unsigned int value;
			unsigned int bits;
			if (sscanf(line, "%[^=]=%[^,],%[^,],%[^,],0x%X,%d,%d", target, name, area, op, &ptr, &value, &bits) == 7) {
				// detector
				int type = Detector::GetType(op);
				if (!strcmp(target, "start")) {
					dStart.push_back(Detector());
					dStart.back().Load(name, area, ptr, type, value, bits);
					raw_detector = !strcmp(area, "raw");
				}
				else if (!strcmp(target, "player1")) {
					dPlayer1.push_back(Detector());
					dPlayer1.back().Load(name, area, ptr, type, value, bits);
				}
				else if (!strcmp(target, "player2")) {
					dPlayer2.push_back(Detector());
					dPlayer2.back().Load(name, area, ptr, type, value, bits);
				}
				else if (!strcmp(target, "char1")) {
					dChar1.Load(name, area, ptr, type, value, bits);
				}
				else if (!strcmp(target, "char2")) {
					dChar2.Load(name, area, ptr, type, value, bits);
				}
				loaded = true;
			}
			else if (sscanf(line, "char=%d,%[^,]", &value, target) == 2) {
				// character names
				wsprintf(characters[value], _T("%s"), ANSIToTCHAR(target, NULL, NULL));
			}
		}
	}
	// something found, set state
	if (loaded) {
		DetectorSetState(ST_WAIT_START, 0, 0);
		VidOverlaySetGameScores(0, 0);
		VidOverlaySetGameSpectators(0);
		VidSSetGameScores(0, 0);
		VidSSetGameSpectators(0);
		run_detector = true;
	}
	else {
		DetectorSetState(ST_NONE, 0, 0);
	}
}

int __cdecl GetMemoryAcbDetectorUpdate(struct BurnArea* pba);

void GameDetector::Update()
{
	// not loaded
	if (state == ST_NONE && debug_mode) {
		return;
	}

	// reset score changed update
	winner = 0;

	// memory based
	if (raw_detector) {
		UpdateDetectors(NULL, true);
	} else {
		BurnAcb = GetMemoryAcbDetectorUpdate;
		BurnAreaScan(ACB_MEMORY_RAM | ACB_READ, NULL);
	}

	// If we detect F3 on P1, disable detector state (some games write values on P1/P2 winners on reset)
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));
	for (unsigned int i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		struct GameInp *pgi = &GameInp[i];
		if (pgi->nInput == GIT_SWITCH && pgi->Input.pVal && !strcmp(bii.szInfo, "reset")) {
			if (*pgi->Input.pVal) {
				state = ST_WAIT_START;
			}
		}
	}

	// detect start
	bool start_detected = false;
	for (size_t i = 0; i < dStart.size(); i++) {
		start_detected |= dStart[i].Detected();
	}

	// run state
	switch (state)
	{
		// wait start
		case ST_WAIT_START:
			if (start_detected) {
				DetectorSetState(ST_WAIT_WINNER, score1, score2);
				if (bVidSaveOverlayFiles) {
					VidOverlaySaveFiles(false, false, true, 0); // to save characters (if there's any detected)
				}
			}
			break;
		// playing
		case ST_WAIT_WINNER:
			if ((frame_time - frame_start) > START_FRAMES) {
				// detect player1
				bool player1_detected = false;
				for (size_t i = 0; i < dPlayer1.size(); i++) {
					player1_detected |= dPlayer1[i].Detected();
				}
				// detect player2
				bool player2_detected = false;
				for (size_t i = 0; i < dPlayer2.size(); i++) {
					player2_detected |= dPlayer2[i].Detected();
				}
				// player1
				if (player1_detected) {
					score1++;
					DetectorSetState(ST_WAIT_START, score1, score2);
					VidOverlaySetGameScores(score1, score2, 1);
					VidSSetGameScores(score1, score2);
					winner = 1;
				}
				// player2
				if (player2_detected) {
					score2++;
					DetectorSetState(ST_WAIT_START, score1, score2);
					VidOverlaySetGameScores(score1, score2, 2);
					VidSSetGameScores(score1, score2);
					winner = 2;
				}
			}
			break;
	}

	if (game_ranked > 1 && (score1 == game_ranked || score2 == game_ranked)) {
		frame_end = frame_time;
	}
}

void GameDetector::UpdateDetectors(struct BurnArea* pba, bool detect_start)
{
	// update detectors
	bool start_detected = false;
	for (size_t i = 0; i < dStart.size(); i++) {
		dStart[i].Update(pba, false);
		start_detected |= dStart[i].Detected() && detect_start;
	}
	for (size_t i = 0; i < dPlayer1.size(); i++) {
		dPlayer1[i].Update(pba, start_detected);
	}
	for (size_t i = 0; i < dPlayer2.size(); i++) {
		dPlayer2[i].Update(pba, start_detected);
	}
	if (dChar1.IsOk()) {
		dChar1.Update(pba, start_detected);
	}
	if (dChar2.IsOk()) {
		dChar2.Update(pba, start_detected);
	}
}

void GameDetector::Render()
{
	// not loaded
	if (state == ST_NONE) {
		return;
	}

	bool draw_detector_info = debug_mode;

	// run state
	switch (state)
	{
		// wait start
		case ST_WAIT_START:
			if (draw_detector_info) {
				fontWrite(_T("Waiting Game Start"), frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * 1, 0xFFFFFFFF, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
			}
			break;
		// playing
		case ST_WAIT_WINNER:
			if (draw_detector_info) {
				fontWrite(_T("Waiting Game Winner"), frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * 1, 0xFFFFFFFF, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
			}
			break;
		// wait end
		case ST_WAIT_END:
			if (draw_detector_info) {
				fontWrite(_T("Waiting Game End"), frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * 1, 0xFFFFFFFF, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
			}
			break;
	}

	// debug detections
	if (draw_detector_info) {
		wchar_t buffer[128];
		int y = 3;
		// start
		for (size_t i = 0; i < dStart.size(); i++) {
			bool detected = dStart[i].Detected();
			swprintf(buffer, 128, _T("%.32hs (%d / %d)"), dStart[i].name, dStart[i].memory_start, dStart[i].memory_current);
			fontWrite(buffer, frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * (y++), detected ? 0xFFA0FF00 : 0xFFBBBBBB, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
		}
		// p1
		for (size_t i = 0; i < dPlayer1.size(); i++) {
			bool detected = dPlayer1[i].Detected();
			swprintf(buffer, 128, _T("%.32hs (%d / %d)"), dPlayer1[i].name, dPlayer1[i].memory_start, dPlayer1[i].memory_current);
			fontWrite(buffer, frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * (y++), detected ? 0xFFA0FF00 : 0xFFBBBBBB, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
		}
		// p2
		for (size_t i = 0; i < dPlayer2.size(); i++) {
			bool detected = dPlayer2[i].Detected();
			swprintf(buffer, 128, _T("%.32hs (%d / %d)"), dPlayer2[i].name, dPlayer2[i].memory_start, dPlayer2[i].memory_current);
			fontWrite(buffer, frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * (y++), detected ? 0xFFA0FF00 : 0xFFBBBBBB, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
		}
		// char1
		if (dChar1.IsOk()) {
			swprintf(buffer, 128, _T("%.32hs (%d / %s)"), dChar1.name, dChar1.memory_current, dChar1.memory_current < MAX_CHARACTERS ? characters[dChar1.memory_current] : L"");
			fontWrite(buffer, frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * (y++), 0xFFBBBBBB, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
		}
		// char2
		if (dChar2.IsOk()) {
			swprintf(buffer, 128, _T("%.32hs (%d / %s)"), dChar2.name, dChar2.memory_current, dChar2.memory_current < MAX_CHARACTERS ? characters[dChar2.memory_current] : L"");
			fontWrite(buffer, frame_width - 0.005f, frame_height - 0.003f - FNT_MED * FNT_SEP * (y++), 0xFFBBBBBB, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
		}
	}
}

static GameDetector gameDetector;

static int __cdecl GetMemoryAcbDetectorUpdate(struct BurnArea* pba)
{
	bool gameDetector2p = true;

	// Stage selector for SSF2XJR1
	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "ssf2xjr1")) {
		if (!strcmp("CpsRamFF", pba->szName)) {
			// 0x82F6 -> select stage
			if (ReadValueAtHardwareAddress(0xFF82F6, 1, 0)) {
				struct BurnInputInfo bii;
				memset(&bii, 0, sizeof(bii));
				// Next stage on the array of used stages
				int addr = 0xE18A;
				while (((unsigned char *)pba->Data)[addr] == 0xFF) {
					addr++;
					if (addr > 0xE199) {
						break;
					}
				}
				if (addr <= 0xE199) {
					// Check p1 cursor is enabled
					if (((unsigned char *)pba->Data)[0x87DC] == 0) {
						// P1 Coin
						BurnDrvGetInputInfo(&bii, 0);
						if (bii.pVal && *bii.pVal) {
							((unsigned char *)pba->Data)[addr] = ((unsigned char *)pba->Data)[0x84F0];
						}
					}
					// Check p2 cursor is enabled
					if (((unsigned char *)pba->Data)[0x8BDC] == 0) {
						// P2 Coin
						BurnDrvGetInputInfo(&bii, 12);
						if (bii.pVal && *bii.pVal) {
							((unsigned char *)pba->Data)[addr] = ((unsigned char *)pba->Data)[0x88F0];					}
					}
				}
			}
			// do not run detector if not in 2p mode
			if (((unsigned char *)pba->Data)[0x87DC] == 0 || ((unsigned char *)pba->Data)[0x8BDC] == 0) {
				gameDetector2p = false;
			}
		}
	}

	// Stage selector for SFA3, introduced in version 3
	if (kNetVersion >= NET_VERSION_SFA3_STAGE) {
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "sfa3")) {
			const char stages[] = { 0,2,4,6,8,10,12,14,16,18,42,22,24,26,28,30,32,34,0,34,0,42,44,0,48,50,52,54,56,58,42,42 };
			struct BurnInputInfo bii;
			memset(&bii, 0, sizeof(bii));
			// Check p1 cursor is enabled
			if (ReadValueAtHardwareAddress(0xFF855B, 2, 0) == 0x400) {
				// P1 Coin
				BurnDrvGetInputInfo(&bii, 0);
				if (bii.pVal && *bii.pVal) {
					WriteValueAtHardwareAddress(0xFFFFD1, stages[ReadValueAtHardwareAddress(0xFF8502, 1, 0)] + 1, 1, 0);
				}
			}
			// Check p2 cursor is enabled
			if (ReadValueAtHardwareAddress(0xFF895B, 2, 0) == 0x400 && ReadValueAtHardwareAddress(0xFF855B, 2, 0) != 0x400) {
				// P1 Coin
				BurnDrvGetInputInfo(&bii, 12);
				if (bii.pVal && *bii.pVal) {
					WriteValueAtHardwareAddress(0xFFFFD1, stages[ReadValueAtHardwareAddress(0xFF8902, 1, 0)] + 1, 1, 0);
				}
			}

			// write position
			int value = ReadValueAtHardwareAddress(0xFF8101 + 7*2, 2, 0);
			int stage = ReadValueAtHardwareAddress(0xFFFFD1, 1, 0) - 1;
			int timer = ReadValueAtHardwareAddress(0xFF8109, 2, 0);
			if (timer >= 25374 && timer <= 25400) {
				//WriteValueAtHardwareAddress(0x618002, 0xFF09, 2, 0);
			}
			if (value == 0x01 && stage >= 0) {
				for (int i = 0; i < sizeof(stages); i++) {
					// make sure we are writing the proper stage
					if (stages[i] == stage) {
						// write stage
						WriteValueAtHardwareAddress(0xFF8101, stage, 1, 0);
						// reset stage
						WriteValueAtHardwareAddress(0xFFFFD1, 0, 1, 0);
						break;
					}
				}
			}
		}
	}

	// Stage selector for SF2HF
	/*
	if (kNetVersion >= ) {
		0 "Disabled" 1 "Balrog",
		0, 0xFFDD5F, 0x0A 2 "Blanka", 0, 0xFFDD5F, 0x02 3 "Chun-Li", 0, 0xFFDD5F, 0x05 4 "Dhalsim", 0, 0xFFDD5F, 0x07 5 "E.Honda", 0, 0xFFDD5F, 0x01 6 "Guile", 0, 0xFFDD5F, 0x03 7 "Ken", 0, 0xFFDD5F, 0x04 8 "M.Bison", 0, 0xFFDD5F, 0x08 9 "Ryu", 0, 0xFFDD5F, 0x00 10 "Sagat", 0, 0xFFDD5F, 0x09 11 "Vega", 0, 0xFFDD5F, 0x0B 12 "Zangief", 0, 0xFFDD5F, 0x06
	}
	*/

	gameDetector.UpdateDetectors(pba, gameDetector2p);

	return 0;
}

void DetectorLoad(const char *game, bool debug, int seed)
{
	debug_mode = debug;
 	gameDetector.Load(game);

	// initial state
	if (kNetVersion >= NET_VERSION_KOF98_MOOD && seed) {
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "kof98")) {
			srand(seed);
			for (int i = 0x001000A1; i <= 0x001000C2; i++) {
				int value = rand() % 3;
				WriteValueAtHardwareAddress(i, value, 1, 0);
			}
		}
	}
}

void DetectorUpdate()
{
	frame_time++;

	// warnings
	stats_line1_warning--;
	if (stats_line1_warning < 0) {
		stats_line1_warning = 0;
	}

	stats_line2_warning--;
	if (stats_line2_warning < 0) {
		stats_line2_warning = 0;
	}

	stats_line3_warning--;
	if (stats_line3_warning < 0) {
		stats_line3_warning = 0;
	}

	bool detector_enabled = gameDetector.run_detector && !gameDetector.frame_end && (gameDetector.state != GameDetector::ST_NONE);

	// run game detector
	if (detector_enabled) {
		gameDetector.Update();
		// character detector
		int char1 = gameDetector.dChar1.IsOk() ? gameDetector.dChar1.memory_current : -1;
		int char2 = gameDetector.dChar2.IsOk() ? gameDetector.dChar2.memory_current : -1;
		if (char1 >= 0) {
			wcscpy(gameDetector.char1, gameDetector.characters[gameDetector.dChar1.memory_current]);
		}
		if (char2 >= 0) {
			wcscpy(gameDetector.char2, gameDetector.characters[gameDetector.dChar2.memory_current]);
		}
		// in ranked games, if there is a winner send to server and terminate game if it was a FTx
		if (game_ranked && !game_spectator && gameDetector.winner != 0) {
			char temp[32];
			// send ReceiveWinner message
			sprintf(temp, "%d,%d,%d,%d,%d", gameDetector.winner, gameDetector.score1, gameDetector.score2, char1, char2);
			QuarkSendChatCmd(temp, 'W');
			if (gameDetector.frame_end) {
				VidOverlayAddChatLine(_T("System"), _T("Ranked game finished"));
			}
		}
	}

	// finish quark
	if (gameDetector.frame_end && (frame_time - gameDetector.frame_end) > END_FRAMES) {
		gameDetector.frame_end = INT_MAX;
		QuarkEnd();
	}

	// save info if needed
	VidOverlaySaveInfo();
}


void DetectorSetState(int state, int score1, int score2, int start1, int start2)
{
	if (state == GameDetector::ST_WAIT_START) {
		gameDetector.frame_start = frame_time;
	}
	gameDetector.state = state;
	gameDetector.score1 = score1;
	gameDetector.score2 = score2;
	if (gameDetector.dPlayer1.size() > 0 && start1 != 0) gameDetector.dPlayer1[0].memory_start = start1;
	if (gameDetector.dPlayer2.size() > 0 && start2 != 0) gameDetector.dPlayer2[0].memory_start = start2;
}

void DetectorGetState(int &state, int &score1, int &score2, int &start1, int &start2)
{
	state = gameDetector.state;
	score1 = gameDetector.score1;
	score2 = gameDetector.score2;
	start1 = (gameDetector.dPlayer1.size() > 0) ? gameDetector.dPlayer1[0].memory_start : 0;
	start2 = (gameDetector.dPlayer2.size() > 0) ? gameDetector.dPlayer2[0].memory_start : 0;
}

//------------------------------------------------------------------------------------------------------------------------------
// text helper
//------------------------------------------------------------------------------------------------------------------------------
struct Text
{
	wchar_t str[300] = {};
	unsigned int col = 0;
	Text();
	void Set(const wchar_t *text);
	void Copy(const Text& text);
	void Render(float x, float y, float alpha, float scale, unsigned int mode);
};

Text::Text()
{
	col = 0xFFFFFFFF;
}

void Text::Set(const wchar_t *text)
{
	wcscpy(str, text);
}

void Text::Copy(const Text& text)
{
	col = text.col;
	Set(text.str);
}

void Text::Render(float x, float y, float alpha, float scale, unsigned int mode)
{
	if (str[0]) {
		fontWrite(str, x, y, col, alpha, scale, mode);
	}
}


//------------------------------------------------------------------------------------------------------------------------------
// sprite helper
//------------------------------------------------------------------------------------------------------------------------------
struct Sprite
{
	enum {
		LEFT = 0,
		TOP = 0,
		HCENTER = 1 << 0,
		VCENTER = 1 << 1,
		RIGHT = 1 << 2,
		BOTTOM = 1 << 3,
		TOPLEFT = LEFT | TOP,
		CENTER = HCENTER | VCENTER
	};
	IDirect3DTexture9 *texture = NULL;
	CDynRender *renderer = NULL;
	int col = 0xffffffff;
	int width = 0;
	int height = 0;

	void Init(const wchar_t *file, CDynRender* render, unsigned color = 0xffffffff, unsigned int w = 0, unsigned int h = 0);
	void End();
	void Render(float x, float y, float scale, unsigned int anchor = TOPLEFT);
	void Render(float x, float y, float w, float h, float u0, float v0, float u1, float v1);
};

void Sprite::Init(const wchar_t *file, CDynRender* render, unsigned int color, unsigned int w, unsigned int h)
{
	renderer = render;
	col = color;
	LoadD3DTextureFromFile(pD3DDevice, TCHARToANSI(file, NULL, NULL), texture, width, height);
}

void Sprite::End()
{
	RELEASE(texture);
}

void Sprite::Render(float x, float y, float scale, unsigned int anchor)
{
	if (!texture) {
		return;
	}

	renderer->SetTexture(texture);

	x = FPX(x);
	y = FPY(y);
	float w = FS(width * scale);
	float h = FS(height * scale);
	float u0 = 0.5f / w;
	float u1 = 1 - 0.5f / w;
	float v0 = 0.5f / h;
	float v1 = 1 - 0.5f / h;

	// anchor
	if (anchor & HCENTER) {
		x -= w*0.5f;
	}
	if (anchor & VCENTER) {
		y -= h*0.5f;
	}
	if (anchor & RIGHT) {
		x -= w;
	}
	if (anchor & BOTTOM) {
		y -= h;
	}

	// add sprite quad
	renderer->VtxColor(col);
	renderer->VtxTexCoord(u0, v0);
	renderer->VtxPos(x, y, 0);
	renderer->VtxTexCoord(u1, u0);
	renderer->VtxPos(x + w, y, 0);
	renderer->VtxTexCoord(u1, u1);
	renderer->VtxPos(x + w, y + h, 0);
	renderer->VtxTexCoord(u0, u1);
	renderer->VtxPos(x, y + h, 0);
}

void Sprite::Render(float x, float y, float w, float h, float u0, float v0, float u1, float v1)
{
	if (!texture) {
		return;
	}

	renderer->SetTexture(texture);

	x = FPX(x);
	y = FPY(y);

	// add sprite quad
	renderer->VtxColor(col);
	renderer->VtxTexCoord(u0, v0);
	renderer->VtxPos(x, y, 0);
	renderer->VtxTexCoord(u1, v0);
	renderer->VtxPos(x + w, y, 0);
	renderer->VtxTexCoord(u1, v1);
	renderer->VtxPos(x + w, y + h, 0);
	renderer->VtxTexCoord(u0, v1);
	renderer->VtxPos(x, y + h, 0);
}


//------------------------------------------------------------------------------------------------------------------------------
// player info
//------------------------------------------------------------------------------------------------------------------------------
struct PlayerInfo {
	Text name;
	Text score;
	Text country;
	Text character;
	int rank;
};

//------------------------------------------------------------------------------------------------------------------------------
// jitter info
//------------------------------------------------------------------------------------------------------------------------------
#define PINGSIZE 10
static int jitterPingArray[PINGSIZE] = {};
static int jitterArray[PINGSIZE] = {};
static int jitterArrayPos = 0;
static int jitterPingAvg = 0;
static int jitterAvg = 0;

//------------------------------------------------------------------------------------------------------------------------------
// overlay
//------------------------------------------------------------------------------------------------------------------------------
static PlayerInfo player1;
static PlayerInfo player2;
static Sprite inputs_spr;
static Sprite scanlines_spr;
static Sprite background_spr;
static Sprite spectators_spr;
static Sprite badges_spr[NUM_RANKS];
static Text spectators;
static Text chat_names[CHAT_LINES];
static Text chat_lines[CHAT_LINES];
static Text chat_input;
static Text system_message;
static Text stats_line1;
static Text stats_line2;
static Text stats_line3;
static Text info;
static Text volume;
static Text warning;

void InputRender(float &x, float &y, int idx)
{
	static float w = 3.f;
	static float h = 3.f;
	inputs_spr.Render(x, y, FS(w), FS(h), 0.f * 8.f / 16.f, idx * 8.f / 120.f, 8.f / 16.f, (idx + 1) * 8.f / 120.f);
}

//------------------------------------------------------------------------------------------------------------------------------
// overlay
//------------------------------------------------------------------------------------------------------------------------------
void VidOverlayInit(IDirect3DDevice9Ex *device)
{
	pD3DDevice = device;
	renderer.Init(pD3DDevice);
	font.Init("ui/font.fnt", NULL, &renderer);
	inputs_spr.Init(_T("ui/inputs.png"), &renderer, 0xFFFFFFFF);
	scanlines_spr.Init(_T("ui/scanlines.png"), &renderer, 0xFF000000);
	background_spr.Init(_T("ui/background.png"), &renderer, 0xFFFFFFFF);
	spectators_spr.Init(_T("ui/spectators.png"), &renderer);
	for (int i = 0; i < NUM_RANKS; i++) {
		wchar_t temp[MAX_PATH];
		wsprintf(temp, _T("ui/rank%d.png"), i);
		badges_spr[i].Init(temp, &renderer);
	}
	DeleteFileA("fightcade/started.inf");

#ifdef TEST_OVERLAY
	VidOverlaySetGameInfo(_T("Bender#2,10"), _T("Iori#3,8"), false, 10, 0);
	VidOverlaySetGameSpectators(13);
	VidOverlayAddChatLine(_T("Bender"), _T("test line 1"));
	VidOverlayAddChatLine(_T("Iori"), _T("good game!"));
	VidOverlayAddChatLine(_T("System"), _T("You guys chat too much"));
	VidOverlayAddChatLine(_T("Bender"), _T("omg"));
	VidOverlayAddChatLine(_T("System"), _T("Too much buttons pressed, autofire detected?"));
	VidOverlayAddChatLine(_T("Bender"), _T("lorem ipsum chat text"));
	VidOverlayAddChatLine(_T("Bender"), _T("this is another lorem ipsum chat text, something longer and longer and longer"));
	VidOverlayAddChatLine(_T("Iori"), _T("Jackie Chan will be best new addition to Fightcade!"));
	VidOverlayAddChatLine(_T("Iori"), _T("Día Completo"));
	VidOverlayAddChatLine(_T("Iori"), _T("test line 11, last line"));
	VidOverlaySetSystemMessage(_T("System message"));
#endif
}

void VidOverlayEnd()
{
	renderer.End();
	font.End();
	inputs_spr.End();
	scanlines_spr.End();
	background_spr.End();
	spectators_spr.End();
	for (int i = 0; i < NUM_RANKS; i++) {
		badges_spr[i].End();
	}
	pD3DDevice = NULL;
}

void VidOverlayQuit()
{
	if (kNetGame && game_ranked && gameDetector.state == GameDetector::ST_WAIT_WINNER) {
		QuarkSendChatCmd("quit", 'S');
	}
}

void VidOverlaySetSize(const RECT &dest, float size)
{
	frame_dest = dest;
	frame_scale = (float)(dest.bottom - dest.top) / size;
	frame_adjust = 1.f;
	if (frame_scale < 1.5f)
	{
		frame_adjust = 1.5f / frame_scale;
		frame_scale = 1.5f;
	}
	frame_width = 1.f;
	frame_height = frame_width * (dest.bottom - dest.top) / (dest.right - dest.left);
}

void VidOverlayRender(const RECT &dest, int gameWidth, int gameHeight, int scan_intensity)
{
	if (!pD3DDevice) {
		return;
	}

#ifdef TEST_OVERLAY
	VidOverlaySetChatInput(_T("testing chat input with a long sentence..."));
#endif

	// save to chat history
	if (bVidSaveChatHistory && kNetGame && !kNetSpectator) {
		static bool enabled = false;
		if (!enabled) {
			enabled = true;
			VidOverlayAddChatLine(_T("System"), _T("Chat history will be saved to fightcade/chat_history.txt"));
		}
	}

	VidOverlaySetSize(dest, 260.f);

	// set render states
	pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pD3DDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	pD3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	pD3DDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	pD3DDevice->SetRenderState(D3DRS_COLORVERTEX, FALSE);
	pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	pD3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pD3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pD3DDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);

	pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

	pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	pD3DDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	pD3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	pD3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	renderer.BeginRender();

	// scanlines
	if (scan_intensity > 0) {
		scanlines_spr.col = scan_intensity << 24;
		scanlines_spr.Render(0.f, 0.f, (float)frame_dest.right, (float)(frame_dest.bottom - frame_dest.top), 0.f, 0.f, (float)gameWidth, (float)gameHeight);
	}

	renderer.Flush();

	pD3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	pD3DDevice->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	renderer.BeginRender();

#ifdef TEST_VERSION
	fontWrite(TEST_VERSION, 0.005f, 0.005f, 0xffffffff, 1.f, FNT_MED, FONT_ALIGN_LEFT);
	wchar_t buf[128];
	wsprintf(buf, _T("%d"), nFramesEmulated);
	fontWrite(buf, 0.005f, 0.005f + FNT_MED * 0.8f * FNT_SEP, 0xffffffff, 1.f, FNT_MED, FONT_ALIGN_LEFT);
#endif

	// system message
	system_message.Render(frame_width * 0.5f, frame_height * 0.2f, 1.f, FNT_MED * 1.8f, FONT_ALIGN_CENTER);

	if (frame_time < volume_time) {
		// volume
		volume.Render(frame_width - 0.0035f, 0.003f, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
	}
	else if (bShowFPS) {
		// stats (fps & ping)
		stats_line1.col = (stats_line1_warning >= 100) ? 0xffff0000 : 0xffffffff;
		stats_line1.Render((bShowFPS==2) ? frame_width - 0.0015f : frame_width - 0.0035f, 0.003f, 0.90f, FNT_MED*0.9, FONT_ALIGN_RIGHT);
		if (bShowFPS > 1) {
			stats_line2.col = (stats_line2_warning >= 100) ? 0xffff0000 : 0xffffffff;
			stats_line2.Render((jitterAvg >= 10) ? frame_width - 0.0052f : frame_width - 0.0035f, 0.023f, 0.90, FNT_MED*0.9, FONT_ALIGN_RIGHT);
		}
		if (bShowFPS > 2) {
			stats_line3.col = (stats_line3_warning >= 100) ? 0xffff0000 : 0xffffffff;
			stats_line3.Render(frame_width - 0.0035f, 0.043f, 0.90, FNT_MED*0.9, FONT_ALIGN_RIGHT);
		}
	}

	// text info
	if (frame_time < info_time) {
		info.Render(0.0035f, 0.003f, 1.f, FNT_MED, FONT_ALIGN_LEFT);
	}

	if (game_enabled) {
		// ranked flag
		bool detector_enabled = (game_ranked || bVidUnrankedScores) && gameDetector.run_detector && (gameDetector.state != GameDetector::ST_NONE);

		if (bVidOverlay) {
			float scale = bVidBigOverlay ? 1.6f : 1.f;
			VidOverlaySetSize(dest, 260.f / scale);

			// positions
			float bar_h = 0.17f;
			float x = frame_width * 0.5f;
			float xs = frame_adjust * 0.025f * scale;
			float xn = frame_adjust * (detector_enabled ? 0.05f : 0.019f) * scale;
			float yn = frame_adjust * 0.002f;
			float yb = frame_adjust * 0.003f;

			// background
			background_spr.Render(frame_width * 0.5f, 0, bar_h, Sprite::HCENTER);

			// VS / FT
			wchar_t vs[16] = _T("VS");
			if (game_ranked > 1) {
				swprintf(vs, 16, _T("FT%d"), game_ranked);
				fontWrite(vs, x, yn, 0xFFFFB200, 1.f, FNT_MED, FONT_ALIGN_CENTER);
			}
			else {
				fontWrite(vs, x, yn, 0xFF808080, 1.f, FNT_MED, FONT_ALIGN_CENTER);
			}

			// matchinfo
			if (player1.name.str[0]) {
				player1.name.Render(x - xn, yn, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
				if (detector_enabled) {
					if (player1.score.str[0]) {
						player1.score.col = 0xFF33F1E5;
						player1.score.Render(x - xs, yn, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
					}
					float len = fontGetTextWidth(player1.name.str, FNT_MED);
					if (game_ranked && player1.rank != -1) {
						badges_spr[player1.rank].Render(x - xn - len - 0.019f * scale, yb, bar_h, Sprite::HCENTER);
					}
				}
			}
			if (player2.name.str[0]) {
				player2.name.Render(x + xn, yn, 1.f, FNT_MED, FONT_ALIGN_LEFT);
				if (detector_enabled) {
					if (player2.score.str[0]) {
						player2.score.col = 0xFF33F1E5;
						player2.score.Render(x + xs, yn, 1.f, FNT_MED, FONT_ALIGN_LEFT);
					}
					float len = fontGetTextWidth(player2.name.str, FNT_MED);
					if (game_ranked && player2.rank != -1) {
						badges_spr[player2.rank].Render(x + xn + len + 0.019f * scale, yb, bar_h, Sprite::HCENTER);
					}
				}
			}

			VidOverlaySetSize(dest, 260.f);

			// spectators
			if (show_spectators) {
				spectators_spr.Render(x, frame_height, bar_h, Sprite::HCENTER | Sprite::BOTTOM);
				spectators.Render(x + 0.012f, frame_height - 0.007f - FNT_MED * 0.8f * FNT_SEP, 1.f, FNT_MED, FONT_ALIGN_CENTER);
			}
		}

		float cx = 0.005f;
		float cy = frame_height - 0.005f - FNT_MED * FNT_SEP * 1.1f;

		// chat input
		if (show_chat_input) {
			float len = fontGetTextWidth(chat_input.str, FNT_CHAT);
			chat_input.Render(cx, cy, 1.f, FNT_CHAT, FONT_ALIGN_LEFT);
			if ((frame_time % 16) < 8) {
				fontWrite(_T("_"), cx + len + 0.002f, cy, 0xFFFFFFFF, 1.f, FNT_CHAT, FONT_ALIGN_LEFT);
			}
		}

		// chat
		if ((frame_time < chat_time) || show_chat || show_chat_input) {
			// chat lines
			for (int i = 0; i < CHAT_LINES; i++) {
				cy -= FNT_CHAT * FNT_SEP;
				float len = fontGetTextWidth(chat_names[i].str, FNT_CHAT);
				chat_names[i].Render(cx, cy, 1.f, FNT_CHAT, FONT_ALIGN_LEFT);
				chat_lines[i].Render(cx + len + 0.006f, cy, 1.f, FNT_CHAT, FONT_ALIGN_LEFT);
			}
		}

		show_chat_input = false;
		show_chat = false;
	}

	// read inputs for display
	if (bVidShowInputs)
	{
		// player
		for (int k = 0; k < 2; k++) {
			if (kNetGame && !kNetSpectator && k != game_player)
				continue;
			// buffer (local = 0, online = 1)
			for (int j = 0; j < 2; j++) {
				if (kNetGame && kNetSpectator && j == 1)
					continue;
				// input buffer
				for (int i = 0; i < INPUT_DISPLAY; i++) {
					const float sep = 0.01f;
					const float size = 1.f;
					const float fy = 0.f;
					const float fx = 0.15f;
					int player = k;
					int buffer = j;
					// get player/buffer depending on netgame
					if (kNetGame && kNetSpectator && j == 0)
						buffer = 1;
					if (kNetGame && !kNetSpectator && j == 0)
						player = 0;
					// positions
					float x = k == 0 ? 0.05f + j * fx : 0.85f - j * fx;
					float y = 0.05f + i * sep;
					INT32 values = display_inputs[player][buffer][i].values;
					wchar_t buf[128];
					wsprintf(buf, _T("%d"), display_inputs[player][buffer][i].frame);
					wchar_t type[2] = _T("");
					INT32 color = 0;
					switch (display_inputs[player][buffer][i].state) {
						case 0: color = 0xFFFFFFFF; break; // Regular frame
						case 1: color = 0xFFFF6000; wsprintf(type, _T("-")); break; // Rollback frame
						case 2: color = 0xFF6000FF; break; // Processed frame
						case 3: color = 0xFF00FF60; wsprintf(type, _T("*")); values = 0; break;
						case 4: color = 0xFFFF00FF; wsprintf(type, _T("R")); break;
					}
					if (color) {
						if (type[0]) {
							static float fs = 0.05f;
							fontWrite(type, x - fs, y + fy, color, 1.f, FNT_SMA * size, FONT_ALIGN_CENTER);
						}
						fontWrite(buf, x, y + fy, color, 1.f, FNT_SMA * size, FONT_ALIGN_CENTER);
						x+= sep*2.5f;
						if (values & (1 << INPUT_LEFT)) { InputRender(x, y, 0); x+= sep; }
						if (values & (1 << INPUT_RIGHT)) { InputRender(x, y, 1); x+= sep; }
						if (values & (1 << INPUT_UP)) { InputRender(x, y, 2); x+= sep; }
						if (values & (1 << INPUT_DOWN)) { InputRender(x, y, 3); x+= sep; }
						if (values & (1 << INPUT_LP)) { InputRender(x, y, 8); x+= sep; }
						if (values & (1 << INPUT_MP)) { InputRender(x, y, 9); x+= sep; }
						if (values & (1 << INPUT_HP)) { InputRender(x, y, 10); x+= sep; }
						if (values & (1 << INPUT_LK)) { InputRender(x, y, 11); x+= sep; }
						if (values & (1 << INPUT_MK)) { InputRender(x, y, 12); x+= sep; }
						if (values & (1 << INPUT_HK)) { InputRender(x, y, 13); x+= sep; }
					}
				}
			}
		}
	}

	gameDetector.Render();

	renderer.EndRender();
}


//------------------------------------------------------------------------------------------------------------------------------
// overlay API
//------------------------------------------------------------------------------------------------------------------------------
void VidOverlaySetGameInfo(const wchar_t *name1, const wchar_t *name2, int spectator, int ranked, int player)
{
	game_enabled = 1;
	game_spectator = spectator;
	game_ranked = ranked;
	game_player = player;
#ifdef TEST_OVERLAY
	gameDetector.run_detector = true;
#endif
	// player1#rank,score,country
	if (name1) {
		int len1 = wcslen(name1);
		int pos1 = 0;
		for (int i = len1; i > 0 && !pos1; --i) {
			if (name1[i] == '#') {
				pos1 = i;
			}
		}
		int score1;
		swscanf(&name1[pos1], _T("#%d,%d,%s"), &player1.rank, &score1, &player1.country.str);
		swprintf(player1.name.str, 256, _T("%.*s"), pos1, name1);
	}
	// player2#rank,score,country
	if (name2) {
		int len2 = wcslen(name2);
		int pos2 = 0;
		for (int i = len2; i > 0 && !pos2; --i) {
			if (name2[i] == '#') {
				pos2 = i;
			}
		}
		int score2;
		swscanf(&name2[pos2], _T("#%d,%d,%s"), &player2.rank, &score2, &player2.country.str);
		swprintf(player2.name.str, 256, _T("%.*s"), pos2, name2);
	}

	// update scores
	VidOverlaySetGameScores(gameDetector.score1, gameDetector.score2);

	// save overlay to data files
	if (bVidSaveOverlayFiles) {
		VidOverlaySaveFiles(true, false, false, 0); // save only info
	}
}

void VidOverlaySetGameScores(int score1, int score2, int winner)
{
	wchar_t buf[32];
	player1.score.Set(_itow(score1, buf, 10));
	player2.score.Set(_itow(score2, buf, 10));

	// save overlay to data files
	if (bVidSaveOverlayFiles) {
		VidOverlaySaveFiles(false, true, false, winner); // save only scores
	}
}

void VidOverlaySetGameSpectators(int num)
{
	wchar_t buf[32];
	spectators.Set(_itow(MAX(0, num - 1), buf, 10));	// remove server spectator
	show_spectators = num > 1;
}

void VidOverlaySetSystemMessage(const wchar_t *text)
{
	system_message.Set(text);
}

void SendToPeer(int delay, int runahead) {
	char buffer[32];
	sprintf(buffer, "%d,%d,%d,%d", CMD_DELAYRA, game_player, delay, runahead);
	QuarkSendChatCmd(buffer, 'C');
}

static int op_delay = 0;
static int op_runahead = 0;
static int prev_runahead = -1;
static int bOpInfoRcvd = false;

extern int nRollbackFrames;
extern int nRollbackCount;

static int nLastRollbackCount = 0;
static UINT32 nLastRollbackFrames = 0;
static UINT32 nRollbackRealtime = 0;
static UINT32 nLastRollbackAt = 0;
static UINT32 nMaxRollback=0;
static UINT32 nRollbacksIn1Cycle = 0;
static UINT32 rollbackPct = 0;
static UINT32 nLastCount = 0;
static UINT32 nRollbacks1CycleAgo = 0;

void VidOverlaySetStats(double fps, int ping, int delay)
{
	if (ping > 0 && nRollbackCount > 0 && prev_runahead != nVidRunahead) {
		prev_runahead = nVidRunahead;
		SendToPeer(delay, nVidRunahead);
	}

	if (bShowFPS <= 0) return;

	wchar_t buf_line1[64];
	wchar_t buf_line2[64];
	wchar_t buf_line3[64];
	if ((game_spectator || ping <= 0 || ping > 40000) && (bShowFPS >= 1)) {
		if (game_spectator || (bShowFPS > 0 && bShowFPS < 3))swprintf(buf_line1, 64, _T("%2.2f fps"), fps);
		else if (bShowFPS >= 3) swprintf(buf_line1, 64, _T("%2.2f fps  |  ra%d "), fps, nVidRunahead);
		stats_line1.Set(buf_line1);
	}
	else {
		if (bShowFPS >= 1) {
			// rollback frames
			nMaxRollback = 3 + ((ping/2)/(100000/nBurnFPS));
			if (nLastRollbackFrames > 0 && nLastRollbackCount > 0) {
				if (nRollbackCount > nLastRollbackCount) {
					nRollbackRealtime = (nRollbackFrames-nLastRollbackFrames)/(nRollbackCount-nLastRollbackCount);
					nLastRollbackAt = nFramesEmulated;
					if ( nRollbackRealtime > nMaxRollback) {
						if (nFramesEmulated > 1000 && nRollbackRealtime > 0) {
							VidOverlaySetWarning(180, 1);
						}
					}

				} else if (nFramesEmulated > nLastRollbackAt + 600) {
					nRollbackRealtime = 0;
				}
			}
			nLastRollbackCount = nRollbackCount;
			nLastRollbackFrames = nRollbackFrames;

			if (bShowFPS == 1 ) swprintf(buf_line1, 64, _T(" %2.2f fps  |  d%d-ra%d "), fps, delay, nVidRunahead);
			else if (bShowFPS == 2) swprintf(buf_line1, 64, _T(" %2.2f fps  |      d%d-ra%d        "), fps, delay, nVidRunahead);
			else swprintf(buf_line1, 64, _T(" %2.2f fps  |  Rollback %df "), fps, nRollbackRealtime);
			stats_line1.Set(buf_line1);
		}
		if (bShowFPS >= 2) {
			// jitter
			if (ping > 0 && nFramesEmulated > 600) {
				int pingSum = 0;
				int jitterSum = 0;
				jitterPingArray[jitterArrayPos] = ping;
				if (jitterArrayPos > 0)
					jitterArray[jitterArrayPos - 1] = abs(jitterPingArray[jitterArrayPos] - jitterPingArray[jitterArrayPos - 1]);
				for (int i = 0; i < PINGSIZE; i++) {
					pingSum += jitterArray[i];
					if (i < PINGSIZE - 1) jitterSum += jitterArray[i];
				}
				jitterPingAvg = pingSum / PINGSIZE;
				jitterAvg = jitterSum / (PINGSIZE - 1);
				jitterArrayPos++;
				if (jitterArrayPos > PINGSIZE) {
					jitterArrayPos = 0;
				}
			}
			if (jitterAvg > jitterPingAvg * 0.15 && jitterAvg > 10) VidOverlaySetWarning(120, 2);

			wchar_t buf_ping[30];
			wchar_t buf_jitter[30];
			if (ping < 1000) wsprintf(buf_ping, _T("Ping %dms  |"), ping);
			else wsprintf(buf_ping, _T("Ping +999ms  |"));
			if (jitterAvg < 10) wsprintf(buf_jitter, _T("  Jitter  %dms  "), jitterAvg);
			else if (jitterAvg < 100) wsprintf(buf_jitter, _T("  Jitter %dms"), jitterAvg);
			else wsprintf(buf_jitter, _T("Jitter +99ms"));
			swprintf(buf_line2, 64, _T("%s%s"), buf_ping, buf_jitter);
			stats_line2.Set(buf_line2);
		}
		if (bShowFPS >= 3) {
			if (nFramesEmulated >= nLastCount + 600) {
				nRollbacksIn1Cycle = nRollbackCount - nRollbacks1CycleAgo;
				nRollbacks1CycleAgo = nRollbackCount;
				rollbackPct = (100 * nRollbacksIn1Cycle)/(nFramesEmulated-nLastCount);
				nLastCount = nFramesEmulated;
			}
			if (rollbackPct >= 50) VidOverlaySetWarning(120, 3);

			//swprintf(buf_line3, 64, _T("Delay %d  | Runahead %d"), delay, nVidRunahead);
			if (!bOpInfoRcvd) {
				if (game_player == 0) swprintf(buf_line3, 64, _T("P1: d%d-ra%d  |  P2: d?-ra?    "), delay, nVidRunahead);
				else swprintf(buf_line3, 64, _T("P1: d?-ra?  |  P2: d%d-ra%d   "), delay, nVidRunahead);
			} else {
				if (game_player == 0) swprintf(buf_line3, 64, _T("P1: d%d-ra%d  |  P2: d%d-ra%d   "), delay, nVidRunahead, op_delay, op_runahead);
				else swprintf(buf_line3, 64, _T("P1: d%d-ra%d  |  P2: d%d-ra%d   "), op_delay, op_runahead, delay, nVidRunahead);
			}
			stats_line3.Set(buf_line3);
		}
	}

	// send warning if fps went down the thresold
	if (fps < ((nBurnFPS / 100) - 4)) {
		if (nFramesEmulated > 2000) {
			VidOverlaySetWarning(150, 1);
		}
	}

}

void VidOverlaySetWarning(int warning, int line)
{

	if (line == 1) {
		stats_line1_warning+= warning;
		if (stats_line1_warning > WARNING_MAX) {
			stats_line1_warning = WARNING_MAX;
		}
	} else if (line == 2) {
		stats_line2_warning+= warning;
		if (stats_line2_warning > WARNING_MAX) {
			stats_line2_warning = WARNING_MAX;
		}
	} else if (line == 3) {
		stats_line3_warning+= warning;
		if (stats_line3_warning > WARNING_MAX) {
			stats_line3_warning = WARNING_MAX;
		}
	}

}

void VidOverlayShowVolume(const wchar_t* text)
{
	volume.Set(text);
	volume_time = frame_time + INFO_FRAMES;
}

void VidOverlaySetChatInput(const wchar_t *text)
{
	chat_input.Set(text);
	chat_time = frame_time + CHAT_FRAMES;
	show_chat_input = true;
	show_chat = true;
	VidOverlaySetWarning(-10000, 1);
	VidOverlaySetWarning(-10000, 2);
	VidOverlaySetWarning(-10000, 3);
}

bool bMutedWarnSent = false;
void VidOverlayAddChatLine(const wchar_t *name, const wchar_t *text)
{
	if (!wcscmp(name, _T("Command"))) {
		int cmd;
		int idx;
		if (swscanf(text, _T("%d,%d"), &cmd, &idx) == 2) {
			switch (cmd)
			{
				// get delay & runahead from opponent
				case CMD_DELAYRA:
					if (idx != game_player) {
						swscanf(text, _T("%d,%d,%d,%d"), &cmd, &idx, &op_delay, &op_runahead);
						bOpInfoRcvd = true;
					}
					break;
				// opponent has ingame chat muted
				case CMD_CHATMUTED:
					if (idx != game_player) {
						VidOverlayAddChatLine(_T("System"), _T("Your opponent has opted to disable ingame chat. Your message was not sent."));
					}
					break;
			}
		}
		// commands are not chat!
		return;
	}

	if (bVidMuteChat) {
		if (!bMutedWarnSent) {
			char buffer[16];
			bMutedWarnSent = true;
			sprintf(buffer, "%d,%d", CMD_CHATMUTED, game_player);
			QuarkSendChatCmd(buffer, 'C');
		}
		return;
	}

	// regular chat
	bool save = false;
	for (int i = CHAT_LINES; i > 1; --i) {
		chat_names[i - 1].Copy(chat_names[i - 2]);
		chat_lines[i - 1].Copy(chat_lines[i - 2]);
	}
	wchar_t user[128];
	if (!wcscmp(name, player1.name.str)) {
		swprintf(user, 128, _T("%s"), name);
		chat_names[0].col = (game_player == 0) ? 0xFF00B2FF : 0xFFFFB200;
		save = true;
	}
	else if (!wcscmp(name, player2.name.str)) {
		swprintf(user, 128, _T("%s"), name);
		chat_names[0].col = (game_player == 1) ? 0xFF00B2FF : 0xFFFFB200;
		save = true;
	}
	else {
		swprintf(user, 128, _T("%s"), name);
		chat_names[0].col = 0xFFFFFFFF - 0x00B2FF;
	}
	chat_names[0].Set(user);
	chat_lines[0].Set(text);
	chat_time = frame_time + CHAT_FRAMES_EXT;

	// chat history
	if (save) {
		wchar_t buffer[MAX_PATH];
		swprintf(buffer, MAX_PATH, _T("%s: %s\n"), name, text);
		VidOverlaySaveChatHistory(buffer);
	}
}

void VidOverlaySaveFile(const char *file, const wchar_t *text)
{
	FILE *f = fopen(file, "wt");
	if (f) {
		fwprintf(f, text);
		fclose(f);
	}
}

void VidOverlaySaveFiles(bool save_info, bool save_scores, bool save_characters, int save_winner)
{
	struct SaveTxt {
		wchar_t text[128] = {};
		void Save(const char *file, wchar_t *str) {
			if (wcscmp(str, text)) {
				wcscpy(text, str);
				VidOverlaySaveFile(file, str);
			}
		}
	};

	static SaveTxt saveVs;
	static SaveTxt saveGame;
	static SaveTxt saveType;
	static SaveTxt saveQuark;
	static SaveTxt saveWinner;
	static SaveTxt saveP1name;
	static SaveTxt saveP2name;
	static SaveTxt saveP1rank;
	static SaveTxt saveP2rank;
	static SaveTxt saveP1country;
	static SaveTxt saveP2country;
	static SaveTxt saveP1score;
	static SaveTxt saveP2score;
	static SaveTxt saveP1character;
	static SaveTxt saveP2character;

	if (save_info) {
		wchar_t vs[16] = _T("VS");
		wchar_t *ranks[] = { _T("?"), _T("E"), _T("D"), _T("C"), _T("B"), _T("A"), _T("S") };
		if (game_ranked > 1) {
			swprintf(vs, 256, _T("FT%d"), game_ranked);
		}
		saveVs.Save("fightcade/vs.txt", vs);
		saveGame.Save("fightcade/game.txt", BurnDrvGetText(DRV_NAME));
		saveType.Save("fightcade/gametype.txt", kNetSpectator ? _T("Spectator") : _T("Game"));
		saveQuark.Save("fightcade/gamequark.txt", ANSIToTCHAR(kNetQuarkId, NULL, NULL));
		saveP1name.Save("fightcade/p1name.txt", player1.name.str);
		saveP2name.Save("fightcade/p2name.txt", player2.name.str);
		saveP1rank.Save("fightcade/p1rank.txt", ranks[player1.rank]);
		saveP2rank.Save("fightcade/p2rank.txt", ranks[player2.rank]);
		saveP1country.Save("fightcade/p1country.txt", player1.country.str);
		saveP2country.Save("fightcade/p2country.txt", player2.country.str);

		char buf[64];
		sprintf(buf, "ui/flags/%s.png", TCHARToANSI(player1.country.str, NULL, NULL));
		DeleteFile(_T("fightcade/p1country.png"));
		CopyFileContents(buf, "fightcade/p1country.png");
		sprintf(buf, "ui/flags/%s.png", TCHARToANSI(player2.country.str, NULL, NULL));
		DeleteFile(_T("fightcade/p2country.png"));
		CopyFileContents(buf, "fightcade/p2country.png");
	}
	if (save_winner > 0) {
		saveWinner.Save("fightcade/winner.txt", save_winner == 1 ? player1.name.str : player2.name.str);
	}
	if (save_scores) {
		saveP1score.Save("fightcade/p1score.txt", player1.score.str);
		saveP2score.Save("fightcade/p2score.txt", player2.score.str);
	}
	if (save_characters) {
		if (wcslen(gameDetector.char1) > 0) {
			saveP1character.Save("fightcade/p1character.txt", gameDetector.char1);
		}
		if (wcslen(gameDetector.char2) > 0) {
			saveP2character.Save("fightcade/p2character.txt", gameDetector.char2);
		}
	}
}

void VidOverlaySaveInfo()
{
	if (bVidSaveOverlayFiles) {
		static bool started = false;
		if (!started) {
			started = true;
			VidOverlaySaveFile("fightcade/started.inf", _T("1"));
		}
	}
}

void VidOverlaySaveChatHistory(const wchar_t *text)
{
	if (bVidSaveChatHistory && !kNetSpectator) {
		if (text) {
			FILE *f = fopen("fightcade/chat_history.txt", "at");
			if (f) {
				fwprintf(f, text);
				fclose(f);
			}
		}
	}
}

bool VidOverlayCanReset()
{
	return !game_ranked || gameDetector.state != GameDetector::ST_WAIT_WINNER || kNetVersion < NET_VERSION_RESET_INGAME;
}


void VidDebug(const wchar_t *text, float a, float b)
{
#ifdef TEST_VERSION
	wchar_t buf[128];
	swprintf(buf, 128, _T("%.2f / %.2f (frame %d)"), a, b, nFramesEmulated);
	VidOverlayAddChatLine(text, buf);
#endif
}

static int nFreezeCount = 0;
static int nFreezeFrames = 0;
static bool freeze_warn1_sent = false;
static bool freeze_warn2_sent = false;
void DetectFreeze()
{
	char temp[32];

	if (nFramesRendered < 3000) return;
	if (!debug_freeze && !game_ranked) return;
	if (game_spectator || gameDetector.frame_end) return;
	if (freeze_warn1_sent && freeze_warn2_sent) return;

	nFreezeFrames++;
	if (nFreezeFrames == 8) nFreezeCount++;

	if (debug_freeze) {
		TCHAR szTemp[128];
		_sntprintf(szTemp, 128, _T("nFreezeFrames: %d nFreezeCount: %d"), nFreezeFrames, nFreezeCount);
		VidOverlayAddChatLine(_T("System"), szTemp);
	}

	if (nFreezeFrames == 1800 && !freeze_warn1_sent) {
		if (debug_freeze) VidOverlayAddChatLine(_T("System"), _T("FREEZE WARNING SENT"));
		sprintf(temp, "%d,%d,%d", game_player,nFreezeFrames,nFreezeCount);
		QuarkSendChatCmd(temp, 'F');
		freeze_warn1_sent=true;
	}

	if (nFreezeCount == 10 && !freeze_warn2_sent) {
		if (debug_freeze) VidOverlayAddChatLine(_T("System"), _T("FREEZE WARNING SENT"));
		sprintf(temp, "%d,%d,%d", game_player,nFreezeFrames,nFreezeCount);
		QuarkSendChatCmd(temp, 'F');
		freeze_warn2_sent=true;
	}
}

#define TURBOARRAYSIZE 60
static int P1LP_Array[TURBOARRAYSIZE] = {};
static int P1MP_Array[TURBOARRAYSIZE] = {};
static int P1HP_Array[TURBOARRAYSIZE] = {};
static int P1LK_Array[TURBOARRAYSIZE] = {};
static int P1MK_Array[TURBOARRAYSIZE] = {};
static int P1HK_Array[TURBOARRAYSIZE] = {};
static int P2LP_Array[TURBOARRAYSIZE] = {};
static int P2MP_Array[TURBOARRAYSIZE] = {};
static int P2HP_Array[TURBOARRAYSIZE] = {};
static int P2LK_Array[TURBOARRAYSIZE] = {};
static int P2MK_Array[TURBOARRAYSIZE] = {};
static int P2HK_Array[TURBOARRAYSIZE] = {};
static int turboArrayPos = 0;
static int p1_turbo_warning = 0;
static int p2_turbo_warning = 0;
static int p1_turbo_certainty = 0;
static int p2_turbo_certainty = 0;
static int p1_max_tps[6]={0, 0, 0, 0, 0, 0};
static int p2_max_tps[6]={0, 0, 0, 0, 0, 0};
static int p1_smooth[6]={0, 0, 0, 0, 0, 0};
static int p2_smooth[6]={0, 0, 0, 0, 0, 0};

void DetectTurbo()
{

	nFreezeFrames = 0;

	UINT32 P1LP=0, P1MP=0, P1HP=0, P1LK=0, P1MK=0, P1HK=0;
	UINT32 P2LP=0, P2MP=0, P2HP=0, P2LK=0, P2MK=0, P2HK=0;

	enum {
		LP=0,
		MP=1,
		HP=2,
		LK=3,
		MK=4,
		HK=5,
	};

	// read all inputs
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));
	for (unsigned int i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		struct GameInp *pgi = &GameInp[i];
		if (pgi->nInput == GIT_SWITCH && pgi->Input.pVal) {
			int value = *pgi->Input.pVal;

			// P1 inputs
			if (!strcmp(bii.szInfo, "p1 fire 1")) P1LP = value << INPUT_LP;
			if (!strcmp(bii.szInfo, "p1 fire 2")) P1MP = value << INPUT_MP;
			if (!strcmp(bii.szInfo, "p1 fire 3")) P1HP = value << INPUT_HP;
			if (!strcmp(bii.szInfo, "p1 fire 4")) P1LK = value << INPUT_LK;
			if (!strcmp(bii.szInfo, "p1 fire 5")) P1MK = value << INPUT_MK;
			if (!strcmp(bii.szInfo, "p1 fire 6")) P1HK = value << INPUT_HK;

			// P2 inputs
			if (!strcmp(bii.szInfo, "p2 fire 1")) P2LP = value << INPUT_LP;
			if (!strcmp(bii.szInfo, "p2 fire 2")) P2MP = value << INPUT_MP;
			if (!strcmp(bii.szInfo, "p2 fire 3")) P2HP = value << INPUT_HP;
			if (!strcmp(bii.szInfo, "p2 fire 4")) P2LK = value << INPUT_LK;
			if (!strcmp(bii.szInfo, "p2 fire 5")) P2MK = value << INPUT_MK;
			if (!strcmp(bii.szInfo, "p2 fire 6")) P2HK = value << INPUT_HK;
		}
	}

	P1LP_Array[turboArrayPos]=P1LP;
	P1MP_Array[turboArrayPos]=P1MP;
	P1HP_Array[turboArrayPos]=P1HP;
	P1LK_Array[turboArrayPos]=P1LK;
	P1MK_Array[turboArrayPos]=P1MK;
	P1HK_Array[turboArrayPos]=P1HK;

	P2LP_Array[turboArrayPos]=P2LP;
	P2MP_Array[turboArrayPos]=P2MP;
	P2HP_Array[turboArrayPos]=P2HP;
	P2LK_Array[turboArrayPos]=P2LK;
	P2MK_Array[turboArrayPos]=P2MK;
	P2HK_Array[turboArrayPos]=P2HK;

	//TCHAR szTemp[128];
	//_sntprintf(szTemp, 128, _T("P1: %d %d %d, P2: %d %d %d"), P1LP, P1MP, P1HP, P2LP, P2MP, P2HP);
	//VidOverlayAddChatLine(_T("inputs:"), szTemp);

	turboArrayPos++;
	if (turboArrayPos >= TURBOARRAYSIZE) turboArrayPos=0;

	if (nFramesEmulated % 36000 == 0) {
		p1_turbo_warning=0;
		p2_turbo_warning=0;
		if (debug_turbo) VidOverlayAddChatLine(_T("System"), _T("RESET TURBO WARNING"));
	}

	int p1_tps[6]={0, 0, 0, 0, 0, 0};
	int p2_tps[6]={0, 0, 0, 0, 0, 0};
	int p1_streak[6]={0, 0, 0, 0, 0, 0};
	int p2_streak[6]={0, 0, 0, 0, 0, 0};
	for (int i = 1; i < TURBOARRAYSIZE; i++) {
		if (!P1LP_Array[i]) p1_streak[LP]=0; else p1_streak[LP]++;
		if (!P1MP_Array[i]) p1_streak[MP]=0; else p1_streak[MP]++;
		if (!P1HP_Array[i]) p1_streak[HP]=0; else p1_streak[HP]++;
		if (!P1LK_Array[i]) p1_streak[LK]=0; else p1_streak[LK]++;
		if (!P1MK_Array[i]) p1_streak[MK]=0; else p1_streak[MK]++;
		if (!P1HK_Array[i]) p1_streak[HK]=0; else p1_streak[HK]++;

		if (!P2LP_Array[i]) p2_streak[LP]=0; else p2_streak[LP]++;
		if (!P2MP_Array[i]) p2_streak[MP]=0; else p2_streak[MP]++;
		if (!P2HP_Array[i]) p2_streak[HP]=0; else p2_streak[HP]++;
		if (!P2LK_Array[i]) p2_streak[LK]=0; else p2_streak[LK]++;
		if (!P2MK_Array[i]) p2_streak[MK]=0; else p2_streak[MK]++;
		if (!P2HK_Array[i]) p2_streak[HK]=0; else p2_streak[HK]++;

		if (P1LP_Array[i] !=0 && P1LP_Array[i-1] == 0) p1_tps[LP]++;
		if (P1MP_Array[i] !=0 && P1MP_Array[i-1] == 0) p1_tps[MP]++;
		if (P1HP_Array[i] !=0 && P1HP_Array[i-1] == 0) p1_tps[HP]++;
		if (P1LK_Array[i] !=0 && P1LK_Array[i-1] == 0) p1_tps[LK]++;
		if (P1MK_Array[i] !=0 && P1MK_Array[i-1] == 0) p1_tps[MK]++;
		if (P1HK_Array[i] !=0 && P1HK_Array[i-1] == 0) p1_tps[HK]++;

		if (P2LP_Array[i] !=0 && P2LP_Array[i-1] == 0) p2_tps[LP]++;
		if (P2MP_Array[i] !=0 && P2MP_Array[i-1] == 0) p2_tps[MP]++;
		if (P2HP_Array[i] !=0 && P2HP_Array[i-1] == 0) p2_tps[HP]++;
		if (P2LK_Array[i] !=0 && P2LK_Array[i-1] == 0) p2_tps[LK]++;
		if (P2MK_Array[i] !=0 && P2MK_Array[i-1] == 0) p2_tps[MK]++;
		if (P2HK_Array[i] !=0 && P2HK_Array[i-1] == 0) p2_tps[HK]++;
	}

	TCHAR szWarn1[128];
	TCHAR szWarn2[128];
	int p1_buttons_with_turbo = 0;
	int p2_buttons_with_turbo = 0;
	int MIN_TURBO_CERTAINTY = 48;
	if (game_ranked) MIN_TURBO_CERTAINTY=32;

	for (int i=0; i < 6; i++) {
		if (p1_tps[i] > 14) {
			p1_buttons_with_turbo++;
			if (p1_tps[i] > p1_max_tps[i]) p1_max_tps[i]=p1_tps[i];
			if (p1_tps[i] > 15) {
				if (p1_tps[i] > p1_turbo_warning) {
					p1_turbo_warning=p1_tps[i] - p1_smooth[i];
					//if (debug_turbo) {
					//	_sntprintf(szWarn1, 128, _T("Player1 button%d: %dtps (%d)"), i+1, p1_tps[i], p1_smooth[i]);
					//	VidOverlayAddChatLine(_T("System"), szWarn1);
					//}
					if (p1_tps[i] >= 20) {
						if (p1_turbo_certainty >= MIN_TURBO_CERTAINTY && p1_tps[i] == p1_max_tps[i]) {
							_sntprintf(szWarn1, 128, _T("Turbo/Autofire detected on Player1 button%d: %dtps"), i+1, p1_tps[i]);
							VidOverlayAddChatLine(_T("System"), szWarn1);
							p1_max_tps[i] += 3; // prevent the message from showing again if the autofire is at a fixed rate
						}
					} else {
						if (p1_turbo_certainty >= MIN_TURBO_CERTAINTY && p1_tps[i] == p1_max_tps[i]) {
							if (debug_turbo) {
								_sntprintf(szWarn1, 128, _T("Possible Turbo/Autofire detected on Player1 button%d: %dtps"), i+1, p1_tps[i]);
								VidOverlayAddChatLine(_T("System"), szWarn1);
							}
							p1_max_tps[i]++;
						}
					}
				}
			}
			else if (p1_buttons_with_turbo >= 2) {
				p1_turbo_warning=15 - p1_smooth[i];
			}
		}
		if (p2_tps[i] > 14) {
			p2_buttons_with_turbo++;
			if (p2_tps[i] > p2_max_tps[i]) p2_max_tps[i]=p2_tps[i];
			if (p2_tps[i] > 15) {
				if (p2_tps[i] > p2_turbo_warning) {
					p2_turbo_warning=p2_tps[i] - p2_smooth[i];
					if (p2_tps[i] >= 20) {
						if (p2_turbo_certainty >= MIN_TURBO_CERTAINTY && p2_tps[i] == p2_max_tps[i]) {
							_sntprintf(szWarn2, 128, _T("Turbo/Autofire detected on Player2 button%d: %dtps"), i+1, p2_tps[i]);
							VidOverlayAddChatLine(_T("System"), szWarn2);
							p2_max_tps[i] += 3;
						}
					} else {
						if (p2_turbo_certainty >= MIN_TURBO_CERTAINTY && p2_tps[i] == p2_max_tps[i]) {
							if (debug_turbo) {
								_sntprintf(szWarn2, 128, _T("Possible Turbo/Autofire detected on Player2 button%d: %dtps"), i+1, p2_tps[i]);
								VidOverlayAddChatLine(_T("System"), szWarn2);
							}
							p2_max_tps[i]++;
						}
					}
				}
			}
			else if (p2_buttons_with_turbo >= 2) {
				p2_turbo_warning=15 - p2_smooth[i];
			}
		}
	}

	if (turboArrayPos == 0) {

		if (p1_streak[LP] > 8 && p1_streak[MP]==0 && p1_streak[HP]==0 && p1_smooth[LP] < 2) p1_smooth[LP]++;
		if (p1_streak[LP]==0 && p1_streak[MP] > 8 && p1_streak[HP]==0 && p1_smooth[MP] < 2) p1_smooth[MP]++;
		if (p1_streak[LP]==0 && p1_streak[MP]==0 && p1_streak[HP] > 8 && p1_smooth[HP] < 2) p1_smooth[HP]++;
		if (p1_streak[LK] > 8 && p1_streak[MK]==0 && p1_streak[HK]==0 && p1_smooth[LK] < 2) p1_smooth[LK]++;
		if (p1_streak[LK]==0 && p1_streak[MK] > 8 && p1_streak[HK]==0 && p1_smooth[MK] < 2) p1_smooth[MK]++;
		if (p1_streak[LK]==0 && p1_streak[MK]==0 && p1_streak[HK] > 8 && p1_smooth[HK] < 2) p1_smooth[HK]++;

		if (p2_streak[LP] > 8 && p2_streak[MP]==0 && p2_streak[HP]==0 && p2_smooth[LP] < 2) p2_smooth[LP]++;
		if (p2_streak[LP]==0 && p2_streak[MP] > 8 && p2_streak[HP]==0 && p2_smooth[MP] < 2) p2_smooth[MP]++;
		if (p2_streak[LP]==0 && p2_streak[MP]==0 && p2_streak[HP] > 8 && p2_smooth[HP] < 2) p2_smooth[HP]++;
		if (p2_streak[LK] > 8 && p2_streak[MK]==0 && p2_streak[HK]==0 && p2_smooth[LK] < 2) p2_smooth[LK]++;
		if (p2_streak[LK]==0 && p2_streak[MK] > 8 && p2_streak[HK]==0 && p2_smooth[MK] < 2) p2_smooth[MK]++;
		if (p2_streak[LK]==0 && p2_streak[MK]==0 && p2_streak[HK] > 8 && p2_smooth[HK] < 2) p2_smooth[HK]++;

		if (p1_turbo_certainty < MIN_TURBO_CERTAINTY && p1_turbo_warning > 14) {
			if (p1_turbo_warning < 16) p1_turbo_certainty += 1;
			else if (p1_turbo_warning < 20) p1_turbo_certainty += p1_turbo_warning - 14;
			else if (p1_turbo_warning < 22) p1_turbo_certainty += p1_turbo_warning;
			else p1_turbo_certainty += p1_turbo_warning * 2;
		}
		if (p2_turbo_certainty < MIN_TURBO_CERTAINTY && p2_turbo_warning > 14) {
			if (p2_turbo_warning < 16) p2_turbo_certainty += 1;
			else if (p2_turbo_warning < 20) p2_turbo_certainty += p2_turbo_warning - 14;
			else if (p2_turbo_warning < 22) p2_turbo_certainty += p2_turbo_warning;
			else p2_turbo_certainty += p2_turbo_warning * 2;
		}
		if (debug_turbo) {
			if (p1_turbo_warning > 14 && p1_turbo_certainty < MIN_TURBO_CERTAINTY) {
				_sntprintf(szWarn1, 128, _T("p1_turbo_certainty=%d"), p1_turbo_certainty);
				VidOverlayAddChatLine(_T("System"), szWarn1);
			}
			if (p2_turbo_warning > 14 && p2_turbo_certainty < MIN_TURBO_CERTAINTY) {
				_sntprintf(szWarn2, 128, _T("p2_turbo_certainty=%d"), p2_turbo_certainty);
				VidOverlayAddChatLine(_T("System"), szWarn2);
			}
		}
		if (p1_turbo_certainty >= MIN_TURBO_CERTAINTY && p1_turbo_certainty < 1000) {
			p1_turbo_certainty = 1000;
			VidOverlayAddChatLine(_T("System"), _T("Possible Turbo/Autofire detected on Player1 (or heavy button mashing)"));
			if (game_ranked && !game_spectator) QuarkSendChatCmd("0", 'X');
		}
		if (p2_turbo_certainty >= MIN_TURBO_CERTAINTY && p2_turbo_certainty < 1000) {
			p2_turbo_certainty = 1000;
			VidOverlayAddChatLine(_T("System"), _T("Possible Turbo/Autofire detected on Player2 (or heavy button mashing)"));
			if (game_ranked && !game_spectator) QuarkSendChatCmd("1", 'X');
		}
		p1_turbo_warning=0;
		p2_turbo_warning=0;
	}
}

void VidDisplayInputs(int slot, int state)
{
	if (bVidShowInputs)
	{
		INT32 inputs[2] = {};

		// read all inputs
		struct BurnInputInfo bii;
		memset(&bii, 0, sizeof(bii));
		for (unsigned int i = 0; i < nGameInpCount; i++) {
			BurnDrvGetInputInfo(&bii, i);
			struct GameInp *pgi = &GameInp[i];
			if (pgi->nInput == GIT_SWITCH && pgi->Input.pVal) {
				int value = *pgi->Input.pVal;

				// P1 inputs
				if (!strcmp(bii.szInfo, "p1 up")) inputs[0] |= value << INPUT_UP;
				if (!strcmp(bii.szInfo, "p1 down")) inputs[0] |= value << INPUT_DOWN;
				if (!strcmp(bii.szInfo, "p1 left")) inputs[0] |= value << INPUT_LEFT;
				if (!strcmp(bii.szInfo, "p1 right")) inputs[0] |= value << INPUT_RIGHT;
				if (!strcmp(bii.szInfo, "p1 fire 1")) inputs[0] |= value << INPUT_LP;
				if (!strcmp(bii.szInfo, "p1 fire 2")) inputs[0] |= value << INPUT_MP;
				if (!strcmp(bii.szInfo, "p1 fire 3")) inputs[0] |= value << INPUT_HP;
				if (!strcmp(bii.szInfo, "p1 fire 4")) inputs[0] |= value << INPUT_LK;
				if (!strcmp(bii.szInfo, "p1 fire 5")) inputs[0] |= value << INPUT_MK;
				if (!strcmp(bii.szInfo, "p1 fire 6")) inputs[0] |= value << INPUT_HK;

				// P2 inputs
				if (!strcmp(bii.szInfo, "p2 up")) inputs[1] |= value << INPUT_UP;
				if (!strcmp(bii.szInfo, "p2 down")) inputs[1] |= value << INPUT_DOWN;
				if (!strcmp(bii.szInfo, "p2 left")) inputs[1] |= value << INPUT_LEFT;
				if (!strcmp(bii.szInfo, "p2 right")) inputs[1] |= value << INPUT_RIGHT;
				if (!strcmp(bii.szInfo, "p2 fire 1")) inputs[1] |= value << INPUT_LP;
				if (!strcmp(bii.szInfo, "p2 fire 2")) inputs[1] |= value << INPUT_MP;
				if (!strcmp(bii.szInfo, "p2 fire 3")) inputs[1] |= value << INPUT_HP;
				if (!strcmp(bii.szInfo, "p2 fire 4")) inputs[1] |= value << INPUT_LK;
				if (!strcmp(bii.szInfo, "p2 fire 5")) inputs[1] |= value << INPUT_MK;
				if (!strcmp(bii.szInfo, "p2 fire 6")) inputs[1] |= value << INPUT_HK;
			}
		}

		// save
		for (int k = 0; k < 2; k++) {
			for (int i = 1; i < INPUT_DISPLAY; i++) {
				display_inputs[k][slot][INPUT_DISPLAY-i] = display_inputs[k][slot][INPUT_DISPLAY-i-1];
			}
			display_inputs[k][slot][0].values = inputs[k];
			display_inputs[k][slot][0].state = state;
			display_inputs[k][slot][0].frame = nFramesEmulated;
		}
	}
}
