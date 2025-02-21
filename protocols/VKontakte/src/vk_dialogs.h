/*
Copyright (c) 2013-25 Miranda NG team (https://miranda-ng.org)

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

#include "stdafx.h"
#pragma once

typedef CProtoDlgBase<CVkProto> CVkDlgBase;

////////////////////////////////// IDD_CAPTCHAFORM ////////////////////////////////////////

struct CAPTCHA_FORM_PARAMS
{
	HBITMAP bmp;
	int w, h;
	char Result[100];
};

class CVkCaptchaForm : public CVkDlgBase
{
	CCtrlData m_instruction;
	CCtrlEdit m_edtValue;
	CCtrlButton m_btnOpenInBrowser;
	CCtrlButton m_btnOk;
	CAPTCHA_FORM_PARAMS *m_param;

public:
	CVkCaptchaForm(CVkProto *proto, CAPTCHA_FORM_PARAMS *param);

	bool OnInitDialog() override;
	bool OnApply() override;
	void OnDestroy() override;

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override;
	
	void On_btnOpenInBrowser_Click(CCtrlButton*);
	void On_edtValue_Change(CCtrlEdit*);

};

////////////////////////////////// IDD_TOKENFORM ////////////////////////////////////////

class CVkTokenForm : public CVkDlgBase
{
	CCtrlData m_instruction;
	CCtrlEdit m_edtValue;
	CCtrlButton m_btnTokenReq;
	CCtrlButton m_btnOk;
	CMStringA m_TokenReq;
	CMStringA m_szAccName;
	
public:
	CVkTokenForm(CVkProto* proto, const char* _szTokenReq);

	bool OnInitDialog() override;
	bool OnApply() override;
	void OnDestroy() override;

	void On_btnTokenReq_Click(CCtrlButton*);
	void On_edtValue_Change(CCtrlEdit*);

	char Result[4096];

};

////////////////////////////////// IDD_WALLPOST ///////////////////////////////////////////

struct WALLPOST_FORM_PARAMS
{
	wchar_t *pwszMsg;
	wchar_t *pwszUrl;
	wchar_t *pwszNick;
	bool bFriendsOnly;

	WALLPOST_FORM_PARAMS(wchar_t *nick) :
		pwszNick(nick),
		bFriendsOnly(false)
	{
		pwszMsg = pwszUrl = nullptr;
	}

	~WALLPOST_FORM_PARAMS()
	{
		mir_free(pwszMsg);
		mir_free(pwszUrl);
		mir_free(pwszNick);
	}
};

class CVkWallPostForm : public CVkDlgBase
{
	CCtrlEdit m_edtMsg;
	CCtrlEdit m_edtUrl;
	CCtrlCheck m_cbOnlyForFriends;
	CCtrlButton m_btnShare;

	WALLPOST_FORM_PARAMS *m_param;

public:
	CVkWallPostForm(CVkProto *proto, WALLPOST_FORM_PARAMS *param);

	bool OnInitDialog() override;
	bool OnApply() override;
	void OnDestroy() override;

	void On_edtValue_Change(CCtrlEdit*);
};

////////////////////////////////// IDD_INVITE /////////////////////////////////////////////

class CVkInviteChatForm : public CVkDlgBase
{
	CCtrlCombo m_cbxCombo;

public:
	MCONTACT m_hContact;

	CVkInviteChatForm(CVkProto *proto);

	bool OnInitDialog() override;
	bool OnApply() override;
};


////////////////////////////////// IDD_VKUSERFORM //////////////////////////////////////////

class CVkUserListForm : public CVkDlgBase
{
	CCtrlClc m_clc;
	CCtrlEdit m_edtMessage;
	CCtrlBase m_stListCaption;
	CCtrlBase m_stMessageCaption;


public:
	CMStringW wszFormCaption;
	CMStringW wszListCaption;
	CMStringW wszMessageCaption;
	CMStringW wszMessage;
	LIST<void> lContacts;
	uint8_t uClcFilterFlag;
	UINT uMaxLengthMessage;

	CVkUserListForm(CVkProto* proto);
	CVkUserListForm(CVkProto* proto, CMStringW _wszMessage, CMStringW _wszFormCaption, CMStringW _wszListCaption, CMStringW _wszMessageCaption, uint8_t _uFilterClcFlag, UINT _uMaxLengthMessage = 0);
	bool OnInitDialog() override;
	bool OnApply() override;

	void FilterList(CCtrlClc*);
	void ResetListOptions();
};
