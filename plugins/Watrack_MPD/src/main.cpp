// Copyright (c) 2008 sss, chaos.persei
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "stdafx.h"

HNETLIBCONN ghConnection;
HANDLE ghPacketReciever;
BOOL Connected;
int gbState;
SONGINFO SongInfo = {};

void Start(void*)
{
	Connected = FALSE;
	ghConnection = Netlib_OpenConnection(ghNetlibUser, _T2A(gbHost), gbPort, 5);
	if (ghConnection)
		ghPacketReciever = Netlib_CreatePacketReceiver(ghConnection, 2048);
}

void ReStart(void*)
{
	if(ghPacketReciever)
		Netlib_CloseHandle(ghPacketReciever);
	if(ghConnection)
		Netlib_CloseHandle(ghConnection);
	Sleep(500);
	mir_forkthread(&Start);
}

int Parser()
{
	static NETLIBPACKETRECVER nlpr = {};
	char *ptr;
	int i;
	char *buf;
	static char ver[16];
	nlpr.dwTimeout = 5;
	if(!ghConnection)
	{
		mir_forkthread(&Start);
	}
	if(ghConnection)
	{	
		int recvResult;
		if(!Connected)
		{
			char tmp[128];
			char *tmp2 = mir_u2a(gbPassword);
			recvResult = Netlib_GetMorePackets(ghPacketReciever, &nlpr);
			if(recvResult == SOCKET_ERROR)
			{
				mir_forkthread(&ReStart);
//				ReStart();
				return 1;
			}
			if(mir_strlen(tmp2) > 2)
			{
				mir_strcpy(tmp, "password ");
				mir_strcat(tmp, tmp2);
				mir_strcat(tmp, "\n");
				Netlib_Send(ghConnection, tmp, (int)mir_strlen(tmp), 0);
				recvResult = Netlib_GetMorePackets(ghPacketReciever, &nlpr);
				if(recvResult == SOCKET_ERROR)
				{
					mir_forkthread(&ReStart);
					return 1;
				}
			}
			mir_free(tmp2);
		}
		Netlib_Send(ghConnection, "status\n", (int)mir_strlen("status\n"), 0);
		recvResult = Netlib_GetMorePackets(ghPacketReciever, &nlpr);
		if(recvResult == SOCKET_ERROR)
		{
			mir_forkthread(&ReStart);
			return 1;
		}
		Netlib_Send(ghConnection, "currentsong\n", (int)mir_strlen("currentsong\n"), 0);
		recvResult = Netlib_GetMorePackets(ghPacketReciever, &nlpr);
		if(recvResult == SOCKET_ERROR)
		{
			mir_forkthread(&ReStart);
			return 1;
		}
		nlpr.bytesUsed = nlpr.bytesAvailable;
	}
	char tmp[256];
	buf = (char*)nlpr.buffer;
	if(ptr = strstr(buf, "MPD"))
	{
		Connected = TRUE;
		ptr = &ptr[4];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		strncpy_s(ver, tmp, _TRUNCATE);
		SongInfo.txtver = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.txtver = mir_utf8decodeW(ver);
	if(ptr = strstr(buf, "file:"))
	{
		ptr = &ptr[6];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.mfile = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.mfile = mir_wstrdup(L"");
	if(ptr = strstr(buf, "Time:"))
	{
		ptr = &ptr[6];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.total = atoi(tmp);
	}
	else if(!SongInfo.total)
		SongInfo.total = 0;
	if(ptr = strstr(buf, "time:"))
	{
		ptr = &ptr[6];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.time = atoi(tmp);
	}
	else if(!SongInfo.time)
		SongInfo.time = 0;
	if(ptr = strstr(buf, "Title:"))
	{
		ptr = &ptr[7];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.title = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.title = mir_wstrdup(L"Unknown track");
	if(ptr = strstr(buf, "Artist:"))
	{
		ptr = &ptr[8];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.artist = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.artist = mir_wstrdup(L"Unknown artist");
	if(ptr = strstr(buf, "Genre:"))
	{
		ptr = &ptr[7];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.genre = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.genre =  mir_wstrdup(L"Unknown genre");
	if(ptr = strstr(buf, "Album:"))
	{
		ptr = &ptr[7];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.album = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.album =  mir_wstrdup(L"Unknown album");
	if(ptr = strstr(buf, "Date:"))
	{
		ptr = &ptr[6];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.year = mir_utf8decodeW(tmp);
	}
	else
		SongInfo.year =  mir_wstrdup(L"Unknown year");
	if(ptr = strstr(buf, "volume:"))
	{
		ptr = &ptr[8];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.volume = atoi(tmp);
	}
	else if(!SongInfo.volume)
		SongInfo.volume = 0;
	if(ptr = strstr(buf, "audio:"))
	{
		ptr = &ptr[7];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.khz = atoi(tmp);
	}
	else if(!SongInfo.khz)
		SongInfo.khz = 0;
	if(ptr = strstr(buf, "bitrate:"))
	{
		ptr = &ptr[9];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.kbps = atoi(tmp);
	}
	else if(!SongInfo.kbps)
		SongInfo.kbps = 0;

	if(ptr = strstr(buf, "Track:"))
	{
		ptr = &ptr[7];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		SongInfo.track = atoi(tmp);
	}
	else if(!SongInfo.track)
		SongInfo.track = 0;
	if(ptr = strstr(buf, "state:"))
	{
		ptr = &ptr[7];
		for(i = 0; ((ptr[i] != '\n') && (ptr[i] != '\0')); i++)
			tmp[i] = ptr[i];
		tmp[i] = '\0';
		if(strstr(tmp, "play"))
			gbState = WAT_PLS_PLAYING;
		if(strstr(tmp, "pause"))
			gbState = WAT_PLS_PAUSED;
		if(strstr(tmp, "stop"))
			gbState = WAT_PLS_STOPPED;
	}
	else if(!gbState)
		gbState = WAT_PLS_UNKNOWN;
	return 0;
}

void Stop()
{
	if(ghPacketReciever)
		Netlib_CloseHandle(ghPacketReciever);
	if(ghConnection)
		Netlib_CloseHandle(ghConnection);
}

int Init()
{
	mir_forkthread(&Start);
	return 0;
}

int DeInit()
{
	Stop();
	return 0;
}

HWND CheckPlayer(HWND, int)
{
	if(!ghConnection)
	{
		mir_forkthread(&Start);
		return nullptr;
	}
	if(Parser())
		return (HWND)WAT_PLS_STOPPED;
	if(Connected)		
		return (HWND)WAT_PLS_PLAYING;
	return nullptr;
}

int GetStatus(HWND)
{
	if(!ghConnection)
	{
		mir_forkthread(&Start);
		return 0;
	}
	return Parser() ? -1 : gbState;
}

wchar_t* GetFileName(HWND, int)
{
	if(!ghConnection)
	{
		mir_forkthread(&Start);
		return nullptr;
	}
	return nullptr;
}

int GetPlayerInfo(LPSONGINFO info, int)
{
	if(!ghConnection)
	{
		mir_forkthread(&Start);
		return 0;
	}
	if(Parser())
		return -1;
/*	
	
	info->channels = SongInfo.channels;
	info->codec = SongInfo.codec;
	info->comment = SongInfo.comment;
	info->cover = SongInfo.cover;
	info->date = SongInfo.date;
	info->fps = SongInfo.fps;
	info->fsize = SongInfo.fsize;
	
	info->icon = SongInfo.icon;
	info->kbps = SongInfo.kbps;
	info->khz = SongInfo.khz;
	info->lyric = SongInfo.lyric;
	info->mfile = SongInfo.mfile;
	info->player = SongInfo.player;
	info->plyver = SongInfo.plyver;
	info->status = SongInfo.status;
	info->time = SongInfo.time;
	info->title = SongInfo.title;
	info->total = SongInfo.total;
	info->track = SongInfo.track;*/
	info->total = SongInfo.total;
	info->time = SongInfo.time;
	info->mfile = SongInfo.mfile;
	info->txtver = SongInfo.txtver;
	info->title = SongInfo.title; 
	info->artist = SongInfo.artist;
	info->genre = SongInfo.genre;
	info->album = SongInfo.album;
	info->year = SongInfo.year;
	info->kbps = SongInfo.kbps;
	info->track = SongInfo.track;
	info->khz = SongInfo.khz;
	info->volume = SongInfo.volume;
/*	info->url = SongInfo.url; //??
	info->vbr = SongInfo.vbr;
	info->volume = SongInfo.volume;
	*/
	return 0;
}

int SendCommand(HWND, int command, int)
{
	switch (command)
	{
	case WAT_CTRL_PREV:
		Netlib_Send(ghConnection, "previous\n", (int)mir_strlen("previous\n"), 0);
		return 0;
	case WAT_CTRL_PLAY: //add resuming support
		if(gbState != WAT_PLS_PAUSED)
			Netlib_Send(ghConnection, "play\n", (int)mir_strlen("play\n"), 0);
		else
			Netlib_Send(ghConnection, "pause 0\n", (int)mir_strlen("pause 0\n"), 0);
		return 0;
	case WAT_CTRL_PAUSE:
		Netlib_Send(ghConnection, "pause 1\n", (int)mir_strlen("pause 1\n"), 0);
		return 0;
	case WAT_CTRL_STOP:
		Netlib_Send(ghConnection, "stop\n", (int)mir_strlen("stop\n"), 0);
		return 0;
	case WAT_CTRL_NEXT:
		Netlib_Send(ghConnection, "next\n", (int)mir_strlen("next\n"), 0);
		return 0;
	case WAT_CTRL_VOLDN:
		return 0;
	case WAT_CTRL_VOLUP:
		return 0;
	case WAT_CTRL_SEEK:
		return 0;
	default:
		return 0;
	}
}

void RegisterPlayer()
{
	if(bWatrackService)
	{
		PLAYERCELL player = {};
		player.Desc = "Music Player Daemon";
		player.Notes = L"mpd is a nice music player for *nix which have not any gui, just daemon.\nuses very small amount of ram, cpu.";
		player.URL = "http://www.musicpd.org";
		player.Check = CheckPlayer;
		player.Init = Init;
		player.DeInit = DeInit;
		player.Command = SendCommand;
		player.GetStatus = GetStatus;
		player.GetName = GetFileName;
		player.GetInfo = GetPlayerInfo;
		player.flags = (WAT_OPT_HASURL|WAT_OPT_SINGLEINST|WAT_OPT_PLAYERINFO);
//		player.Icon = //TODO:implement icon support
		CallService(MS_WAT_PLAYER, WAT_ACT_REGISTER, (LPARAM)&player);
	}
}
