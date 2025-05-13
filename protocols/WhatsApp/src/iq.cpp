/*

WhatsApp plugin for Miranda NG
Copyright © 2019-25 George Hazan

*/

#include "stdafx.h"

void WhatsAppProto::OnAccountSync(const WANode &node)
{
	if (auto *pList = node.getChild("devices")) {
		auto *pUser = FindUser(m_szJid);
		pUser->arDevices.destroy();

		for (auto &it : pList->getChildren())
			if (it->title == "device")
				pUser->arDevices.insert(new WAJid(it->getAttr("jid"), it->getAttrInt("key-index")));
	}

	if (auto *pList = node.getChild("blocklist"))
		for (auto &it : pList->getChildren())
			if (it->title == "item") {
				auto *pUser = AddUser(it->getAttr("jid"), false);
				Contact::Hide(pUser->hContact, 0 == mir_strcmp(it->getAttr("action"), "block"));
			}

	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqBlockList(const WANode &node)
{
	for (auto &it : node.getChild("list")->getChildren()) {
		auto *pUser = AddUser(it->getAttr("jid"), false);
		Contact::Hide(pUser->hContact);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqCountPrekeys(const WANode &node)
{
	m_bUpdatedPrekeys = true;

	int iCount = node.getChild("count")->getAttrInt("value");
	if (iCount < 5)
		UploadMorePrekeys();
}

void WhatsAppProto::UploadMorePrekeys()
{
	WANodeIq iq(IQ::SET, "encrypt");

	auto regId = encodeBigEndian(getDword(DBKEY_REG_ID));
	iq.addChild("registration")->content.append(regId.c_str(), regId.size());

	iq.addChild("type")->content.append(KEY_BUNDLE_TYPE, 1);
	iq.addChild("identity")->content.append(m_signalStore.signedIdentity.pub);

	const int PORTION = 10;
	m_signalStore.generatePrekeys(PORTION);

	int iStart = getDword(DBKEY_PREKEY_UPLOAD_ID, 1);
	auto *n = iq.addChild("list");
	for (int i = 0; i < PORTION; i++) {
		auto *nKey = n->addChild("key");

		int keyId = iStart + i;
		auto encId = encodeBigEndian(keyId, 3);
		nKey->addChild("id")->content.append(encId.c_str(), encId.size());
		nKey->addChild("value")->content.append(getBlob(CMStringA(FORMAT, "PreKey%dPublic", keyId)));
	}
	setDword(DBKEY_PREKEY_UPLOAD_ID, iStart + PORTION);

	auto *skey = iq.addChild("skey");

	auto encId = encodeBigEndian(m_signalStore.preKey.keyid, 3);
	skey->addChild("id")->content.append(encId.c_str(), encId.size());
	skey->addChild("value")->content.append(m_signalStore.preKey.pub);
	skey->addChild("signature")->content.append(m_signalStore.preKey.signature);

	WSSendNode(iq, &WhatsAppProto::OnIqDoNothing);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqDoNothing(const WANode&)
{
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqGetKeys(const WANode &node, void *pUserInfo)
{
	for (auto &it : node.getChild("list")->getChildren())
		if (it->title == "user")
			m_signalStore.injectSession(it->getAttr("jid"), it, it);

	// don't forget to send delayed message when all keys are retrieved
	if (pUserInfo)
		FinishTask((WASendTask *)pUserInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqGetUsync(const WANode &node, void *pUserInfo)
{
	WANodeIq iq(IQ::GET, "encrypt");
	auto *pKey = iq.addChild("key");

	for (auto *nUser : node.getChild("usync")->getChild("list")->getChildren()) {
		auto *pszJid = nUser->getAttr("jid");

		auto *pUser = AddUser(pszJid, false);
		pUser->bDeviceInit = true;
		pUser->arDevices.destroy();

		if (auto *pList = nUser->getChild("devices")->getChild("device-list"))
			for (auto &it : pList->getChildren())
				if (it->title == "device")
					pUser->arDevices.insert(new WAJid(pszJid, it->getAttrInt("id")));

		for (auto &it : pUser->arDevices) {
			auto blob = getBlob(MSignalSession(it->user, it->device).getSetting());
			if (blob.isEmpty())
				pKey->addChild("user")->addAttr("jid", it->toString());
		}
	}

	if (pKey->getChildren().getCount() > 0)
		WSSendNode(iq, &WhatsAppProto::OnIqGetKeys, pUserInfo);
	else if (pUserInfo)
		FinishTask((WASendTask *)pUserInfo);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqPairDevice(const WANode &node)
{
	WSSendNode(WANodeIq(IQ::RESULT) << CHAR_PARAM("id", node.getAttr("id")));

	if (m_bUnregister) {
		m_bTerminated = true;
		return;
	}

	if (auto *pRef = node.getChild("pair-device")->getChild("ref")) {
		ShowQrCode(pRef->getBody());
	}
	else {
		debugLogA("OnIqPairDevice: got reply without ref, exiting");
		ShutdownSession();
	}
}

void WhatsAppProto::OnIqPairSuccess(const WANode &node)
{
	CloseQrDialog();

	auto *pRoot = node.getChild("pair-success");

	try {
		if (auto *pPlatform = pRoot->getChild("platform"))
			debugLogA("Got response from platform: %s", pPlatform->getBody().c_str());

		if (auto *pBiz = pRoot->getChild("biz"))
			if (auto *pszName = pBiz->getAttr("name"))
				setUString("Nick", pszName);

		if (auto *pDevice = pRoot->getChild("device")) {
			if (auto *pszJid = pDevice->getAttr("jid")) {
				WAJid jid(pszJid);
				m_szJid = jid.user + "@" + jid.server;
				m_arUsers.insert(new WAUser(0, m_szJid, false));

				setUString(DBKEY_ID, m_szJid);
				setDword(DBKEY_DEVICE_ID, jid.device);
			}
		}
		else throw "OnIqPairSuccess: got reply without device info, exiting";

		if (auto *pIdentity = pRoot->getChild("device-identity")) {
			proto::ADVSignedDeviceIdentityHMAC payload(pIdentity->content);

			auto &hmac = payload->hmac;
			auto &details = payload->details;
			{
				// check details signature using HMAC
				uint8_t signature[32];
				unsigned int out_len = sizeof(signature);
				MBinBuffer secret(getBlob(DBKEY_SECRET_KEY));
				HMAC(EVP_sha256(), secret.data(), (int)secret.length(), details.data, (int)details.len, signature, &out_len);
				if (memcmp(hmac.data, signature, sizeof(signature)))
					throw "OnIqPairSuccess: got reply with invalid details signature, exiting";
			}

			proto::ADVSignedDeviceIdentity account(details);

			auto &deviceDetails = account->details;
			auto &accountSignature = account->accountsignature;
			auto &accountSignatureKey = account->accountsignaturekey;
			{
				MBinBuffer buf;
				buf.append("\x06\x00", 2);
				buf.append(deviceDetails.data, deviceDetails.len);
				buf.append(m_signalStore.signedIdentity.pub);

				ec_public_key key = {};
				memcpy(key.data, accountSignatureKey.data, sizeof(key.data));
				if (1 != curve_verify_signature(&key, buf.data(), buf.length(), accountSignature.data, accountSignature.len))
					throw "OnIqPairSuccess: got reply with invalid account signature, exiting";
			}
			debugLogA("Received valid account signature");
			{
				MBinBuffer buf;
				buf.append("\x06\x01", 2);
				buf.append(deviceDetails.data, deviceDetails.len);
				buf.append(m_signalStore.signedIdentity.pub);
				buf.append(accountSignatureKey.data, accountSignatureKey.len);

				signal_buffer *result;
				ec_private_key key = {};
				memcpy(key.data, m_signalStore.signedIdentity.priv.data(), m_signalStore.signedIdentity.priv.length());
				if (curve_calculate_signature(m_signalStore.CTX(), &result, &key, buf.data(), buf.length()) != 0)
					throw "OnIqPairSuccess: cannot calculate account signature, exiting";

				account->devicesignature = proto::SetBinary(result->data, result->len);
				account->has_devicesignature = true;
				signal_buffer_free(result);
			}

			{
				MBinBuffer key;
				if (accountSignatureKey.len == 32)
					key.append(KEY_BUNDLE_TYPE, 1);
				key.append(accountSignatureKey.data, accountSignatureKey.len);
				db_set_blob(0, m_szModuleName, "SignalIdentifierKey", key.data(), (int)key.length());
			}

			proto::CleanBinary(account->accountsignaturekey); account->has_accountsignaturekey = false;
			MBinBuffer accountEnc(proto::Serialize(account));
			db_set_blob(0, m_szModuleName, "WAAccount", accountEnc.data(), (int)accountEnc.length());

			proto::ADVDeviceIdentity deviceIdentity(deviceDetails);

			WANodeIq reply(IQ::RESULT); reply << CHAR_PARAM("id", node.getAttr("id"));

			WANode *nodePair = reply.addChild("pair-device-sign");

			WANode *nodeDeviceIdentity = nodePair->addChild("device-identity");
			nodeDeviceIdentity->addAttr("key-index", deviceIdentity->keyindex);
			nodeDeviceIdentity->content.append(accountEnc.data(), accountEnc.length());
			WSSendNode(reply);
		}
		else throw "OnIqPairSuccess: got reply without identity, exiting";
	}
	catch (const char *pErrMsg) {
		debugLogA(pErrMsg);
		ShutdownSession();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnIqResult(const WANode &node)
{
	if (auto *pszId = node.getAttr("id")) {
		for (auto &it : m_arPacketQueue) {
			if (it->szPacketId == pszId) {
				it->Execute(this, node);
				m_arPacketQueue.remove(it);
				break;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnNotifyAny(const WANode &node)
{
	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnReceiveChatState(const WANode &node)
{
	if (auto *pUser = FindUser(node.getAttr("from"))) {
		if (node.getChild("composing")) {
			pUser->m_timer1 = time(0);
			pUser->m_timer2 = 0;
			setWord(pUser->hContact, "Status", ID_STATUS_ONLINE);

			CallService(MS_PROTO_CONTACTISTYPING, pUser->hContact, 60);
		}
		else if (node.getChild("paused"))
			CallService(MS_PROTO_CONTACTISTYPING, pUser->hContact, PROTOTYPE_CONTACTTYPING_OFF);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnNotifyDevices(const WANode &node)
{
	if (!mir_strcmp(node.getAttr("jid"), m_szJid))
		debugLogA("received list of my own devices");
	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnNotifyEncrypt(const WANode &node)
{
	if (!mir_strcmp(node.getAttr("from"), S_WHATSAPP_NET))
		OnIqCountPrekeys(node);
	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnNotifyPicture(const WANode &node)
{
	if (auto *pszFrom = node.getAttr("from"))
		if (m_szJid != pszFrom)
			if (auto *pszUser = FindUser(pszFrom))
				ServerFetchAvatar(pszFrom);

	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnProcessHandshake(const uint8_t *pData, int cbLen)
{
	proto::HandshakeMessage msg(pData, cbLen);
	if (!msg) {
		debugLogA("Error parsing data, exiting");

LBL_Error:
		ShutdownSession();
		return;
	}

	auto &static_ = msg->serverhello->static_;
	auto &payload_ = msg->serverhello->payload;
	auto &ephemeral_ = msg->serverhello->ephemeral;

	m_noise->updateHash(ephemeral_.data, ephemeral_.len);
	m_noise->mixIntoKey(m_noise->ephemeral.priv.data(), ephemeral_.data);

	MBinBuffer decryptedStatic = m_noise->decrypt(static_.data, static_.len);
	m_noise->mixIntoKey(m_noise->ephemeral.priv.data(), decryptedStatic.data());

	proto::CertChain cert(m_noise->decrypt(payload_.data, payload_.len));
	proto::CertChain__NoiseCertificate__Details details(cert->intermediate->details);
	if (details->issuerserial != 0) {
		debugLogA("Invalid certificate serial number, exiting");
		goto LBL_Error;
	}

	MBinBuffer encryptedPub = m_noise->encrypt(m_noise->noiseKeys.pub.data(), m_noise->noiseKeys.pub.length());
	m_noise->mixIntoKey(m_noise->noiseKeys.priv.data(), ephemeral_.data);

	// create reply
	Wa__ClientPayload node;
	Wa__ClientPayload__DevicePairingRegistrationData pairingData;
	Wa__DeviceProps companion;
	Wa__DeviceProps__AppVersion appVersion;
	T2Utf devName(m_wszDeviceName);

	// not received our jid from server? generate registration packet then
	if (m_szJid.IsEmpty()) {
		uint8_t buildHash[16];
		mir_md5_hash((BYTE *)WA_APPVERSION_STRING, sizeof(WA_APPVERSION_STRING) - 1, buildHash);

		appVersion.primary   = WA_PROTO_MAJOR; appVersion.has_primary = true;
		appVersion.secondary = WA_PROTO_MINOR; appVersion.has_secondary = true;
		appVersion.tertiary  = WA_PROTO_BUILD; appVersion.has_tertiary = true;

		companion.os = devName.get();
		companion.version = &appVersion;
		companion.platformtype = WA__DEVICE_PROPS__PLATFORM_TYPE__DESKTOP; companion.has_platformtype = true;
		companion.requirefullsync = false; companion.has_requirefullsync = true;

		MBinBuffer buf(proto::Serialize(&companion));
		auto szRegId(encodeBigEndian(getDword(DBKEY_REG_ID)));
		auto szKeyId(encodeBigEndian(m_signalStore.preKey.keyid));

		pairingData.deviceprops = proto::SetBinary(buf.data(), buf.length()); pairingData.has_deviceprops = true;
		pairingData.buildhash = proto::SetBinary(buildHash, sizeof(buildHash)); pairingData.has_buildhash = true;
		pairingData.eregid = proto::SetBinary(szRegId.c_str(), szRegId.size()); pairingData.has_eregid = true;
		pairingData.ekeytype = proto::SetBinary(KEY_BUNDLE_TYPE, 1); pairingData.has_ekeytype = true;
		pairingData.eident = proto::SetBinary(m_signalStore.signedIdentity.pub.data(), m_signalStore.signedIdentity.pub.length()); pairingData.has_eident = true;
		pairingData.eskeyid = proto::SetBinary(szKeyId.c_str(), szKeyId.size()); pairingData.has_eskeyid = true;
		pairingData.eskeyval = proto::SetBinary(m_signalStore.preKey.pub.data(), m_signalStore.preKey.pub.length()); pairingData.has_eskeyval = true;
		pairingData.eskeysig = proto::SetBinary(m_signalStore.preKey.signature.data(), m_signalStore.preKey.signature.length()); pairingData.has_eskeysig = true;
		node.devicepairingdata = &pairingData;

		node.passive = false; node.has_passive = true;
	}
	// generate login packet
	else {
		WAJid jid(m_szJid);
		node.username = _atoi64(jid.user);       node.has_username = true;
		node.device = getDword(DBKEY_DEVICE_ID); node.has_device = true;
		node.passive = true;                     node.has_passive = true;
	}

	Wa__ClientPayload__UserAgent__AppVersion userVersion;
	userVersion.primary = WA_PROTO_MAJOR; userVersion.has_primary = true;
	userVersion.secondary = WA_PROTO_MINOR; userVersion.has_secondary = true;
	userVersion.tertiary = WA_PROTO_BUILD; userVersion.has_tertiary = true;

	Wa__ClientPayload__UserAgent userAgent;
	userAgent.appversion = &userVersion;
	userAgent.platform = WA__CLIENT_PAYLOAD__USER_AGENT__PLATFORM__WEB; userAgent.has_platform = true;
	userAgent.releasechannel = WA__CLIENT_PAYLOAD__USER_AGENT__RELEASE_CHANNEL__RELEASE; userAgent.has_releasechannel = true;
	userAgent.mcc = "000";
	userAgent.mnc = "000";
	userAgent.osversion = "0.1";
	userAgent.osbuildnumber = "0.1";
	userAgent.manufacturer = "";
	userAgent.device = "Desktop";
	userAgent.localelanguageiso6391 = "en";
	userAgent.localecountryiso31661alpha2 = "US";

	Wa__ClientPayload__WebInfo webInfo;
	webInfo.websubplatform = WA__CLIENT_PAYLOAD__WEB_INFO__WEB_SUB_PLATFORM__WEB_BROWSER; webInfo.has_websubplatform = true;

	node.connecttype = WA__CLIENT_PAYLOAD__CONNECT_TYPE__WIFI_UNKNOWN; node.has_connecttype = true;
	node.connectreason = WA__CLIENT_PAYLOAD__CONNECT_REASON__USER_ACTIVATED; node.has_connectreason = true;
	node.useragent = &userAgent;
	node.webinfo = &webInfo;

	MBinBuffer payload(proto::Serialize(&node));
	MBinBuffer payloadEnc = m_noise->encrypt(payload.data(), payload.length());

	Wa__HandshakeMessage__ClientFinish finish;
	finish.payload = {payloadEnc.length(), payloadEnc.data()}; finish.has_payload = true;
	finish.static_ = {encryptedPub.length(), encryptedPub.data()}; finish.has_static_ = true;

	Wa__HandshakeMessage handshake;
	handshake.clientfinish = &finish;
	WSSend(handshake);

	m_noise->finish();
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnReceiveFailure(const WANode &node)
{
	m_bTerminated = true;

	ProcessFailure(node.getAttrInt("reason"));
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnReceiveInfo(const WANode &node)
{
	if (auto *pChild = node.getFirstChild()) {
		if (pChild->title == "offline") {
			debugLogA("Processed %d offline events", pChild->getAttrInt("count"));

			// retrieve loaded prekeys count
			if (!m_bUpdatedPrekeys)
				WSSendNode(WANodeIq(IQ::GET, "encrypt") << XCHILD("count"), &WhatsAppProto::OnIqCountPrekeys);

			auto *pUser = FindUser(m_szJid);
			if (pUser->arDevices.getCount() == 0) {
				LIST<char> jids(1);
				jids.insert(m_szJid.GetBuffer());
				SendUsync(jids, nullptr);
			}

			for (auto &it : m_arCollections) {
				if (it->version == 0) {
					m_impl.m_resyncApp.Stop();
					m_impl.m_resyncApp.Start(1000);
					break;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnReceiveCall(const WANode &node)
{
	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::ProcessReceipt(MCONTACT hContact, const char *msgId, bool bRead)
{
	MEVENT hEvent = db_event_getById(m_szModuleName, msgId);
	if (hEvent == 0)
		return;

	if (bRead)
		db_event_markRead(hContact, hEvent, true);
}

void WhatsAppProto::OnReceiveReceipt(const WANode &node)
{
	if (!mir_strcmp(node.getAttr("type"), "retry")) {
		for (auto &it : node.getChildren())
			if (it->title == "keys")
				m_signalStore.injectSession(node.getAttr("from"), &node, it);
		return;
	}

	if (auto *pUser = FindUser(node.getAttr("from"))) {
		bool bRead = mir_strcmp(node.getAttr("type"), "read") == 0;
		ProcessReceipt(pUser->hContact, node.getAttr("id"), bRead);

		if (auto *pList = node.getChild("list"))
			for (auto &it : pList->getChildren())
				if (it->title == "item")
					ProcessReceipt(pUser->hContact, it->getAttr("id"), bRead);
	}

	SendAck(node);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnStreamError(const WANode &node)
{
	m_bTerminated = true;

	if (auto *pszCode = node.getAttr("code"))
		ProcessFailure(atoi(pszCode));
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::OnSuccess(const WANode &)
{
	OnLoggedIn();

	WSSendNode(WANodeIq(IQ::SET, "passive") << XCHILD("active"), &WhatsAppProto::OnIqDoNothing);
}

/////////////////////////////////////////////////////////////////////////////////////////

void WhatsAppProto::InitPersistentHandlers()
{
	m_arPersistent.insert(new WAPersistentHandler("iq", "set", "md", "pair-device", &WhatsAppProto::OnIqPairDevice));
	m_arPersistent.insert(new WAPersistentHandler("iq", "set", "md", "pair-success", &WhatsAppProto::OnIqPairSuccess));

	m_arPersistent.insert(new WAPersistentHandler("notification", "devices", 0, 0, &WhatsAppProto::OnNotifyDevices));
	m_arPersistent.insert(new WAPersistentHandler("notification", "encrypt", 0, 0, &WhatsAppProto::OnNotifyEncrypt));
	m_arPersistent.insert(new WAPersistentHandler("notification", "picture", 0, 0, &WhatsAppProto::OnNotifyPicture));
	m_arPersistent.insert(new WAPersistentHandler("notification", "account_sync", 0, 0, &WhatsAppProto::OnAccountSync));
	m_arPersistent.insert(new WAPersistentHandler("notification", "server_sync", 0, 0, &WhatsAppProto::OnServerSync));
	m_arPersistent.insert(new WAPersistentHandler("notification", 0, 0, 0, &WhatsAppProto::OnNotifyAny));

	m_arPersistent.insert(new WAPersistentHandler("ack", 0, 0, 0, &WhatsAppProto::OnReceiveAck));
	m_arPersistent.insert(new WAPersistentHandler("ib", 0, 0, 0, &WhatsAppProto::OnReceiveInfo));
	m_arPersistent.insert(new WAPersistentHandler("call", 0, 0, 0, &WhatsAppProto::OnReceiveCall));
	m_arPersistent.insert(new WAPersistentHandler("failure", 0, 0, 0, &WhatsAppProto::OnReceiveFailure));
	m_arPersistent.insert(new WAPersistentHandler("message", 0, 0, 0, &WhatsAppProto::OnReceiveMessage));
	m_arPersistent.insert(new WAPersistentHandler("receipt", 0, 0, 0, &WhatsAppProto::OnReceiveReceipt));
	m_arPersistent.insert(new WAPersistentHandler("chatstates", 0, 0, 0, &WhatsAppProto::OnReceiveChatState));
	m_arPersistent.insert(new WAPersistentHandler("stream:error", 0, 0, 0, &WhatsAppProto::OnStreamError));
	m_arPersistent.insert(new WAPersistentHandler("success", 0, 0, 0, &WhatsAppProto::OnSuccess));

	m_arPersistent.insert(new WAPersistentHandler(0, "result", 0, 0, &WhatsAppProto::OnIqResult));
}
