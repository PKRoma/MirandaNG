/*
Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org)

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

class COptionsMainDlg : public CTeamsDlgBase
{
	CCtrlEdit m_login, m_password, m_group, m_place;
	CCtrlCheck m_autosync, m_usehostname, m_usebb;
	CCtrlHyperlink m_link;

public:
	COptionsMainDlg(CTeamsProto *proto, int idDialog) :
		CTeamsDlgBase(proto, idDialog),
		m_login(this, IDC_LOGIN),
		m_password(this, IDC_PASSWORD),
		m_group(this, IDC_GROUP),
		m_place(this, IDC_PLACE),
		m_autosync(this, IDC_AUTOSYNC),
		m_usehostname(this, IDC_USEHOST),
		m_usebb(this, IDC_BBCODES),
		m_link(this, IDC_CHANGEPASS, "https://login.skype.com/recovery/password-change") // TODO : ...?username=%username%
	{
		CreateLink(m_group, proto->m_wstrCListGroup);
		CreateLink(m_autosync, proto->m_bAutoHistorySync);
		CreateLink(m_place, proto->m_wstrPlace);
		CreateLink(m_usehostname, proto->m_bUseHostnameAsPlace);
		CreateLink(m_usebb, proto->m_bUseBBCodes);
		m_usehostname.OnChange = Callback(this, &COptionsMainDlg::OnUsehostnameCheck);
	}

	bool OnInitDialog() override
	{
		m_login.SetTextA(ptrA(m_proto->getStringA(SKYPE_SETTINGS_ID)));
		m_password.SetTextA(pass_ptrA(m_proto->getStringA("Password")));
		m_place.Enable(!m_proto->m_bUseHostnameAsPlace);
		m_login.SendMsg(EM_LIMITTEXT, 128, 0);
		m_password.SendMsg(EM_LIMITTEXT, 128, 0);
		m_group.SendMsg(EM_LIMITTEXT, 64, 0);
		return true;
	}

	bool OnApply() override
	{
		ptrA szNewSkypename(m_login.GetTextA()),
			szOldSkypename(m_proto->getStringA(SKYPE_SETTINGS_ID));
		pass_ptrA szNewPassword(m_password.GetTextA()),
			szOldPassword(m_proto->getStringA("Password"));
		if (mir_strcmpi(szNewSkypename, szOldSkypename) || mir_strcmp(szNewPassword, szOldPassword))
			m_proto->delSetting("TokenExpiresIn");
		m_proto->setString(SKYPE_SETTINGS_ID, szNewSkypename);
		m_proto->setString("Password", szNewPassword);
		ptrW group(m_group.GetText());
		if (mir_wstrlen(group) > 0 && !Clist_GroupExists(group))
			Clist_GroupCreate(0, group);
		return true;
	}

	void OnUsehostnameCheck(CCtrlCheck *)
	{
		m_place.Enable(!m_usehostname.GetState());
	}
};

/////////////////////////////////////////////////////////////////////////////////

MWindow CTeamsProto::OnCreateAccMgrUI(MWindow hwndParent)
{
	auto *pDlg = new COptionsMainDlg(this, IDD_ACCOUNT_MANAGER);
	pDlg->SetParent(hwndParent);
	pDlg->Show();
	return pDlg->GetHwnd();
}

int CTeamsProto::OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = { sizeof(odp) };
	odp.szTitle.w = m_tszUserName;
	odp.flags = ODPF_BOLDGROUPS | ODPF_UNICODE | ODPF_DONTTRANSLATE;
	odp.szGroup.w = LPGENW("Network");

	odp.szTab.w = LPGENW("Account");
	odp.pDialog = new COptionsMainDlg(this, IDD_OPTIONS_MAIN);
	g_plugin.addOptions(wParam, &odp);

	return 0;
}
