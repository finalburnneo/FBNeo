#ifndef _LUASAV_H_
#define _LUASAV_H_

struct LuaSaveData
{
	LuaSaveData() { recordList = 0; }
	~LuaSaveData() { ClearRecords(); }

	struct Record
	{
		unsigned int key; // crc32
		unsigned int size; // size of data
		unsigned char* data;
		Record* next;
	};

	Record* recordList;

	void SaveRecord(struct lua_State* L, unsigned int key); // saves Lua stack into a record and pops it
	void LoadRecord(struct lua_State* L, unsigned int key, unsigned int itemsToLoad) const; // pushes a record's data onto the Lua stack
	void SaveRecordPartial(struct lua_State* L, unsigned int key, int idx); // saves part of the Lua stack (at the given index) into a record and does NOT pop anything

	void ExportRecords(void* file) const; // writes all records to an already-open file
	void ImportRecords(void* file); // reads records from an already-open file
	void ClearRecords(); // deletes all record data

private:
	// disallowed, it's dangerous to call this
	// (because the memory the destructor deletes isn't refcounted and shouldn't need to be copied)
	// so pass LuaSaveDatas by reference and this should never get called
	LuaSaveData(const LuaSaveData& copy) {}
};

#define LUA_SAVE_CALLBACK_STRING "FBA.Save"
#define LUA_LOAD_CALLBACK_STRING "FBA.Load"
#define LUA_DATARECORDKEY 42

void CallRegisteredLuaSaveFunctions(const char *savestateNumber, LuaSaveData& saveData);
void CallRegisteredLuaLoadFunctions(const char *savestateNumber, const LuaSaveData& saveData);

#endif
