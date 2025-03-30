/*
Copyright (c) 2015-25 Miranda NG team (https://miranda-ng.org)

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

#ifndef _SKYPE_REQUEST_PROFILE_H_
#define _SKYPE_REQUEST_PROFILE_H_

struct GetProfileRequest : public AsyncHttpRequest
{
	GetProfileRequest(CTeamsProto *ppro, MCONTACT hContact) :
		AsyncHttpRequest(REQUEST_GET, HOST_API, 0, &CTeamsProto::LoadProfile)
	{
		m_szUrl.AppendFormat("/users/%s/profile", (hContact == 0) ? "self" : mir_urlEncode(ppro->getId(hContact)));
		pUserInfo = (void *)hContact;

		AddHeader("Accept", "application/json");
	}
};

#endif //_SKYPE_REQUEST_PROFILE_H_
