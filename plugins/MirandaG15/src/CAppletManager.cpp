#include "stdafx.h"
#include "CConfig.h"
#include "CAppletManager.h"

// specifies how long contact status notifications are ignored after signon
#define PROTOCOL_NOTIFY_DELAY 1500

//########################################################################
// functions
//########################################################################

//************************************************************************
// returns the AppletManager's instance
//************************************************************************
CAppletManager *CAppletManager::GetInstance()
{
	return (CAppletManager*)CLCDOutputManager::GetInstance();
}

//************************************************************************
// Constructor
//************************************************************************
CAppletManager::CAppletManager()
{
}

//************************************************************************
// Destructor
//************************************************************************
CAppletManager::~CAppletManager()
{

}

//************************************************************************
// Initializes the AppletManager
//************************************************************************
bool CAppletManager::Initialize(tstring strAppletName)
{
	if (!CLCDOutputManager::Initialize(strAppletName))
		return false;

	GetLCDConnection()->Connect(CConfig::GetIntSetting(DEVICE));

	// set the volumewheel hook
	SetVolumeWheelHook();

	// initialize the screens
	m_NotificationScreen.Initialize();
	m_EventScreen.Initialize();
	m_ContactlistScreen.Initialize();
	m_ChatScreen.Initialize();
	m_CreditsScreen.Initialize();
	m_ScreensaverScreen.Initialize();

	// add the screens to the list
	AddScreen(&m_NotificationScreen);
	AddScreen(&m_EventScreen);
	AddScreen(&m_ContactlistScreen);
	AddScreen(&m_ChatScreen);
	AddScreen(&m_CreditsScreen);
	AddScreen(&m_ScreensaverScreen);

	// activate the event screen
	ActivateScreen(&m_EventScreen);

	// hook the neccessary events
	m_hMIHookMessageWindowEvent = HookEvent(ME_MSG_WINDOWEVENT, CAppletManager::HookMessageWindowEvent);
	m_hMIHookEventAdded = HookEvent(ME_DB_EVENT_ADDED, CAppletManager::HookEventAdded);
	m_hMIHookStatusChanged = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, CAppletManager::HookStatusChanged);
	m_hMIHookProtoAck = HookEvent(ME_PROTO_ACK, CAppletManager::HookProtoAck);
	m_hMIHookContactDeleted = HookEvent(ME_DB_CONTACT_DELETED, CAppletManager::HookContactDeleted);
	m_hMIHookContactAdded = HookEvent(ME_DB_CONTACT_ADDED, CAppletManager::HookContactAdded);
	m_hMIHookSettingChanged = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, CAppletManager::HookSettingChanged);
	m_hMIHookContactIsTyping = HookEvent(ME_PROTO_CONTACTISTYPING, CAppletManager::HookContactIsTyping);
	m_hMIHookChatEvent = HookEvent(ME_GC_HOOK_EVENT, CAppletManager::HookChatInbound);

	// enumerate protocols
	int iProtoCount = 0;
	CProtocolData *pProtoData = nullptr;
	CIRCConnection *pIRCConnection = nullptr;

	for (auto &pa : Accounts()) {
		if (pa->bIsEnabled == 0)
			continue;

		iProtoCount++;
		pProtoData = new CProtocolData();
		pProtoData->iStatus = ID_STATUS_OFFLINE;
		pProtoData->strProtocol = toTstring(pa->szModuleName);
		pProtoData->lTimeStamp = 0;

		// try to create an irc connection for that protocol (will fail if it is no irc protocol)
		pIRCConnection = CreateIRCConnection(pProtoData->strProtocol);

		m_vProtocolData.push_back(pProtoData);
	}

	// load status bitmaps
	m_ahStatusBitmaps[0] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_OFFLINE), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[1] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_ONLINE), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[2] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_AWAY), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[3] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_NA), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[4] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_OCCUPIED), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[5] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_DND), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[6] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_INVISIBLE), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	m_ahStatusBitmaps[7] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_STATUS_FFC), IMAGE_BITMAP, 5, 5, LR_MONOCHROME);
	// Load event bitmaps
	m_ahEventBitmaps[0] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_MSG), IMAGE_BITMAP, 6, 6, LR_MONOCHROME);
	m_ahEventBitmaps[1] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_CON), IMAGE_BITMAP, 6, 6, LR_MONOCHROME);
	m_ahEventBitmaps[2] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_USER), IMAGE_BITMAP, 6, 6, LR_MONOCHROME);
	m_ahEventBitmaps[3] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_INFO), IMAGE_BITMAP, 6, 6, LR_MONOCHROME);

	m_ahLargeEventBitmaps[0] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_MSG_LARGE), IMAGE_BITMAP, 8, 8, LR_MONOCHROME);
	m_ahLargeEventBitmaps[1] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_CON_LARGE), IMAGE_BITMAP, 8, 8, LR_MONOCHROME);
	m_ahLargeEventBitmaps[2] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_USER_LARGE), IMAGE_BITMAP, 8, 8, LR_MONOCHROME);
	m_ahLargeEventBitmaps[3] = (HBITMAP)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(IDB_EVENT_INFO_LARGE), IMAGE_BITMAP, 8, 8, LR_MONOCHROME);

	// start the update timer
	m_uiTimer = SetTimer(nullptr, 0, 1000 / 10, CAppletManager::UpdateTimer);

	return true;
}

//************************************************************************
// Deinitializes the AppletManager
//************************************************************************
bool CAppletManager::Shutdown()
{
	if (!IsInitialized())
		return false;

	// stop the update timer
	KillTimer(nullptr, m_uiTimer);

	// delete status bitmaps
	for (int i = 0; i < 8; i++)
		DeleteObject(m_ahStatusBitmaps[i]);

	// delete event bitmaps
	for (int i = 0; i < 4; i++) {
		DeleteObject(m_ahLargeEventBitmaps[i]);
		DeleteObject(m_ahEventBitmaps[i]);
	}

	// unhook the events	
	UnhookEvent(m_hMIHookMessageWindowEvent);
	UnhookEvent(m_hMIHookEventAdded);
	UnhookEvent(m_hMIHookStatusChanged);
	UnhookEvent(m_hMIHookProtoAck);
	UnhookEvent(m_hMIHookContactDeleted);
	UnhookEvent(m_hMIHookContactAdded);
	UnhookEvent(m_hMIHookSettingChanged);
	UnhookEvent(m_hMIHookChatEvent);

	// unhook all irc protocols, and delete the classes
	vector<CIRCConnection*>::iterator iter = m_vIRCConnections.begin();
	while (iter != m_vIRCConnections.end()) {
		delete *iter;
		iter++;
	}
	m_vIRCConnections.clear();

	// Deinitialize the screens
	m_NotificationScreen.Shutdown();
	m_EventScreen.Shutdown();
	m_ContactlistScreen.Shutdown();
	m_ChatScreen.Shutdown();
	m_CreditsScreen.Shutdown();
	m_ScreensaverScreen.Shutdown();

	// deinitialize the configuration manager
	CConfig::Shutdown();

	// delete the protocol information
	CProtocolData *pProtoData;
	for (vector<CProtocolData*>::size_type i = 0; i < m_vProtocolData.size(); i++) {
		pProtoData = m_vProtocolData[i];
		delete pProtoData;
	}
	m_vProtocolData.clear();

	// deinitialize the outputmanager
	if (!CLCDOutputManager::Shutdown())
		return false;
	return true;
}

//************************************************************************
// Translates the specified string, and inserts the parameters
//************************************************************************
tstring CAppletManager::TranslateString(wchar_t *szString, ...)
{
	wchar_t out[1024];
	wchar_t *szTranslatedString = TranslateW(szString);

	va_list body;
	va_start(body, szString);
	vswprintf_s(out, _countof(out), szTranslatedString, body);
	va_end(body);
	return out;
}

//************************************************************************
// checks if the patched IRC protocol is in place
//************************************************************************
bool CAppletManager::IsIRCHookEnabled()
{
	if (m_vIRCConnections.size() == NULL)
		return false;
	return true;
}

//************************************************************************
// returns the informations structure for the specified protocol
//************************************************************************
CProtocolData* CAppletManager::GetProtocolData(tstring strProtocol)
{
	for (vector<CProtocolData*>::size_type i = 0; i < m_vProtocolData.size(); i++) {
		if (m_vProtocolData[i]->strProtocol == strProtocol)
			return m_vProtocolData[i];
	}
	return nullptr;
}

//************************************************************************
// Updates the AppletManager
//************************************************************************
bool CAppletManager::Update()
{
	if (!CLCDOutputManager::Update())
		return false;

	// Update Messagejobs
	UpdateMessageJobs();

	// Screensaver detection
	BOOL bActive = false;
	SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &bActive, 0);
	if (bActive != (BOOL)m_bScreensaver) {
		if (CConfig::GetBoolSetting(SCREENSAVER_LOCK)) {
			if (!m_bScreensaver)
				ActivateScreensaverScreen();
			else
				ActivateEventScreen();
		}
		if (CConfig::GetBoolSetting(CONTROL_BACKLIGHTS)) {
			if (GetLCDConnection() &&
				GetLCDConnection()->GetConnectionType() == TYPE_LOGITECH) {
				CLCDConnectionLogitech *pLCDConnection = (CLCDConnectionLogitech*)GetLCDConnection();

				// Screensaver starts
				if (!m_bScreensaver) {
					m_G15LightStatus = pLCDConnection->GetLightStatus();
					pLCDConnection->SetLCDBacklight(LCD_OFF);
					pLCDConnection->SetKBDBacklight(KBD_OFF);
					pLCDConnection->SetMKeyLight(0, 0, 0, 0);
				}
				// Screensaver ends
				else {
					SG15LightStatus currentStatus = pLCDConnection->GetLightStatus();

					if (currentStatus.eLCDBrightness == LCD_OFF)
						pLCDConnection->SetLCDBacklight(m_G15LightStatus.eLCDBrightness);
					if (currentStatus.eKBDBrightness == KBD_OFF)
						pLCDConnection->SetKBDBacklight(m_G15LightStatus.eKBDBrightness);
					if (!currentStatus.bMRKey && !currentStatus.bMKey[0] && !currentStatus.bMKey[1]
						&& !currentStatus.bMKey[2])
						pLCDConnection->SetMKeyLight(m_G15LightStatus.bMKey[0], m_G15LightStatus.bMKey[1], m_G15LightStatus.bMKey[2], m_G15LightStatus.bMRKey);
				}
			}
		}
		m_bScreensaver = bActive != 0;
	}

	return true;
}

//************************************************************************
// Called when the active screen has expired
//************************************************************************
void CAppletManager::OnScreenExpired(CLCDScreen *pScreen)
{
	// If the notification screen has expired, activate the last active screen
	if (pScreen == (CLCDScreen*)&m_NotificationScreen) {
		ActivateScreen(m_pLastScreen);
		if (CConfig::GetBoolSetting(TRANSITIONS))
			m_pGfx->StartTransition();
	}
}

//************************************************************************
// the update timer's callback function
//************************************************************************
VOID CALLBACK CAppletManager::UpdateTimer(HWND, UINT, UINT_PTR, DWORD)
{
	CAppletManager::GetInstance()->Update();
}

//************************************************************************
// applies the volumewheel setting
//************************************************************************
void CAppletManager::SetVolumeWheelHook()
{
	// Set the volumewheel hook
	if (GetLCDConnection() && GetLCDConnection()->GetConnectionType() == TYPE_LOGITECH) {
		CLCDConnectionLogitech *pLCDConnection = (CLCDConnectionLogitech*)GetLCDConnection();
		if (pLCDConnection->GetConnectionState() == CONNECTED)
			pLCDConnection->SetVolumeWheelHook(CConfig::GetBoolSetting(HOOK_VOLUMEWHEEL));
	}
}

//************************************************************************
// Called when the connection state has changed
//************************************************************************
void CAppletManager::OnConnectionChanged(int iConnectionState)
{
	if (iConnectionState == CONNECTED) {
		SetVolumeWheelHook();
	}
	CConfig::OnConnectionChanged();
}

//************************************************************************
// called when the plugin's configuration has changed
//************************************************************************
void CAppletManager::OnConfigChanged()
{
	GetLCDConnection()->Connect(CConfig::GetIntSetting(DEVICE));

	m_pGfx->StartTransition(TRANSITION_MORPH);

	// Set the volumewheel hook
	SetVolumeWheelHook();
	// send the event to all screens
	m_NotificationScreen.OnConfigChanged();
	m_ChatScreen.OnConfigChanged();
	m_EventScreen.OnConfigChanged();
	m_ContactlistScreen.OnConfigChanged();
	m_CreditsScreen.OnConfigChanged();
}
//************************************************************************
// activate a screen
//************************************************************************
void CAppletManager::ActivateScreen(CScreen *pScreen)
{
	if (GetActiveScreen() && GetActiveScreen() != &m_NotificationScreen) {
		m_pLastScreen = (CScreen*)GetActiveScreen();
	}

	CLCDOutputManager::ActivateScreen(pScreen);
}

//************************************************************************
// activates the previous screen
//************************************************************************
void CAppletManager::ActivatePreviousScreen()
{
	if (m_pLastScreen) {
		ActivateScreen(m_pLastScreen);
	}
}

//************************************************************************
// activates the credits screen
//************************************************************************
void CAppletManager::ActivateScreensaverScreen()
{
	m_ScreensaverScreen.Reset();
	ActivateScreen(&m_ScreensaverScreen);
}

//************************************************************************
// activates the credits screen
//************************************************************************
void CAppletManager::ActivateCreditsScreen()
{
	m_CreditsScreen.Reset();
	ActivateScreen(&m_CreditsScreen);
}

//************************************************************************
// activates the event screen
//************************************************************************
void CAppletManager::ActivateEventScreen()
{
	m_ChatScreen.SetContact(NULL);
	ActivateScreen(&m_EventScreen);

	if (CConfig::GetBoolSetting(TRANSITIONS))
		m_pGfx->StartTransition();
}

//************************************************************************
// activates the contactlist screen
//************************************************************************
void CAppletManager::ActivateCListScreen()
{
	m_ChatScreen.SetContact(NULL);
	m_ContactlistScreen.ResetPosition();
	ActivateScreen(&m_ContactlistScreen);

	if (CConfig::GetBoolSetting(TRANSITIONS))
		m_pGfx->StartTransition();
}

//************************************************************************
// activates the chat screen
//************************************************************************
bool CAppletManager::ActivateChatScreen(MCONTACT hContact)
{
	if (!m_ChatScreen.SetContact(hContact))
		return false;

	m_ContactlistScreen.OnSessionOpened(hContact);
	ActivateScreen(&m_ChatScreen);

	if (CConfig::GetBoolSetting(TRANSITIONS))
		m_pGfx->StartTransition();
	return true;
}

//************************************************************************
// returns the contacts displayname
//************************************************************************
tstring CAppletManager::GetContactDisplayname(MCONTACT hContact, bool bShortened)
{
	if (!bShortened || !CConfig::GetBoolSetting(NOTIFY_NICKCUTOFF))
		return Clist_GetContactDisplayName(hContact);

	tstring strNick = GetContactDisplayname(hContact, false);
	if (strNick.length() > (tstring::size_type)CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET))
		return strNick.erase(CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET)) + L"...";

	return strNick;
}

//************************************************************************
// returns the contacts group
//************************************************************************
tstring CAppletManager::GetContactGroup(MCONTACT hContact)
{
	ptrW wszGroup(Clist_GetGroup(hContact));
	return (wszGroup) ? wszGroup : L"";
}

//************************************************************************
// returns the bitmap for the specified event
//************************************************************************
HBITMAP CAppletManager::GetEventBitmap(EventType eType, bool bLarge)
{
	switch (eType) {
	case EVENT_MSG_RECEIVED:
	case EVENT_MSG_SENT:
	case EVENT_IRC_RECEIVED:
	case EVENT_IRC_SENT:
		if (bLarge)
			return m_ahLargeEventBitmaps[0];
		else
			return m_ahEventBitmaps[0];
	case EVENT_PROTO_STATUS:
	case EVENT_PROTO_CONNECTED:
	case EVENT_PROTO_DISCONNECTED:
		if (bLarge)
			return m_ahLargeEventBitmaps[1];
		else
			return m_ahEventBitmaps[1];
	case EVENT_STATUS:
	case EVENT_SIGNED_ON:
	case EVENT_SIGNED_OFF:
		if (bLarge)
			return m_ahLargeEventBitmaps[2];
		else
			return m_ahEventBitmaps[2];
	default:
		if (bLarge)
			return m_ahLargeEventBitmaps[3];
		else
			return m_ahEventBitmaps[3];
	}
}

//************************************************************************
// returns the bitmap for the specified status
//************************************************************************
HBITMAP CAppletManager::GetStatusBitmap(int iStatus)
{
	switch (iStatus) {
	case ID_STATUS_OFFLINE:
		return m_ahStatusBitmaps[0];
	case ID_STATUS_ONLINE:
		return m_ahStatusBitmaps[1];
	case ID_STATUS_NA:
		return m_ahStatusBitmaps[3];
	case ID_STATUS_OCCUPIED:
		return m_ahStatusBitmaps[4];
	case ID_STATUS_DND:
		return m_ahStatusBitmaps[5];
	case ID_STATUS_INVISIBLE:
		return m_ahStatusBitmaps[6];
	case ID_STATUS_FREECHAT:
		return m_ahStatusBitmaps[7];
	case ID_STATUS_AWAY:
	default:
		return m_ahStatusBitmaps[2];
	}
}
//************************************************************************
// returns a formatted timestamp string
//************************************************************************
tstring CAppletManager::GetFormattedTimestamp(tm *tm_time)
{
	time_t now;
	tm tm_now;
	time(&now);
	localtime_s(&tm_now, &now);

	wchar_t buffer[128];

	if (tm_time->tm_mday != tm_now.tm_mday || tm_time->tm_mon != tm_now.tm_mon) {
		if (CConfig::GetBoolSetting(TIMESTAMP_SECONDS))
			wcsftime(buffer, 128, L"[%x %H:%M:%S]", tm_time);
		else
			wcsftime(buffer, 128, L"[%x %H:%M]", tm_time);
	}
	else {
		if (CConfig::GetBoolSetting(TIMESTAMP_SECONDS))
			wcsftime(buffer, 128, L"[%H:%M:%S]", tm_time);
		else
			wcsftime(buffer, 128, L"[%H:%M]", tm_time);
	}

	return toTstring(buffer);
}

//************************************************************************
// called to process the specified event
//************************************************************************
void CAppletManager::HandleEvent(CEvent *pEvent)
{
	TRACE(L"<< Event: %i\n", (int)pEvent->eType);

	// check if the event's timestamp needs to be set
	if (!pEvent->bTime) {
		time_t now;
		time(&now);
		localtime_s(&pEvent->Time, &now);
	}
	// check wether the event needs notification

	// check for protocol filters
	if (pEvent->hContact != NULL && pEvent->eType != EVENT_CONTACT_ADDED) {
		char *szProto = Proto_GetBaseAccountName(pEvent->hContact);
		if (szProto == nullptr || !CConfig::GetProtocolNotificationFilter(toTstring(szProto)))
			pEvent->bNotification = false;
	}
	pEvent->bLog = pEvent->bNotification;

	if (db_mc_isSub(pEvent->hContact)) {
		pEvent->bLog = false;
		pEvent->bNotification = false;
	}

	// if the applet is in foreground, skip notifications for the chatsession contact
	if (pEvent->hContact && GetLCDConnection()->IsForeground() && pEvent->hContact == m_ChatScreen.GetContact() &&
		(!m_ChatScreen.IsInputActive() || !CConfig::GetBoolSetting(NOTIFY_NO_SKIP_REPLY))) {
		if (pEvent->eType == EVENT_STATUS && CConfig::GetBoolSetting(NOTIFY_SKIP_STATUS))
			pEvent->bNotification = false;
		if (pEvent->eType == EVENT_SIGNED_ON && CConfig::GetBoolSetting(NOTIFY_SKIP_SIGNON))
			pEvent->bNotification = false;
		if (pEvent->eType == EVENT_SIGNED_OFF && CConfig::GetBoolSetting(NOTIFY_SKIP_SIGNOFF))
			pEvent->bNotification = false;
		if ((pEvent->eType == EVENT_IRC_RECEIVED || pEvent->eType == EVENT_MSG_RECEIVED) && CConfig::GetBoolSetting(NOTIFY_SKIP_MESSAGES))
			pEvent->bNotification = false;
	}

	// send the event to all screens
	m_NotificationScreen.OnEventReceived(pEvent);
	m_ChatScreen.OnEventReceived(pEvent);
	m_EventScreen.OnEventReceived(pEvent);
	m_ContactlistScreen.OnEventReceived(pEvent);

	// activate notification screen if neccessary (and screensaverscreen is not active)
	if (pEvent->bNotification) {
		if (GetActiveScreen() != (CLCDScreen*)&m_NotificationScreen && GetActiveScreen() != (CLCDScreen*)&m_ScreensaverScreen) {
			m_NotificationScreen.SetAlert(true);
			m_NotificationScreen.SetExpiration(CConfig::GetIntSetting(NOTIFY_DURATION) * 1000);
			ActivateScreen(&m_NotificationScreen);

			if (GetLCDConnection()->IsForeground() && CConfig::GetBoolSetting(TRANSITIONS))
				m_pGfx->StartTransition();
		}
	}
}

//************************************************************************
// updates all pending message jobs
//************************************************************************
void CAppletManager::UpdateMessageJobs()
{
	list<SMessageJob*>::iterator iter = m_MessageJobs.begin();
	while (iter != m_MessageJobs.end()) {
		// TODO: Fertigstellen
		if ((*iter)->dwTimestamp + 15 * 1000 < GetTickCount()) {
			CEvent Event;

			Event.eType = EVENT_MESSAGE_ACK;
			Event.hValue = (*iter)->hEvent;
			Event.hContact = (*iter)->hContact;
			Event.iValue = ACKRESULT_FAILED;
			Event.strValue = TranslateString(L"Timeout: No response from contact/server");

			HandleEvent(&Event);

			SMessageJob *pJob = *iter;
			m_MessageJobs.erase(iter);
			free(pJob->pcBuffer);
			delete(pJob);
			break;
		}
		iter++;
	}
}

//************************************************************************
// adds a message job to the list
//************************************************************************
void CAppletManager::AddMessageJob(SMessageJob *pJob)
{
	m_MessageJobs.push_back(pJob);
}

//************************************************************************
// finishes a message job
//************************************************************************
void CAppletManager::FinishMessageJob(SMessageJob *pJob)
{
	list<SMessageJob*>::iterator iter = m_MessageJobs.begin();
	while (iter != m_MessageJobs.end()) {
		if ((*iter) == pJob) {
			char *szProto = Proto_GetBaseAccountName(pJob->hContact);
			tstring strProto = toTstring(szProto);
			CIRCConnection *pIRCCon = GetIRCConnection(strProto);

			// Only add the message to the history if the contact isn't an irc chatroom
			if (!(pIRCCon && Contact::IsGroupChat(pJob->hContact, szProto))) {
				// Add the message to the database
				DBEVENTINFO dbei = {};
				dbei.eventType = EVENTTYPE_MESSAGE;
				dbei.flags = DBEF_SENT | DBEF_UTF;
				dbei.szModule = szProto;
				dbei.iTimestamp = time(0);
				// Check if protocoll is valid
				if (dbei.szModule == nullptr)
					return;

				dbei.cbBlob = pJob->iBufferSize;
				dbei.pBlob = pJob->pcBuffer;

				db_event_add(pJob->hContact, &dbei);
			}

			pJob = *iter;
			m_MessageJobs.erase(iter);
			free(pJob->pcBuffer);
			delete(pJob);
			return;
		}
	}
}

//************************************************************************
// cancels a message job
//************************************************************************
void CAppletManager::CancelMessageJob(SMessageJob *pJob)
{
	list<SMessageJob*>::iterator iter = m_MessageJobs.begin();
	while (iter != m_MessageJobs.end()) {
		if ((*iter) == pJob) {
			pJob = *iter;
			m_MessageJobs.erase(iter);
			free(pJob->pcBuffer);
			delete(pJob);
			return;
		}
	}
}

//************************************************************************
// sends typing notifications to the specified contact
//************************************************************************
void CAppletManager::SendTypingNotification(MCONTACT hContact, bool bEnable)
{
	if (!hContact)
		return;

	// Don't send to protocols who don't support typing
	// Don't send to users who are unchecked in the typing notification options
	// Don't send to protocols that are offline
	// Don't send to users who are not visible and
	// Don't send to users who are not on the visible list when you are in invisible mode.
	if (!db_get_b(hContact, "SRMsg", "SupportTyping", db_get_b(0, "SRMsg", "DefaultTyping", 1)))
		return;

	char *szProto = Proto_GetBaseAccountName(hContact);
	if (!szProto)
		return;

	uint32_t typeCaps = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_4, 0);
	if (!(typeCaps & PF4_SUPPORTTYPING))
		return;

	uint32_t protoStatus = Proto_GetStatus(szProto);
	if (protoStatus < ID_STATUS_ONLINE)
		return;

	if (!Contact::OnList(hContact) && !db_get_b(0, "SRMsg", "UnknownTyping", 1))
		return;
	// End user check
	CallService(MS_PROTO_SELFISTYPING, hContact, bEnable ? PROTOTYPE_SELFTYPING_ON : PROTOTYPE_SELFTYPING_OFF);
}

//************************************************************************
// sends a message to the specified contact
//************************************************************************
MEVENT CAppletManager::SendMessageToContact(MCONTACT hContact, tstring strMessage)
{
	tstring strAscii = _A2T(toNarrowString(strMessage).c_str());

	char *szProto = Proto_GetBaseAccountName(hContact);
	tstring strProto = toTstring(szProto);

	CIRCConnection *pIRCCon = CAppletManager::GetInstance()->GetIRCConnection(strProto);

	if (pIRCCon && Contact::IsGroupChat(hContact, szProto)) {
		ptrW wszNick(db_get_wsa(hContact, szProto, "Nick"));
		if (wszNick == NULL)
			return NULL;

		tstring strID = tstring(wszNick) + L" - " + tstring(_A2T(toNarrowString(pIRCCon->strNetwork).c_str()));
		Chat_SendUserMessage(Chat_Find(strID.c_str(), szProto), strAscii.c_str());
		return 0;
	}

	SMessageJob *pJob = new SMessageJob();
	pJob->dwTimestamp = GetTickCount();
	pJob->hContact = hContact;

	char* szMsgUtf = mir_utf8encodeW(strMessage.c_str());

	pJob->iBufferSize = (int)mir_strlen(szMsgUtf) + 1;
	pJob->pcBuffer = (char *)malloc(pJob->iBufferSize);
	pJob->dwFlags = 0;

	memcpy(pJob->pcBuffer, szMsgUtf, pJob->iBufferSize);
	mir_free(szMsgUtf);

	pJob->hEvent = (MEVENT)ProtoChainSend(pJob->hContact, PSS_MESSAGE, 0, (LPARAM)pJob->pcBuffer);
	CAppletManager::GetInstance()->AddMessageJob(pJob);
	return pJob->hEvent;
}

//************************************************************************
// check if a contacts message window is opened
//************************************************************************
bool CAppletManager::IsMessageWindowOpen(MCONTACT hContact)
{
	MessageWindowData mwd;
	Srmm_GetWindowData(hContact, mwd);
	if (mwd.uState & MSG_WINDOW_STATE_EXISTS)
		return true;
	return false;
}

//************************************************************************
// marks the given message as read
//************************************************************************
void CAppletManager::MarkMessageAsRead(MCONTACT hContact, MEVENT hEvent)
{
	db_event_markRead(hContact, hEvent);
	Clist_RemoveEvent(hContact, hEvent);
}

//************************************************************************
// translates the given database event
//************************************************************************
bool CAppletManager::TranslateDBEvent(CEvent *pEvent, WPARAM hContact, LPARAM hdbevent)
{
	// Create struct for event
	DB::EventInfo dbei(hdbevent);
	if (!dbei)
		return false;

	pEvent->dwFlags = dbei.flags;
	pEvent->hContact = hContact;
	pEvent->hValue = hdbevent;

	time_t timestamp = (time_t)dbei.getUnixtime();
	localtime_s(&pEvent->Time, &timestamp);
	pEvent->bTime = true;

	// Skip events from the user except for messages
	if (dbei.eventType != EVENTTYPE_MESSAGE && (dbei.flags & DBEF_SENT))
		return false;

	int msglen = 0;

	tstring strName = CAppletManager::GetContactDisplayname(hContact, true);

	switch (dbei.eventType) {
	case EVENTTYPE_MESSAGE:
		msglen = (int)mir_strlen((char *)dbei.pBlob) + 1;
		if (dbei.flags & DBEF_UTF) {
			pEvent->strValue = Utf8_Decode((char*)dbei.pBlob);
		}
		else if ((int)dbei.cbBlob == msglen * 3) {
			pEvent->strValue = (wchar_t *)& dbei.pBlob[msglen];
		}
		else {
			pEvent->strValue = toTstring((char*)dbei.pBlob);
		}
		pEvent->eType = (dbei.flags & DBEF_SENT) ? EVENT_MSG_SENT : EVENT_MSG_RECEIVED;
		if (pEvent->eType == EVENT_MSG_RECEIVED) {
			pEvent->dwFlags = MSG_UNREAD;
			if (CConfig::GetBoolSetting(NOTIFY_MESSAGES))
				pEvent->bNotification = true;
		}

		pEvent->strDescription = strName + L": " + pEvent->strValue;
		pEvent->strSummary = TranslateString(L"New message from %s", strName.c_str());
		break;
	case EVENTTYPE_CONTACTS:
		if (CConfig::GetBoolSetting(NOTIFY_CONTACTS))
			pEvent->bNotification = true;

		pEvent->strDescription = TranslateString(L"Incoming contacts from %s", strName.c_str());
		pEvent->eType = EVENT_CONTACTS;
		break;
	case EVENTTYPE_ADDED:
		if (CConfig::GetBoolSetting(NOTIFY_CONTACTS))
			pEvent->bNotification = true;

		pEvent->strDescription = TranslateString(L"You were added by %s", strName.c_str());
		pEvent->eType = EVENT_ADDED;
		break;
	case EVENTTYPE_AUTHREQUEST:
		if (CConfig::GetBoolSetting(NOTIFY_CONTACTS))
			pEvent->bNotification = true;

		pEvent->strDescription = TranslateString(L"Incoming Authrequest!");
		pEvent->eType = EVENT_AUTHREQUEST;
		break;
	case EVENTTYPE_FILE:
		if (CConfig::GetBoolSetting(NOTIFY_FILE))
			pEvent->bNotification = true;

		pEvent->strDescription = TranslateString(L"Incoming file from %s", strName.c_str());
		pEvent->eType = EVENT_FILE;
		break;

	default:
		return false;
	}

	if (CConfig::GetBoolSetting(NOTIFY_SHOWPROTO)) {
		char *szProto = Proto_GetBaseAccountName(pEvent->hContact);
		pEvent->strDescription = L"(" + toTstring(szProto) + L") " + pEvent->strDescription;
	}

	return true;
}

//************************************************************************
// removes IRC formatting tags from the string
//************************************************************************
tstring CAppletManager::StripIRCFormatting(tstring strText)
{
	tstring::size_type start = 0, i = 0;
	tstring strEntity = L"";
	tstring strReplace = L"";

	while (i < strText.length()) {
		start = strText.find(L"%", i);
		if (start != string::npos && start < strText.length() - 1) {
			strEntity = strText[start + 1];
			if (strEntity == L"%") {
				strText.replace(start, 2, L"%");
				i = start + 1;
			}
			/*
			else if(strEntity == L"b" || strEntity == L"B" ||
				strEntity == L"i" || strEntity == L"I" ||
				strEntity ==L"u" || strEntity == L"U" ||
				strEntity == L"C" ||strEntity == L"F")
			{
				strText.erase(start,2);
				i = start;
			}
			*/
			else if (strEntity == L"c" || strEntity == L"f") {
				strText.erase(start, 4);
				i = start;
			}
			else {
				strText.erase(start, 2);
				i = start;
			}
		}
		else
			break;
	}

	return strText;
}

//************************************************************************
// returns the IRC connection class for the specified protocol
//************************************************************************
CIRCConnection *CAppletManager::GetIRCConnection(tstring strProtocol)
{
	vector<CIRCConnection*>::iterator iter = m_vIRCConnections.begin();
	while (iter != m_vIRCConnections.end()) {
		if ((*iter)->strProtocol == strProtocol)
			return *iter;
		iter++;
	}
	return nullptr;
}

//************************************************************************
// creates the IRC connection class for the specified protocol
//************************************************************************
CIRCConnection *CAppletManager::CreateIRCConnection(tstring strProtocol)
{
	CIRCConnection *pIRCCon = new CIRCConnection();
	pIRCCon->strProtocol = strProtocol;
	pIRCCon->strNetwork = L"";

	m_vIRCConnections.push_back(pIRCCon);

	return pIRCCon;
}

//************************************************************************
// returns the history class for the specified IRC channel
//************************************************************************
CIRCHistory *CAppletManager::GetIRCHistory(MCONTACT hContact)
{
	list<CIRCHistory*>::iterator iter = m_LIRCHistorys.begin();
	while (iter != m_LIRCHistorys.end()) {
		if ((*iter)->hContact == hContact)
			return *iter;
		iter++;
	}
	return nullptr;
}

CIRCHistory *CAppletManager::GetIRCHistoryByName(tstring strProtocol, tstring strChannel)
{
	list<CIRCHistory*>::iterator iter = m_LIRCHistorys.begin();
	while (iter != m_LIRCHistorys.end()) {
		if ((*iter)->strChannel == strChannel && (*iter)->strProtocol == strProtocol)
			return *iter;
		iter++;
	}
	return nullptr;
}

//************************************************************************
// deletes the history class for the specified IRC channel
//************************************************************************
void CAppletManager::DeleteIRCHistory(MCONTACT hContact)
{
	list<CIRCHistory*>::iterator iter = m_LIRCHistorys.begin();
	while (iter != m_LIRCHistorys.end()) {
		if ((*iter)->hContact == hContact) {
			CIRCHistory *pHistory = *iter;
			pHistory->LMessages.clear();
			pHistory->LUsers.clear();

			m_LIRCHistorys.erase(iter);

			delete pHistory;

			return;
		}
		iter++;
	}
}

//************************************************************************
// creates a history class for the specified IRC channel
//************************************************************************
CIRCHistory *CAppletManager::CreateIRCHistory(MCONTACT hContact, tstring strChannel)
{
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (!szProto)
		return nullptr;

	CIRCHistory *pHistory = GetIRCHistoryByName(toTstring(szProto), strChannel);
	if (pHistory) {
		pHistory->hContact = hContact;
		return pHistory;
	}

	pHistory = new CIRCHistory();
	pHistory->hContact = hContact;
	pHistory->strChannel = strChannel;
	pHistory->strProtocol = toTstring(szProto);

	m_LIRCHistorys.push_back(pHistory);

	return pHistory;
}

CIRCHistory *CAppletManager::CreateIRCHistoryByName(tstring strProtocol, tstring strChannel)
{
	CIRCHistory *pHistory = GetIRCHistoryByName(strProtocol, strChannel);
	if (pHistory)
		return pHistory;

	pHistory = new CIRCHistory();
	pHistory->hContact = NULL;
	pHistory->strChannel = strChannel;
	pHistory->strProtocol = strProtocol;

	m_LIRCHistorys.push_back(pHistory);

	return pHistory;
}

//########################################################################
// hook functions
//########################################################################

//************************************************************************
// inbound chat event hook function
//************************************************************************
int CAppletManager::HookChatInbound(WPARAM, LPARAM lParam)
{
	GCEVENT *gce = (GCEVENT*)lParam;
	if (gce == nullptr) {
		TRACE(L"<< [%s] skipping invalid event\n");
		return 0;
	}

	TRACE(L"<< [%s:%s] event %04X\n", toTstring(gce->si->pszModule).c_str(), gce->si->ptszID, gce->iType);

	// get the matching irc connection entry
	CIRCConnection *pIRCCon = CAppletManager::GetInstance()->GetIRCConnection(toTstring(gce->si->pszModule));
	if (!pIRCCon) {
		TRACE(L"<< [%s] connection not found, skipping event\n", toTstring(gce->si->pszModule).c_str());
		return 0;
	}

	// fetch the network name
	// if (gcd->iType == GC_EVENT_CHANGESESSIONAME) {
	// 	if (gcd->ptszID && !mir_wstrcmpi(gcd->ptszID, L"Network log")) {
	// 		pIRCCon->strNetwork = toTstring(gce->ptszText);
	// 		TRACE(L"\t Found network identifier: %s\n", pIRCCon->strNetwork.c_str());
	// 		return 0;
	// 	}
	// }

	CEvent Event;
	if (gce->bIsMe)
		Event.eType = EVENT_IRC_SENT;
	else
		Event.eType = EVENT_IRC_RECEIVED;
	Event.iValue = gce->iType;
	Event.hValue = lParam;

	CIRCHistory *pHistory = nullptr;
	if (gce->si->ptszID) {
		tstring strChannel = toTstring(gce->si->ptszID);
		tstring::size_type pos = strChannel.find('-');
		if (pos != tstring::npos)
			strChannel = strChannel.substr(0, pos - 1);
		else {
			if (mir_wstrcmpi(gce->si->ptszID, L"Network log"))
				TRACE(L"\t WARNING: ignoring unknown event!\n");
			return 0;
		}
		pHistory = CAppletManager::GetInstance()->GetIRCHistoryByName(pIRCCon->strProtocol, strChannel);
		if (!pHistory) {
			if (gce->iType == GC_EVENT_JOIN) {
				pHistory = CAppletManager::GetInstance()->CreateIRCHistoryByName(pIRCCon->strProtocol, strChannel);
				if (pHistory)
					pHistory->LUsers.push_back(toTstring(gce->pszNick.w));
			}
			return 0;
		}
		Event.hContact = pHistory->hContact;
	}
	else if (gce->iType != GC_EVENT_INFORMATION) {
		TRACE(L"\t WARNING: ignoring unknown event!\n");
		return 0;
	}
	else
		Event.hContact = NULL;

	// Ignore events from hidden chatrooms, except for join events
	if (gce->si->ptszID != nullptr && Contact::IsHidden(Event.hContact)) {
		if (gce->iType == GC_EVENT_JOIN && pHistory)
			pHistory->LUsers.push_back(toTstring(gce->pszNick.w));

		TRACE(L"\t Chatroom is hidden, skipping event!\n");
		return 0;
	}

	tstring strText = StripIRCFormatting(toTstring(gce->pszText.w));
	tstring strNick = toTstring(gce->pszNick.w);
	tstring strStatus = toTstring(gce->pszStatus.w);

	if (CConfig::GetBoolSetting(NOTIFY_NICKCUTOFF) && strNick.length() > (tstring::size_type)CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET))
		strNick = strNick.erase(CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET)) + L"...";

	TRACE(L"\t Handling event...\t");

	switch (gce->iType) {
	case GC_EVENT_INFORMATION:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_CHANNEL))
			Event.bNotification = true;

		if (strText.find(L"CTCP") == 0)
			Event.strValue = L"--> " + strText;
		else
			Event.strValue = strText;

		break;
	case GC_EVENT_ACTION:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_EMOTES))
			Event.bNotification = true;
		Event.strValue = strNick + L" " + strText;
		break;
	case GC_EVENT_MESSAGE:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_MESSAGES))
			Event.bNotification = true;
		Event.strValue = strNick + L": " + strText;
		break;
	case GC_EVENT_JOIN:
		// Add the user to the list
		pHistory->LUsers.push_back(toTstring(gce->pszNick.w));

		if (CConfig::GetBoolSetting(NOTIFY_IRC_USERS))
			Event.bNotification = true;
		// Skip join event for user
		if (gce->bIsMe)
			return 0;
		Event.strValue = TranslateString(L"%s has joined the channel", strNick.c_str());

		break;
	case GC_EVENT_PART:
		{
			if (CConfig::GetBoolSetting(NOTIFY_IRC_USERS))
				Event.bNotification = true;
			tstring strFullNick = toTstring(gce->pszNick.w);
			Event.strValue = TranslateString(strText.empty() ? L"%s has left" : L"%s has left: %s", strNick.c_str(), strText.c_str());
			if (pHistory) {
				// Remove the user from the list
				list<tstring>::iterator iter = pHistory->LUsers.begin();
				while (iter != pHistory->LUsers.end()) {
					if ((*iter) == strFullNick) {
						pHistory->LUsers.erase(iter);
						break;
					}
					iter++;
				}
			}
			break;
		}
	case GC_EVENT_QUIT:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_USERS))
			Event.bNotification = true;
		Event.strValue = TranslateString(strText.empty() ? L"%s has disconnected" : L"%s has disconnected: %s", strNick.c_str(), strText.c_str());
		break;
	case GC_EVENT_KICK:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_USERS))
			Event.bNotification = true;
		Event.strValue = TranslateString(L"%s has kicked %s: %s", strStatus.c_str(), strNick.c_str(), strText.c_str());
		break;
	case GC_EVENT_NICK:
		{
			if (CConfig::GetBoolSetting(NOTIFY_IRC_USERS))
				Event.bNotification = true;
			tstring strFullNick = toTstring(gce->pszNick.w);

			if (CConfig::GetBoolSetting(NOTIFY_NICKCUTOFF) && strText.length() > (tstring::size_type)CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET))
				strText = strText.erase(CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET)) + L"...";

			Event.strValue = TranslateString(L"%s is now known as %s", strNick.c_str(), strText.c_str());
			if (pHistory) {
				// change the nick in the userlist
				list<tstring>::iterator iter = pHistory->LUsers.begin();
				while (iter != pHistory->LUsers.end()) {
					if ((*iter) == strFullNick)
						(*iter) = strText;
					iter++;
				}
			}
			break;
		}
	case GC_EVENT_NOTICE:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_NOTICES))
			Event.bNotification = true;
		Event.strValue = TranslateString(L"Notice from %s: %s", strNick.c_str(), strText.c_str());
		break;
	case GC_EVENT_TOPIC:
		if (CConfig::GetBoolSetting(NOTIFY_IRC_CHANNEL))
			Event.bNotification = true;
		Event.strValue = TranslateString(L"Topic is now '%s' (set by %s)", strText.c_str(), strNick.c_str());
		break;
	case GC_EVENT_ADDSTATUS:
		{
			if (CConfig::GetBoolSetting(NOTIFY_IRC_STATUS))
				Event.bNotification = true;
			tstring strNick2 = toTstring(gce->pszStatus.w);
			if (CConfig::GetBoolSetting(NOTIFY_NICKCUTOFF) && strNick2.length() > (tstring::size_type)CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET))
				strNick2 = strNick2.erase(CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET)) + L"...";

			Event.strValue = TranslateString(L"%s enables '%s' for %s", strText.c_str(), strNick2.c_str(), strNick.c_str());
			break;
		}
	case GC_EVENT_REMOVESTATUS:
		{
			if (CConfig::GetBoolSetting(NOTIFY_IRC_STATUS))
				Event.bNotification = true;
			tstring strNick2 = toTstring(gce->pszStatus.w);
			if (CConfig::GetBoolSetting(NOTIFY_NICKCUTOFF) && strNick2.length() > (tstring::size_type)CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET))
				strNick2 = strNick2.erase(CConfig::GetIntSetting(NOTIFY_NICKCUTOFF_OFFSET)) + L"...";

			Event.strValue = TranslateString(L"%s disables '%s' for %s", strText.c_str(), strNick2.c_str(), strNick.c_str());
			break;
		}
	default:
		TRACE(L"OK!\n");
		return 0;
	}
	if (gce->bIsMe || gce->si->ptszID == nullptr)
		Event.bNotification = false;

	// set the event's timestamp
	Event.bTime = true;
	time_t now;
	time(&now);
	localtime_s(&Event.Time, &now);

	SIRCMessage IRCMsg;
	IRCMsg.bIsMe = (gce->bIsMe != 0);
	IRCMsg.strMessage = Event.strValue;
	IRCMsg.Time = Event.Time;

	if (pHistory) {
		pHistory->LMessages.push_back(IRCMsg);

		// Limit the size to the session logsize
		if (pHistory->LMessages.size() > CConfig::GetIntSetting(SESSION_LOGSIZE))
			pHistory->LMessages.pop_front();
	}
	else if (gce->pszNick.w && gce->iType == GC_EVENT_QUIT) {
		strNick = toTstring(gce->pszNick.w);

		if (!CAppletManager::GetInstance()->m_LIRCHistorys.empty()) {
			list<CIRCHistory*>::iterator iter = CAppletManager::GetInstance()->m_LIRCHistorys.begin();
			list<tstring>::iterator nickiter;
			while (iter != CAppletManager::GetInstance()->m_LIRCHistorys.end()) {
				nickiter = (*iter)->LUsers.begin();
				while (nickiter != (*iter)->LUsers.end()) {
					if ((*nickiter) == strNick) {
						(*iter)->LMessages.push_back(IRCMsg);
						// Limit the size to the session logsize
						if ((*iter)->LMessages.size() > CConfig::GetIntSetting(SESSION_LOGSIZE))
							(*iter)->LMessages.pop_front();

						(*iter)->LUsers.erase(nickiter);

						Event.hContact = (*iter)->hContact;
						tstring strName = CAppletManager::GetContactDisplayname((*iter)->hContact, true);
						Event.strDescription = strName + L" - " + Event.strValue;
						Event.strSummary = L"(" + toTstring(gce->si->pszModule) + L") " + strName;
						CAppletManager::GetInstance()->HandleEvent(&Event);
						break;
					}
					nickiter++;
				}
				iter++;
			}
		}
		TRACE(L"OK!\n");
		return 0;
	}
	else if (gce->si->ptszID != nullptr) {
		TRACE(L"OK!\n");
		return 0;
	}

	if (pHistory) {
		tstring strChannel = pHistory->strChannel;
		if (CConfig::GetBoolSetting(NOTIFY_CHANNELCUTOFF) && strChannel.length() > CConfig::GetIntSetting(NOTIFY_CHANNELCUTOFF_OFFSET)) {
			strChannel = strChannel.erase(CConfig::GetIntSetting(NOTIFY_CHANNELCUTOFF_OFFSET)) + L"...";
		}
		Event.strDescription = strChannel + L" - " + Event.strValue;
		Event.strSummary = L"(" + toTstring(gce->si->pszModule) + L") " + pHistory->strChannel;
	}
	else Event.strDescription = Event.strValue;

	TRACE(L"OK!\n");

	CAppletManager::GetInstance()->HandleEvent(&Event);

	return 0;
}

//************************************************************************
// message window event hook function
//************************************************************************
int CAppletManager::HookMessageWindowEvent(WPARAM uType, LPARAM lParam)
{
	auto *pDlg = (CSrmmBaseDialog *)lParam;

	CEvent Event;
	Event.eType = EVENT_MESSAGEWINDOW;
	Event.hContact = pDlg->m_hContact;
	Event.iValue = uType;

	CAppletManager::GetInstance()->HandleEvent(&Event);
	return 0;
}


//************************************************************************
// contact typing notification hook function
//************************************************************************
int CAppletManager::HookContactIsTyping(WPARAM wParam, LPARAM lParam)
{
	MCONTACT hContact = wParam;
	int iState = (int)lParam;

	CEvent Event;

	Event.eType = EVENT_TYPING_NOTIFICATION;
	Event.hContact = hContact;
	Event.iValue = iState;

	CAppletManager::GetInstance()->HandleEvent(&Event);
	return 0;
}

//************************************************************************
// new event hook function
//************************************************************************
int CAppletManager::HookEventAdded(WPARAM wParam, LPARAM lParam)
{
	CEvent Event;

	if (CAppletManager::TranslateDBEvent(&Event, wParam, lParam))
		CAppletManager::GetInstance()->HandleEvent(&Event);

	return 0;
}

//************************************************************************
// contact status change hook function
//************************************************************************
int CAppletManager::HookStatusChanged(WPARAM wParam, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;

	if ((wParam == 0) || (strcmp(cws->szSetting, "Status") != NULL))
		return 0;


	// Prepare message and append to queue
	CEvent Event;
	Event.hContact = wParam;
	int iStatus = cws->value.wVal;
	Event.iValue = iStatus;

	int iOldStatus = CAppletManager::GetInstance()->m_ContactlistScreen.GetContactStatus(Event.hContact);

	char *szProto = Proto_GetBaseAccountName(Event.hContact);
	tstring strProto = toTstring(szProto);

	CProtocolData *pProtocolData = CAppletManager::GetInstance()->GetProtocolData(toTstring(szProto));
	if (pProtocolData == nullptr)
		return false;

	// Fetch the contacts name
	tstring strName = CAppletManager::GetContactDisplayname(Event.hContact, true);

	// Get status String
	Event.strValue = toTstring(Clist_GetStatusModeDescription(iStatus, 0));

	// check if this is an irc protocol
	CIRCConnection *pIRCCon = CAppletManager::GetInstance()->GetIRCConnection(strProto);

	// Contact signed on
	if (iOldStatus == ID_STATUS_OFFLINE && iStatus != ID_STATUS_OFFLINE) {
		if (CConfig::GetBoolSetting(NOTIFY_SIGNOFF))
			Event.bNotification = true;

		Event.eType = EVENT_SIGNED_ON;
		if (pIRCCon && Contact::IsGroupChat(Event.hContact, szProto)) {
			Event.strDescription = TranslateString(L"Joined %s", strName.c_str());

			DBVARIANT dbv;
			if (db_get_ws(Event.hContact, szProto, "Nick", &dbv))
				return 0;
			CAppletManager::GetInstance()->CreateIRCHistory(Event.hContact, dbv.pwszVal);
			db_free(&dbv);
		}
		else
			Event.strDescription = TranslateString(L"%s signed on (%s)", strName.c_str(), Event.strValue.c_str());
	}
	// Contact signed off
	else if (iStatus == ID_STATUS_OFFLINE && iOldStatus != ID_STATUS_OFFLINE) {
		if (CConfig::GetBoolSetting(NOTIFY_SIGNON))
			Event.bNotification = true;

		Event.eType = EVENT_SIGNED_OFF;
		if (pIRCCon && Contact::IsGroupChat(Event.hContact, szProto)) {
			Event.strDescription = TranslateString(L"Left %s", strName.c_str());
			// delete IRC-Channel history
			CAppletManager::GetInstance()->DeleteIRCHistory(Event.hContact);
		}
		else
			Event.strDescription = TranslateString(L"%s signed off", strName.c_str());
	}
	// Contact changed status
	else if (iStatus != iOldStatus) {
		if (CConfig::GetBoolSetting(NOTIFY_STATUS))
			Event.bNotification = true;

		Event.eType = EVENT_STATUS;
		Event.strDescription = TranslateString(L"%s is now %s", strName.c_str(), Event.strValue.c_str());
	}
	// ignore remaining events
	else
		return 0;

	if (CConfig::GetBoolSetting(NOTIFY_SHOWPROTO))
		Event.strDescription = L"(" + strProto + L") " + Event.strDescription;



	Event.strSummary = TranslateString(L"Contactlist event");

	// Block notifications after connecting/disconnecting
	if (pProtocolData->iStatus == ID_STATUS_OFFLINE || (uint32_t)pProtocolData->lTimeStamp + PROTOCOL_NOTIFY_DELAY > GetTickCount())
		Event.bNotification = false;

	//CAppletManager::GetInstance()->ActivateNotificationScreen(&Event);
	CAppletManager::GetInstance()->HandleEvent(&Event);

	return 0;
}

//************************************************************************
// protocoll ack hook function
//************************************************************************
int CAppletManager::HookProtoAck(WPARAM, LPARAM lParam)
{
	ACKDATA *pAck = (ACKDATA *)lParam;

	if (lParam == 0)
		return 0;

	// Prepare message and append to queue
	CEvent Event;

	// Message job handling
	if (pAck->type == ACKTYPE_MESSAGE) {
		list<SMessageJob*>::iterator iter = CAppletManager::GetInstance()->m_MessageJobs.begin();
		while (iter != CAppletManager::GetInstance()->m_MessageJobs.end()) {
			if ((*iter)->hEvent == (UINT_PTR)pAck->hProcess && (*iter)->hContact == pAck->hContact) {
				Event.eType = EVENT_MESSAGE_ACK;
				Event.hValue = (UINT_PTR)pAck->hProcess;
				Event.hContact = pAck->hContact;
				Event.iValue = pAck->result;
				if (pAck->lParam != 0)
					Event.strValue = toTstring((char*)pAck->lParam);
				else
					Event.strValue = L"";

				if (Event.iValue == ACKRESULT_SUCCESS)
					CAppletManager::GetInstance()->FinishMessageJob((*iter));
				else
					CAppletManager::GetInstance()->CancelMessageJob((*iter));

				CAppletManager::GetInstance()->HandleEvent(&Event);

				return 0;
			}
			iter++;
		}
	}
	// protocol status changes
	else if (pAck->type == ACKTYPE_STATUS && pAck->result == ACKRESULT_SUCCESS) {
		int iOldStatus = (INT_PTR)pAck->hProcess;
		int iNewStatus = pAck->lParam;

		tstring strProto = toTstring(pAck->szModule);

		// ignore metacontacts status changes
		if (toLower(strProto) == L"metacontacts")
			return 0;

		CProtocolData *pProtoData = CAppletManager::GetInstance()->GetProtocolData(strProto);
		if (pProtoData == nullptr)
			return 0;

		// Skip connecting status
		if (iNewStatus == ID_STATUS_CONNECTING)
			return 0;

		if (iNewStatus == ID_STATUS_OFFLINE) {
			if (CConfig::GetBoolSetting(NOTIFY_PROTO_SIGNOFF))
				Event.bNotification = true;
			Event.eType = EVENT_PROTO_DISCONNECTED;
		}
		else if (iNewStatus != ID_STATUS_OFFLINE && iOldStatus == ID_STATUS_CONNECTING) {
			if (CConfig::GetBoolSetting(NOTIFY_PROTO_SIGNON))
				Event.bNotification = true;
			Event.eType = EVENT_PROTO_CONNECTED;
		}
		else {
			if (CConfig::GetBoolSetting(NOTIFY_PROTO_STATUS))
				Event.bNotification = true;
			Event.eType = EVENT_PROTO_STATUS;
		}

		pProtoData->iStatus = iNewStatus;

		Event.iValue = iNewStatus;
		Event.strValue = strProto;

		// set the event description / summary
		tstring strStatus = toTstring(Clist_GetStatusModeDescription(iNewStatus, 0));
		Event.strDescription = L"(" + Event.strValue + L") " + TranslateString(L"You are now %s", strStatus.c_str());
		Event.strSummary = TranslateString(L"Protocol status change");

		if (Event.eType != EVENT_PROTO_STATUS)
			pProtoData->lTimeStamp = GetTickCount();

		CAppletManager::GetInstance()->HandleEvent(&Event);
		//CAppletManager::GetInstance()->ActivateNotificationScreen(&Event);
	}

	return 0;
}

//************************************************************************
// contact added hook function
//************************************************************************
int CAppletManager::HookContactAdded(WPARAM wParam, LPARAM)
{
	CEvent Event;
	Event.eType = EVENT_CONTACT_ADDED;
	Event.hContact = wParam;

	CAppletManager::GetInstance()->HandleEvent(&Event);
	return 0;
}

//************************************************************************
// contact deleted hook function
//************************************************************************
int CAppletManager::HookContactDeleted(WPARAM wParam, LPARAM)
{
	CEvent Event;
	Event.eType = EVENT_CONTACT_DELETED;
	Event.hContact = wParam;
	Event.bNotification = CConfig::GetBoolSetting(NOTIFY_CONTACTS);
	Event.bLog = Event.bNotification;

	tstring strName = CAppletManager::GetContactDisplayname(Event.hContact, true);

	Event.strDescription = TranslateString(L"%s was deleted from contactlist!", strName.c_str());

	CAppletManager::GetInstance()->HandleEvent(&Event);
	return 0;
}

//************************************************************************
// setting changed hook function
//************************************************************************
int CAppletManager::HookSettingChanged(WPARAM hContact, LPARAM lParam)
{
	DBCONTACTWRITESETTING *dbcws = (DBCONTACTWRITESETTING*)lParam;

	CEvent Event;
	Event.hContact = hContact;

	if (!strcmp(dbcws->szSetting, "Nick") || !strcmp(dbcws->szSetting, "MyHandle")) {
		DBVARIANT dbv = {0};
		// if the protocol nick has changed, check if a custom handle is set
		if (!strcmp(dbcws->szSetting, "Nick")) {
			if (!db_get_ws(Event.hContact, "CList", "MyHandle", &dbv)) {
				// handle found, ignore this event
				if (dbv.pszVal && mir_strlen(dbv.pszVal) > 0)
					return 0;
			}
			db_free(&dbv);
		}

		Event.eType = EVENT_CONTACT_NICK;
		if (dbcws->value.type != DBVT_DELETED && dbcws->value.pszVal && mir_strlen(dbcws->value.pszVal) > 0) {
			if (dbcws->value.type == DBVT_UTF8)
				Event.strValue = Utf8_Decode(dbcws->value.pszVal);
			else
				Event.strValue = toTstring(dbcws->value.pszVal);
		}
		else {
			char *szProto = Proto_GetBaseAccountName(Event.hContact);
			if (db_get_ws(Event.hContact, szProto, "Nick", &dbv))
				return 0;
			Event.strValue = dbv.pwszVal;
			db_free(&dbv);
		}
	}
	else if (!strcmp(dbcws->szModule, "CList")) {
		if (!strcmp(dbcws->szSetting, "Hidden")) {
			Event.eType = EVENT_CONTACT_HIDDEN;
			Event.iValue = Contact::IsHidden(hContact);
		}
		else if (!strcmp(dbcws->szSetting, "Group")) {
			Event.eType = EVENT_CONTACT_GROUP;

			ptrW wszGroup(Clist_GetGroup(hContact));
			if (wszGroup)
				Event.strValue = wszGroup;
		}
		else return 0;
	}
	else return 0;

	CAppletManager::GetInstance()->HandleEvent(&Event);
	return 0;
}
