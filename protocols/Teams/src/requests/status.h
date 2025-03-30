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

#ifndef _SKYPE_REQUEST_STATUS_H_
#define _SKYPE_REQUEST_STATUS_H_

struct SetStatusRequest : public AsyncHttpRequest
{
	SetStatusRequest(const char *status) :
		AsyncHttpRequest(REQUEST_PUT, HOST_DEFAULT, "/users/ME/presenceDocs/messagingService", &CTeamsProto::OnStatusChanged)
	{
		JSONNode node(JSON_NODE);
		node << CHAR_PARAM("status", status);
		m_szParam = node.write().c_str();
	}
};

#endif //_SKYPE_REQUEST_STATUS_H_
