#define _RETAIL
#include "GMLuaModule.h"
#include "gm_bass.h"

#define META_CHANNEL "BASSChannel"
#define TYPE_CHANNEL 6667

#define CHANNELFROMLUA() ILuaInterface *gLua = Lua(); \
							if (gLua->GetType(1) != TYPE_CHANNEL) gLua->TypeError(META_CHANNEL, 1); \
							DWORD handle = (DWORD)gLua->GetUserData(1);

#define TIME2LUA(X) 	QWORD len = X(handle, BASS_POS_BYTE); \
					float time = (float)BASS_ChannelBytes2Seconds(handle, len); \
					gLua->Push(time);

struct streamProcData
{
	char *file;
	int callback;
	DWORD handle;
	int error;
	SyncList<streamProcData *>* pending;
};

SyncList<streamProcData *>* GetPendingChannels( ILuaInterface *gLua )
{
	ILuaObject* bass = gLua->GetGlobal( "BASS" );
	if (!bass) return NULL;

	SyncList<streamProcData *>* pData = (SyncList<streamProcData *>*)bass->GetMemberUserDataLite( "p_PendingChannels" );
	return pData;
}

LUA_FUNCTION(channel_gc)
{
	CHANNELFROMLUA()

	BASS_StreamFree(handle);
	return 0;
}

LUA_FUNCTION(channel_play)
{
	CHANNELFROMLUA()
	BASS_ChannelPlay(handle, true);
	return 0;
}

LUA_FUNCTION(channel_pause)
{
	CHANNELFROMLUA()

	BASS_ChannelPause(handle);
	return 0;
}

LUA_FUNCTION(channel_stop)
{
	CHANNELFROMLUA()

	BASS_ChannelStop(handle);
	return 0;
}

LUA_FUNCTION(channel_getlength)
{
	CHANNELFROMLUA()

	TIME2LUA(BASS_ChannelGetLength)
	return 1;
}

LUA_FUNCTION(channel_getposition)
{
	CHANNELFROMLUA()

	TIME2LUA(BASS_ChannelGetPosition)
	return 1;
}

LUA_FUNCTION(channel_getplaying)
{
	CHANNELFROMLUA()

	gLua->Push((bool)(BASS_ChannelIsActive(handle) == BASS_ACTIVE_PLAYING));
	return 1;
}

LUA_FUNCTION(channel_getlevel)
{
	CHANNELFROMLUA()

	DWORD level = BASS_ChannelGetLevel(handle);
	gLua->Push((float)HIWORD(level));
	gLua->Push((float)LOWORD(level));
	return 2;
}

LUA_FUNCTION(channel_fft2048)
{
	CHANNELFROMLUA()

	float fft[1024];
	BASS_ChannelGetData(handle,fft,BASS_DATA_FFT2048);
	ILuaObject *tbl = gLua->GetNewTable();
	for(int x = 0; x < 1024; x++)
	{
		tbl->SetMember((float)x+1,fft[x]);
	}
	gLua->Push(tbl);
	tbl->UnReference();
	return 1;
}

LUA_FUNCTION(channel_getrawtag)
{
	CHANNELFROMLUA()

	gLua->CheckType(2, GLua::TYPE_NUMBER);
	int tag = gLua->GetInteger(2);

	const char *buff = BASS_ChannelGetTags(handle,tag);
	gLua->Push(buff);
	return 1;
}

LUA_FUNCTION(channel_gettag)
{
	CHANNELFROMLUA()

	gLua->CheckType(2, GLua::TYPE_STRING);
	const char *format = gLua->GetString(2);

	const char *buff = TAGS_Read(handle, format);
	gLua->Push(buff);
	return 1;
}

BASS_3DVECTOR *Get3DVect(Vector *vect)
{
	BASS_3DVECTOR *vec = new BASS_3DVECTOR;
	vec->x = vect->x;
	vec->y = vect->y;
	vec->z = vect->z;
	return vec;
}

LUA_FUNCTION(channel_set3dposition)
{
	CHANNELFROMLUA()

	gLua->CheckType(2, GLua::TYPE_VECTOR);
	gLua->CheckType(3, GLua::TYPE_VECTOR);
	gLua->CheckType(4, GLua::TYPE_VECTOR);
	BASS_3DVECTOR *pos = Get3DVect((Vector *)gLua->GetUserData(2));
	BASS_3DVECTOR *orient = Get3DVect((Vector *)gLua->GetUserData(3));
	BASS_3DVECTOR *vel = Get3DVect((Vector *)gLua->GetUserData(4));

	BASS_ChannelSet3DAttributes(handle, BASS_3DMODE_NORMAL, 0, 0, 360, 360, 0);
	BASS_ChannelSet3DPosition(handle, pos, orient, vel);

	BASS_Apply3D();

	delete pos;
	delete orient;
	delete vel;

	return 0;
}

LUA_FUNCTION(bass_setposition)
{
	ILuaInterface *gLua = Lua();

	gLua->CheckType(1, GLua::TYPE_VECTOR);
	gLua->CheckType(2, GLua::TYPE_VECTOR);
	gLua->CheckType(3, GLua::TYPE_VECTOR);
	gLua->CheckType(4, GLua::TYPE_VECTOR);

	BASS_3DVECTOR *pos = Get3DVect((Vector *)gLua->GetUserData(1));
	BASS_3DVECTOR *vel = Get3DVect((Vector *)gLua->GetUserData(2));
	BASS_3DVECTOR *front = Get3DVect((Vector *)gLua->GetUserData(3));
	BASS_3DVECTOR *up = Get3DVect((Vector *)gLua->GetUserData(4));

	BASS_Set3DFactors(1, 0.01, -1.0);
	BASS_Set3DPosition(pos, vel, front, up);

	BASS_Apply3D();

	delete pos;
	delete vel;
	delete front;
	delete up;

	return 0;
}

LUA_FUNCTION(bass_streamfile)
{
	ILuaInterface *gLua = Lua();
	gLua->CheckType(1, GLua::TYPE_STRING);
	const char *file = gLua->GetString(1);
	char buff[1024];
	_snprintf(buff, sizeof(buff), "garrysmod/sound/%s", file);

	DWORD handle = BASS_StreamCreateFile(false, buff, 0, 0, BASS_SAMPLE_MONO | BASS_SAMPLE_3D);

	ILuaObject* Channel = gLua->GetMetaTable(META_CHANNEL, TYPE_CHANNEL);
	gLua->PushUserData(Channel, (void *)handle);
	Channel->UnReference();
	return 1;
}

unsigned long WINAPI streamProc(void *param)
{
	streamProcData *data = (streamProcData *)param;
	DWORD handle = BASS_StreamCreateURL(data->file, 0, BASS_STREAM_BLOCK | BASS_SAMPLE_MONO | BASS_SAMPLE_3D, NULL, 0);
	
	data->error = BASS_ErrorGetCode();
	data->handle = handle;
	data->pending->add(data);

	return 0;
}

LUA_FUNCTION(bass_streamfileurl)
{
	ILuaInterface *gLua = Lua();
	gLua->CheckType(1, GLua::TYPE_STRING);
	gLua->CheckType(2, GLua::TYPE_FUNCTION);

	char *file = strdup(gLua->GetString(1));
	int callback = gLua->GetReference(2);

	streamProcData *data = new streamProcData;
	data->file = file;
	data->callback = callback;
	data->pending = GetPendingChannels(gLua);

	CreateThread(NULL, 0, &streamProc, data, 0, NULL); 

	return 0;
}

LUA_FUNCTION(poll)
{
	ILuaInterface *gLua = Lua();

	SyncList<streamProcData *> *results = GetPendingChannels(gLua);

	while(results->getSize() > 0)
	{
		streamProcData *qres = results->remove();
		if(!qres) continue;

		gLua->PushReference(qres->callback);

		if(qres->handle)
		{
			ILuaObject* Channel = gLua->GetMetaTable(META_CHANNEL, TYPE_CHANNEL);
			gLua->PushUserData(Channel, (void *)qres->handle);
			Channel->UnReference();
		} else {
			gLua->Push(false);
		}

		gLua->Push((float)qres->error);
		gLua->Call(2);

		free(qres->file);
		gLua->FreeReference(qres->callback);
		delete qres;
	}
	return 0;
}


int Start( lua_State* L )
{
	ILuaInterface *gLua = Lua();
	BASS_Init(-1, 44100, BASS_DEVICE_3D, 0, NULL);
	BASS_SetConfig(BASS_CONFIG_FLOATDSP, true);
	BASS_SetConfig(BASS_CONFIG_NET_PLAYLIST, 1);

	ILuaObject* table = gLua->GetNewTable();
		table->SetMember("StreamFile", bass_streamfile);
		table->SetMember("StreamFileURL", bass_streamfileurl);
		table->SetMember("SetPosition", bass_setposition);

		SyncList<streamProcData *>* pending = new SyncList<streamProcData *>();
		table->SetMemberUserDataLite( "p_PendingChannels", pending );

	gLua->SetGlobal("BASS", table);
	table->UnReference();

	table = gLua->GetMetaTable(META_CHANNEL, TYPE_CHANNEL);
		table->SetMember("__gc",       channel_gc);
		ILuaObject* __index = gLua->GetNewTable();
			__index->SetMember("play",	channel_play);
			__index->SetMember("pause",	channel_pause);
			__index->SetMember("stop",	channel_stop);
			__index->SetMember("getlength",	channel_getlength);
			__index->SetMember("getposition",	channel_getposition);
			__index->SetMember("gettag", channel_gettag);
			__index->SetMember("getplaying", channel_getplaying);
			__index->SetMember("getlevel", channel_getlevel);
			__index->SetMember("fft2048", channel_fft2048);
			__index->SetMember("set3dposition", channel_set3dposition);
			__index->SetMember("getrawtag", channel_getrawtag);
		table->SetMember("__index", __index);
		__index->UnReference();
	table->UnReference();

	ILuaObject *hookt = gLua->GetGlobal("hook");
	ILuaObject *addf = hookt->GetMember("Add");
	addf->Push();
	gLua->Push("Think");
	gLua->Push("BASSCallback");
	gLua->Push(poll);
	gLua->Call(3);

	return 0;
}

int Close( lua_State* L )
{
	BASS_Free();
	return 0;
}