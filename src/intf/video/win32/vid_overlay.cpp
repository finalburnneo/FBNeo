#include "burner.h"
#include "vid_overlay.h"
#include "vid_font.h"
#include "vid_dynrender.h"
#include "vid_memorybuffer.h"
#include "vid_detector.h"
#include <d3dx9.h>

//#define PRINT_DEBUG_INFO
//#define TEST_OVERLAY
//#define TEST_INPUTS
//#define TEST_VERSION					L"RC7 v3"

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define TEXT_ANIM_FRAMES    	8
#define NUM_RANKS           	7
#define INFO_FRAMES			      150
#define WARNING_FRAMES				180
#define CHAT_LINES			      7
#define CHAT_FRAMES			      200
#define CHAT_FRAMES_EXT 	    350
#define START_FRAMES          300
#define END_FRAMES            500
#define DETECTOR_FRAMES				30
#define WARNING_THRESHOLD			260
#define WARNING_MAX						510
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
static float frame_scale;
static float frame_adjust;
static float frame_width;
static float frame_height;
static float frame_ratio = 1;
static int frame_time = 0;
static int frame_warning = 0;
static int frame_warning_sent = 0;
static int frame_warning_count = 0;

//------------------------------------------------------------------------------------------------------------------------------
// test inputs
//------------------------------------------------------------------------------------------------------------------------------
#define INPUT_DISPLAY 60
#define INPUT_BUTTONS 10
struct TInput {
	INT32 frame;
	INT32 values;
	int stage;
};

static TInput inputs_p1[2][INPUT_DISPLAY+1] = {};
static TInput inputs_p2[2][INPUT_DISPLAY+1] = {};

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
#define FS(x)		((float)(x) * (float)frame_scale)
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
static bool show_stats = false;
static int info_time = 0;
static int chat_time = 0;
static int volume_time = 0;
static int warning_time = 0;

enum
{
	CMD_FPSWARNING = 1,
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
	memory_bits = (1 << _bits) - 1;
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
				case 0xffffffff:
					memory_current = ((unsigned int *)ba->Data)[memory_ptr>>2];
					break;
				// 16 bits
				case 0xffff:
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
			case 0xffffffff:
				memory_current = ReadValueAtHardwareAddress(memory_ptr, 4, 0);
				break;
			// 16 bits
			case 0xffff:
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
					VidOverlaySaveFiles(false, false, true); // to save characters (if there's any detected)
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
					VidOverlaySetGameScores(score1, score2);
					VidSSetGameScores(score1, score2);
					winner = 1;
				}
				// player2
				if (player2_detected) {
					score2++;
					DetectorSetState(ST_WAIT_START, score1, score2);
					VidOverlaySetGameScores(score1, score2);
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
	if (kNetVersion >= 3) {
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

	// Stage selector for SF2HF, introduced in version 5
	if (kNetVersion >= 5) {
		/*
		0 "Disabled" 1 "Balrog",
		0, 0xFFDD5F, 0x0A 2 "Blanka", 0, 0xFFDD5F, 0x02 3 "Chun-Li", 0, 0xFFDD5F, 0x05 4 "Dhalsim", 0, 0xFFDD5F, 0x07 5 "E.Honda", 0, 0xFFDD5F, 0x01 6 "Guile", 0, 0xFFDD5F, 0x03 7 "Ken", 0, 0xFFDD5F, 0x04 8 "M.Bison", 0, 0xFFDD5F, 0x08 9 "Ryu", 0, 0xFFDD5F, 0x00 10 "Sagat", 0, 0xFFDD5F, 0x09 11 "Vega", 0, 0xFFDD5F, 0x0B 12 "Zangief", 0, 0xFFDD5F, 0x06
		*/
	}

	gameDetector.UpdateDetectors(pba, gameDetector2p);

	return 0;
}

void DetectorLoad(const char *game, bool debug, int seed)
{
	debug_mode = debug;
 	gameDetector.Load(game);

	// initial state
	if (kNetVersion >= 3 && seed) {
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
	frame_warning--;
	if (frame_warning < 0) {
		frame_warning = 0;
	}
	frame_warning_sent--;
	if (frame_warning_sent < 0) {
		frame_warning_sent = 0;
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
static Text stats;
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
	VidOverlaySetGameInfo(_T("Kinikkuman#2,10"), _T("shine#3,8"), false, 10, 0);
	VidOverlaySetGameSpectators(13);
	VidOverlayAddChatLine(_T("Kinikkuman"), _T("test line 1"));
	VidOverlayAddChatLine(_T("shine"), _T("good game!"));
	VidOverlayAddChatLine(_T("System"), _T("You guys chat too much"));
	VidOverlayAddChatLine(_T("Kinikkuman"), _T("omg"));
	VidOverlayAddChatLine(_T("System"), _T("Too much buttons pressed, autofire detected?"));
	VidOverlayAddChatLine(_T("Kinikkuman"), _T("lorem ipsum chat text"));
	VidOverlayAddChatLine(_T("Kinikkuman"), _T("this is another lorem ipsum chat text, something longer and longer and longer"));
	VidOverlayAddChatLine(_T("shine"), _T("Jackie Chan will be best new addition to Fightcade - also Daraku Tenshi"));
	VidOverlayAddChatLine(_T("shine"), _T("Día Completón ííííí"));
	VidOverlayAddChatLine(_T("shine"), _T("test line 11, last line"));
	VidOverlaySetSystemMessage(_T("System message"));
#endif
}

void VidOverlayEnd()
{
	if (gameDetector.state == GameDetector::ST_WAIT_WINNER) {
		QuarkSendChatCmd("quit", 'S');
	}
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
			VidOverlaySaveChatHistory(NULL);
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
	else if (show_stats) {
		// stats (fps & ping)
		stats.col = (frame_warning >= 100) ? 0xffff0000 : 0xffffffff;
		stats.Render(frame_width - 0.0035f, 0.003f, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
	}
	
	if (frame_time < warning_time) {
		// warning
		warning.Render(frame_width - 0.0035f, 0.003f + FNT_MED * FNT_SEP, 1.f, FNT_MED, FONT_ALIGN_RIGHT);
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

#ifdef TEST_INPUTS
	// inputs P1
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < INPUT_DISPLAY; i++) {
			static float sep = 0.01f;
			static float size = 1.f;
			static float fy = 0.f;
			static float fx = 0.15f;
			float x = 0.05f + j * fx;
			float y = 0.05f + i * sep;
			INT32 inputs = inputs_p1[j][i].values;
			wchar_t buf[128];
			wsprintf(buf, _T("%d"), inputs_p1[j][i].frame);
			INT32 color;
			switch (inputs_p1[j][i].stage) {
				case 0: color = 0xFFFFFFFF; break;
				case 1: color = 0xFFFF9090; break;
				case 2: color = 0xFF9090FF; break;
				case 3: color = 0xFF90FF90; inputs = 0; break;
				case 4: color = 0xFFFF90FF; break;
			}
			fontWrite(buf, x, y + fy, color, 1.f, FNT_SMA * size, FONT_ALIGN_CENTER);
			x+= sep*2.5f;
			if (inputs & (1 << INPUT_LEFT)) { InputRender(x, y, 0); x+= sep; }
			if (inputs & (1 << INPUT_RIGHT)) { InputRender(x, y, 1); x+= sep; }
			if (inputs & (1 << INPUT_UP)) { InputRender(x, y, 2); x+= sep; }
			if (inputs & (1 << INPUT_DOWN)) { InputRender(x, y, 3); x+= sep; }
			if (inputs & (1 << INPUT_LP)) { InputRender(x, y, 8); x+= sep; }
			if (inputs & (1 << INPUT_MP)) { InputRender(x, y, 9); x+= sep; }
			if (inputs & (1 << INPUT_HP)) { InputRender(x, y, 10); x+= sep; }
			if (inputs & (1 << INPUT_LK)) { InputRender(x, y, 11); x+= sep; }
			if (inputs & (1 << INPUT_MK)) { InputRender(x, y, 12); x+= sep; }
			if (inputs & (1 << INPUT_HK)) { InputRender(x, y, 13); x+= sep; }
		}
	}
	// inputs P2
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < INPUT_DISPLAY; i++) {
			static float sep = 0.01f;
			static float size = 1.f;
			static float fy = 0.f;
			static float fx = 0.15f;
			float x = 0.70f + j * fx;
			float y = 0.05f + i * sep;
			INT32 inputs = inputs_p2[j][i].values;
			wchar_t buf[128];
			wsprintf(buf, _T("%d"), inputs_p2[j][i].frame);
			INT32 color;
			switch (inputs_p2[j][i].stage) {
			case 0: color = 0xFFFFFFFF; break;
			case 1: color = 0xFFFF9090; break;
			case 2: color = 0xFF9090FF; break;
			case 3: color = 0xFF90FF90; inputs = 0; break;
			case 4: color = 0xFFFF90FF; break;
			}
			fontWrite(buf, x, y + fy, color, 1.f, FNT_SMA * size, FONT_ALIGN_CENTER);
			x+= sep*2.5f;
			if (inputs & (1 << INPUT_LEFT)) { InputRender(x, y, 0); x+= sep; }
			if (inputs & (1 << INPUT_RIGHT)) { InputRender(x, y, 1); x+= sep; }
			if (inputs & (1 << INPUT_UP)) { InputRender(x, y, 2); x+= sep; }
			if (inputs & (1 << INPUT_DOWN)) { InputRender(x, y, 3); x+= sep; }
			if (inputs & (1 << INPUT_LP)) { InputRender(x, y, 8); x+= sep; }
			if (inputs & (1 << INPUT_MP)) { InputRender(x, y, 9); x+= sep; }
			if (inputs & (1 << INPUT_HP)) { InputRender(x, y, 10); x+= sep; }
			if (inputs & (1 << INPUT_LK)) { InputRender(x, y, 11); x+= sep; }
			if (inputs & (1 << INPUT_MK)) { InputRender(x, y, 12); x+= sep; }
			if (inputs & (1 << INPUT_HK)) { InputRender(x, y, 13); x+= sep; }
		}
	}
#endif

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
		VidOverlaySaveFiles(true, false, false); // save only info
	}
}

void VidOverlaySetGameScores(int score1, int score2)
{
	wchar_t buf[32];
	player1.score.Set(_itow(score1, buf, 10));
	player2.score.Set(_itow(score2, buf, 10));

	// save overlay to data files
	if (bVidSaveOverlayFiles) {
		VidOverlaySaveFiles(false, true, false); // save only scores
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

void VidOverlaySetStats(double fps, int ping, int delay)
{
	wchar_t buf[64];
	if (game_spectator || ping == 0) {
		swprintf(buf, 64, _T("%2.2ffps"), fps);
	}
	else {
		swprintf(buf, 64, _T("%2.2ffps | %dms (%d-%d)"), fps, ping, delay, nVidRunahead);
	}

	stats.Set(buf);
}

void VidOverlaySetWarning(int warning)
{
	frame_warning+= warning;
	if (frame_warning > WARNING_MAX) {
		frame_warning = WARNING_MAX;
	}

	if (frame_warning > WARNING_THRESHOLD) {
		if (!frame_warning_sent) {
			// only 3 warnings
			if (frame_warning_count < WARNING_MSGCOUNT) {
				char buffer[32];
				sprintf(buffer, "%d,%d", CMD_FPSWARNING, game_player);
				QuarkSendChatCmd(buffer, 'C');
				// send next warning in 15 seconds
				frame_warning_sent = 60 * 15;
				frame_warning_count++;
			}
			// force show stats
			show_stats = true;
		}
	}
}

void VidOverlayShowStats(bool show)
{
	show_stats = show;
}

void VidOverlayShowVolume(int vol)
{
	wchar_t buffer[32];
	swprintf(buffer, 32, _T("VOL %d%%"), vol / 100);
	volume.Set(buffer);
	volume_time = frame_time + INFO_FRAMES;
}

void VidOverlaySetChatInput(const wchar_t *text)
{
	chat_input.Set(text);
	chat_time = frame_time + CHAT_FRAMES;
	show_chat_input = true;
	show_chat = true;
	VidOverlaySetWarning(-10000);
}

void VidOverlayAddChatLine(const wchar_t *name, const wchar_t *text)
{
	if (bVidMuteChat) {
		return;
	}

	if (!wcscmp(name, _T("Command"))) {
		int cmd;
		int idx;
		if (swscanf(text, _T("%d"), &cmd) == 1) {
			switch (cmd)
			{
				// warning command
				case CMD_FPSWARNING:
					if (swscanf(text, _T("%d,%d"), &cmd, &idx) == 2)
					{
						if (idx == game_player) {
							warning.Set(_T("You are having framerate issues and netplay is being affected, try removing Runahead"));
						}
						else {
							warning.Set(_T("Opponent is having framerate issues and netplay is being affected"));
						}
						warning_time = frame_time + WARNING_FRAMES;
					}
					break;
			}
		}
		// commands are not chat!
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

void VidOverlaySaveFiles(bool save_info, bool save_scores, bool save_characters)
{
	if (save_info) {
		wchar_t vs[16] = _T("VS");
		wchar_t *ranks[] = { _T("?"), _T("E"), _T("D"), _T("C"), _T("B"), _T("A"), _T("S") };
		if (game_ranked > 1) {
			swprintf(vs, 256, _T("FT%d"), game_ranked);
		}
		VidOverlaySaveFile("fightcade/vs.txt", vs);
		VidOverlaySaveFile("fightcade/p1name.txt", player1.name.str);
		VidOverlaySaveFile("fightcade/p2name.txt", player2.name.str);
		VidOverlaySaveFile("fightcade/p1rank.txt", ranks[player1.rank]);
		VidOverlaySaveFile("fightcade/p2rank.txt", ranks[player2.rank]);
		VidOverlaySaveFile("fightcade/p1country.txt", player1.country.str);
		VidOverlaySaveFile("fightcade/p2country.txt", player2.country.str);

		char buf[64];
		sprintf(buf, "ui/flags/%s.png", TCHARToANSI(player1.country.str, NULL, NULL));
		DeleteFile(_T("fightcade/p1country.png"));
		CopyFileContents(buf, "fightcade/p1country.png");
		sprintf(buf, "ui/flags/%s.png", TCHARToANSI(player2.country.str, NULL, NULL));
		DeleteFile(_T("fightcade/p2country.png"));
		CopyFileContents(buf, "fightcade/p2country.png");
	}
	if (save_scores) {
		VidOverlaySaveFile("fightcade/p1score.txt", player1.score.str);
		VidOverlaySaveFile("fightcade/p2score.txt", player2.score.str);
	}
	if (save_characters) {
		if (wcslen(gameDetector.char1) > 0) {
			VidOverlaySaveFile("fightcade/p1character.txt", gameDetector.char1);
		}
		if (wcslen(gameDetector.char2) > 0) {
			VidOverlaySaveFile("fightcade/p2character.txt", gameDetector.char2);
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
		else {
			DeleteFile(_T("fightcade/chat_history.txt"));
		}
	}
}

void VidDebug(const wchar_t *text, float a, float b)
{
#ifdef TEST_VERSION
	wchar_t buf[128];
	swprintf(buf, 128, _T("%.2f / %.2f (frame %d)"), a, b, nFramesEmulated);
	VidOverlayAddChatLine(text, buf);
#endif
}

void VidDisplayInputs(int slot, int stage)
{
#ifdef TEST_INPUTS
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

	// p1
	for (int i = 1; i < INPUT_DISPLAY; i++) {
		inputs_p1[slot][INPUT_DISPLAY-i] = inputs_p1[slot][INPUT_DISPLAY-i-1];
	}
	inputs_p1[slot][0].values = inputs[0];
	inputs_p1[slot][0].stage = stage;
	inputs_p1[slot][0].frame = nFramesEmulated;

	// p2
	for (int i = 1; i < INPUT_DISPLAY; i++) {
		inputs_p2[slot][INPUT_DISPLAY-i] = inputs_p2[slot][INPUT_DISPLAY-i-1];
	}
	inputs_p2[slot][0].values = inputs[1];
	inputs_p2[slot][0].stage = stage;
	inputs_p2[slot][0].frame = nFramesEmulated;
#endif
}
