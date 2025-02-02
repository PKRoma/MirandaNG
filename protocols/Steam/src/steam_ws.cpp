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

void __cdecl CSteamProto::ServerThread(void *)
{
	// load web socket servers first if needed
	int iTimeDiff = db_get_dw(0, MODULENAME, DBKEY_HOSTS_DATE);
	int iHostCount = db_get_dw(0, MODULENAME, DBKEY_HOSTS_COUNT);
	if (!iHostCount || time(0) - iTimeDiff > 3600 * 24 * 7) { // once a week
		if (!SendRequest(new GetHostsRequest(), &CSteamProto::OnGotHosts)) {
			Logout();
			return;
		}
		iHostCount = db_get_dw(0, MODULENAME, DBKEY_HOSTS_COUNT);
	}

	srand(time(0));
	m_ws = nullptr;

	CMStringA szHost;
	szHost.Format("Host%d", rand() % iHostCount);
	szHost = db_get_sm(0, MODULENAME, szHost);
	szHost.Insert(0, "wss://");
	szHost += "/cmsocket/";

	WebSocket<CSteamProto> ws(this);

	NLHR_PTR pReply(ws.connect(m_hNetlibUser, szHost));
	if (pReply) {
		if (pReply->resultCode == 101) {
			m_ws = &ws;

			debugLogA("Websocket connection succeeded");

			// Send init packets
			Login();

			ws.run();
		}
		else debugLogA("websocket connection failed: %d", pReply->resultCode);
	}
	else debugLogA("websocket connection failed");

	Logout();
	m_impl.m_heartBeat.Stop();
	m_ws = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////

void WebSocket<CSteamProto>::process(const uint8_t *buf, size_t cbLen)
{
	p->ProcessMessage(buf, cbLen);
}

void CSteamProto::ProcessMulti(const uint8_t *buf, size_t cbLen)
{
	proto::MsgMulti pMulti(buf, cbLen);
	if (pMulti == nullptr) {
		debugLogA("Unable to decode multi message, exiting");
		return;
	}

	debugLogA("processing %s multi message of size %d", (pMulti->size_unzipped) ? "zipped" : "normal", pMulti->message_body.len);
	
	ptrA tmp;
	if (pMulti->size_unzipped) {
		tmp = (char *)mir_alloc(pMulti->size_unzipped + 1);
		cbLen = FreeImage_ZLibGUnzip((uint8_t*)tmp.get(), pMulti->size_unzipped, pMulti->message_body.data, (unsigned)pMulti->message_body.len);
		if (!cbLen) {
			debugLogA("Unable to unzip multi message, exiting");
			return;
		}

		buf = (const uint8_t *)tmp.get();
	}
	else {
		buf = pMulti->message_body.data;
		cbLen = pMulti->message_body.len;
	}

	while ((int)cbLen > 0) {
		uint32_t cbPacketLen = *(uint32_t *)buf; buf += sizeof(uint32_t); cbLen -= sizeof(uint32_t);
		ProcessMessage(buf, cbPacketLen);
		buf += cbPacketLen; cbLen -= cbPacketLen;
	}
}

void CSteamProto::ProcessMessage(const uint8_t *buf, size_t cbLen)
{
	ptrA szTargetJobName;
	CMsgProtoBufHeader hdr;
	uint32_t dwSign = *(uint32_t *)buf; buf += sizeof(uint32_t); cbLen -= sizeof(uint32_t);
	EMsg msgType = (EMsg)(dwSign & ~STEAM_PROTOCOL_MASK);
	bool bIsProtobuf = (dwSign & STEAM_PROTOCOL_MASK) != 0;

	if (msgType == EMsg::ChannelEncryptRequest || msgType == EMsg::ChannelEncryptResult) {
		hdr.has_jobid_source = hdr.has_jobid_target = true;
		hdr.jobid_source = *(int64_t *)buf; buf += sizeof(int64_t);
		hdr.jobid_target = *(int64_t *)buf; buf += sizeof(int64_t);
		debugLogA("Encrypted results cannot be processed, ignoring");
		return;
	}
	
	if (msgType == EMsg::Multi) {
		buf += sizeof(uint32_t); cbLen -= sizeof(uint32_t);
		ProcessMulti(buf, cbLen);
		return;
	}

	if (bIsProtobuf) {
		uint32_t hdrLen = *(uint32_t *)buf; buf += sizeof(uint32_t); cbLen -= sizeof(uint32_t);
		auto *p = cmsg_proto_buf_header__unpack(0, hdrLen, buf);
		if (p == nullptr) {
			debugLogA("Unable to decode message header, exiting");
			return;
		}

		buf += hdrLen; cbLen -= hdrLen;
		memcpy(&hdr, p, sizeof(hdr));
		if (hdr.target_job_name) {
			szTargetJobName = mir_strdup(hdr.target_job_name);
			hdr.target_job_name = szTargetJobName;
		}

		cmsg_proto_buf_header__free_unpacked(p, 0);

		if (hdr.has_client_sessionid)
			m_iSessionId = hdr.client_sessionid;
	}
	else { // non protobuf message
		buf += 3; // 1 byte for header size (fixed at 36), 2 bytes for header version (fixed at 2)
		hdr.jobid_target = *(uint64_t *)buf; buf += sizeof(uint64_t);
		hdr.jobid_source = *(uint64_t *)buf; buf += sizeof(uint64_t);
		buf++; // 1 byte for header canary (fixed at 239)
		hdr.steamid = *(uint64_t *)buf; buf += sizeof(uint64_t);
		hdr.client_sessionid = *(uint32_t *)buf; buf += sizeof(uint32_t);
		hdr.has_jobid_target = hdr.has_jobid_source = hdr.has_steamid = hdr.has_client_sessionid = true;
		cbLen -= 30;
	}

	if (hdr.has_eresult && hdr.eresult != (int)EResult::OK)
		debugLogA("HDR: error code %d", hdr.eresult);

	// persistent callbacks
	switch (msgType) {
	case EMsg::ServiceMethod:
	case EMsg::ServiceMethodResponse:
		ProcessServiceResponse(buf, cbLen, hdr);
		break;

	default:
		// find message descriptor first, if succeeded, try to find a message handler then
		auto md = g_plugin.messages.find(msgType);
		if (md == g_plugin.messages.end()) {
			debugLogA("Received message of type %d", msgType);
			Netlib_Dump(HNETLIBCONN(m_ws->getConn()), buf, cbLen, false, 0);
		}
		else if (auto *pMessage = protobuf_c_message_unpack(md->second, 0, cbLen, buf)) {
			debugLogA("Received known message:\n%s", protobuf_c_text_to_string(*pMessage).c_str());

			auto mh = g_plugin.messageHandlers.find(msgType);
			if (mh != g_plugin.messageHandlers.end())
				(this->*(mh->second))(*pMessage, hdr);

			protobuf_c_message_free_unpacked(pMessage, 0);
		}
	}
}

void CSteamProto::ProcessServiceResponse(const uint8_t *buf, size_t cbLen, const CMsgProtoBufHeader &hdr)
{
	char *tmpName = NEWSTR_ALLOCA(hdr.target_job_name);
	char *p = strchr(tmpName, '.');
	if (!p) {
		debugLogA("Invalid service function: %s", hdr.target_job_name);
		return;
	}

	*p = 0;
	auto it = g_plugin.services.find(tmpName);
	if (it == g_plugin.services.end()) {
		debugLogA("Unregistered service module: %s", tmpName);
		return;
	}
	*p = '.';

	auto pHandler = g_plugin.serviceHandlers.find(tmpName);
	if (pHandler == g_plugin.serviceHandlers.end()) {
		debugLogA("Unsupported service function: %s", hdr.target_job_name);
		return;
	}

	if (char *p1 = strchr(++p, '#'))
		*p1 = 0;

	if (auto *pMethod = protobuf_c_service_descriptor_get_method_by_name(it->second, p)) {
		auto *pDescr = (hdr.jobid_target == -1) ? pMethod->input : pMethod->output;
		
		if (auto *pMessage = protobuf_c_message_unpack(pDescr, 0, cbLen, buf)) {
			debugLogA("Processing service message: %s\n%s", hdr.target_job_name, protobuf_c_text_to_string(*pMessage).c_str());

			(this->*(pHandler->second))(*pMessage, hdr);
			protobuf_c_message_free_unpacked(pMessage, 0);
		}
	}
	else debugLogA("Unregistered service method: %s", hdr.target_job_name);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CSteamProto::WSSend(EMsg msgType, const ProtobufCppMessage &msg)
{
	CMsgProtoBufHeader hdr;
	hdr.has_client_sessionid = hdr.has_steamid = hdr.has_jobid_source = hdr.has_jobid_target = true;
	hdr.steamid = m_iSteamId;
	hdr.client_sessionid = m_iSessionId;

	switch (msgType) {
	case EMsg::ClientHello:
		hdr.jobid_source = -1;
		break;

	default:
		hdr.jobid_source = getRandomInt();
		break;
	}

	hdr.jobid_target = -1;

	WSSendHeader(msgType, hdr, msg);
}

void CSteamProto::WSSendRaw(EMsg msgType, const MBinBuffer &body)
{
	MBinBuffer payload;
	payload << (uint32_t)(int)msgType << uint8_t(36) << uint16_t(2) << uint64_t(-1) << getRandomInt()
		<< uint8_t(239) << m_iSteamId << m_iSessionId;
	payload.append(body);

	m_ws->sendBinary(payload.data(), payload.length());
}

void CSteamProto::WSSendHeader(EMsg msgType, const CMsgProtoBufHeader &hdr, const ProtobufCppMessage &msg)
{
	debugLogA("Message sent:\n%s", protobuf_c_text_to_string(msg).c_str());

	uint32_t hdrLen = (uint32_t)protobuf_c_message_get_packed_size(&hdr);
	MBinBuffer hdrbuf(hdrLen);
	protobuf_c_message_pack(&hdr, hdrbuf.data());
	hdrbuf.appendBefore(&hdrLen, sizeof(hdrLen));

	uint32_t type = (uint32_t)msgType;
	type |= STEAM_PROTOCOL_MASK;
	hdrbuf.appendBefore(&type, sizeof(type));

	MBinBuffer body(protobuf_c_message_get_packed_size(&msg));
	protobuf_c_message_pack(&msg, body.data());

	hdrbuf.append(body);
	m_ws->sendBinary(hdrbuf.data(), hdrbuf.length());
}

void CSteamProto::WSSendAnon(const char *pszServiceName, const ProtobufCppMessage &msg)
{
	CMsgProtoBufHeader hdr;
	hdr.has_client_sessionid = hdr.has_steamid = hdr.has_jobid_source = hdr.has_jobid_target = true;
	hdr.jobid_source = getRandomInt();
	hdr.jobid_target = -1;
	hdr.target_job_name = (char *)pszServiceName;
	WSSendHeader(EMsg::ServiceMethodCallFromClientNonAuthed, hdr, msg);
}

void CSteamProto::WSSendService(const char *pszServiceName, const ProtobufCppMessage &msg, void *pInfo)
{
	CMsgProtoBufHeader hdr;
	hdr.has_client_sessionid = hdr.has_steamid = hdr.has_jobid_source = hdr.has_jobid_target = true;
	hdr.steamid = m_iSteamId, hdr.client_sessionid = m_iSessionId;
	hdr.jobid_source = getRandomInt();
	hdr.jobid_target = -1;
	hdr.target_job_name = (char *)pszServiceName;

	if (pInfo)
		SetRequestInfo(hdr.jobid_source, pInfo);

	WSSendHeader(EMsg::ServiceMethodCallFromClient, hdr, msg);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CSteamProto::SetRequestInfo(uint64_t requestId, void *pInfo)
{
	mir_cslock lck(m_csRequestLock);
	m_requestInfo[requestId] = pInfo;
}

void* CSteamProto::GetRequestInfo(uint64_t requestId)
{
	mir_cslock lck(m_csRequestLock);
	auto it = m_requestInfo.find(requestId);
	if (it == m_requestInfo.end())
		return nullptr;

	void *pRet = it->second;
	m_requestInfo.erase(it);
	return pRet;
}
