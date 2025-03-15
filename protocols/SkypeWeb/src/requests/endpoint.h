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

#ifndef _SKYPE_REQUEST_ENDPOINT_H_
#define _SKYPE_REQUEST_ENDPOINT_H_

struct CreateEndpointRequest : public AsyncHttpRequest
{
	CreateEndpointRequest(CSkypeProto *ppro) :
		AsyncHttpRequest(REQUEST_POST, HOST_DEFAULT, "/users/ME/endpoints", &CSkypeProto::OnEndpointCreated)
	{
		m_szParam = "{\"endpointFeatures\":\"Agent,Presence2015,MessageProperties,CustomUserProperties,Casts,ModernBots,AutoIdleForWebApi,secureThreads,notificationStream,InviteFree,SupportsReadReceipts,ued\"}";

		AddHeader("Origin", "https://web.skype.com");
		AddHeader("Referer", "https://web.skype.com/");
		AddAuthentication(ppro);
	}
};

struct DeleteEndpointRequest : public AsyncHttpRequest
{
	DeleteEndpointRequest(CSkypeProto *ppro) :
	  AsyncHttpRequest(REQUEST_DELETE, HOST_DEFAULT, "/users/ME/endpoints/" + mir_urlEncode(ppro->m_szId), &CSkypeProto::OnEndpointDeleted)
	{
	}
};

#endif //_SKYPE_REQUEST_ENDPOINT_H_
