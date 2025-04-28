/*
Copyright (c) 2014-17 Robert Pösel, 2017-25 Miranda NG team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

struct CDummyProto;

struct CDummyProto : public PROTO<CDummyProto>
{
				CDummyProto(const char*, const wchar_t*);
				~CDummyProto();

	//====================================================================================
	// PROTO_INTERFACE
	//====================================================================================

	INT_PTR  GetCaps(int type, MCONTACT hContact = NULL) override;

	int      SendMsg(MCONTACT hContact, MEVENT, const char* msg) override;

	int      SetStatus(int iNewStatus) override;

	HANDLE   SearchBasic(const wchar_t* id) override;

	MCONTACT AddToList(int flags, PROTOSEARCHRESULT* psr) override;

	//======================================================================================

	MWindow OnCreateAccMgrUI(MWindow) override;
	
	void __cdecl SearchIdAckThread(void*);
	
	char uniqueIdText[100];
	wchar_t uniqueIdSetting[100];

	int getTemplateId();
	void selectTemplate(HWND, int templateId);

	CMOption<bool> bAllowSending;
	volatile unsigned int msgid;
};
