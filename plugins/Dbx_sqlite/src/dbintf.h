#pragma once

#define OWN_CACHED_CONTACT

#include <m_db_int.h>

struct CQuery
{
	~CQuery();

	sqlite3_stmt *pQuery = nullptr;
};

struct DBCachedContact : public DBCachedContactBase
{
	int32_t m_count = 0;
};

struct CDbxSQLiteEventCursor : public DB::EventCursor
{
	CDbxSQLiteEventCursor(MCONTACT _1, sqlite3* m_db, MEVENT hDbEvent, bool reverse = false);
	~CDbxSQLiteEventCursor() override;
	MEVENT FetchNext() override;

private:
	sqlite3 *m_db;
	sqlite3_stmt *cursor;
};

class CDbxSQLite : public MDatabaseCommon, public MIDatabaseChecker, public MZeroedObject
{
	ptrW m_wszFileName;
	sqlite3 *m_db = nullptr;

	struct {
		sqlite3_stmt *cur;
		MCONTACT hContact;
		MEVENT hEvent;

		void clear() {
			if (cur)
				sqlite3_reset(cur);
			memset(this, 0, sizeof(*this));
		}
	}
	fwd, back;

	DBCachedContact m_system;

	struct Impl {
		CDbxSQLite &pro;

		CTimer m_timer;
		void OnTimer(CTimer *pTimer)
		{
			pTimer->Stop();
			pro.DBFlush(true);
		}

		Impl(CDbxSQLite &_p) :
			pro(_p),
			m_timer(Miranda_GetSystemWindow(), UINT_PTR(this))
		{
			m_timer.OnEvent = Callback(this, &Impl::OnTimer);
		}
	} m_impl;

	bool m_safetyMode, m_bReadOnly, m_bShared, m_bTranStarted;

	// contacts
	void InitContacts();
	int  GetContactEventCount(MCONTACT hContact);
	CQuery qCntCount, qCntAdd, qCntDel, qCntDelSettings, qCntCountEvents, qCntDelEvents, qCntDelEventSrt;

	// encryption
	void InitEncryption();
	CQuery qCryptGetMode, qCryptSetMode, qCryptGetProvider, qCryptSetProvider, qCryptGetKey, qCryptSetKey, qCryptEnc1, qCryptEnc2;

	// events
	LIST<char> m_modules;
	void InitEvents();
	void UninitEvents();
	CQuery qEvAdd, qEvDel, qEvEdit1, qEvEdit2, qEvBlobSize, qEvGet, qEvGetFlags, qEvSetFlags, qEvGetContact, qEvGetContact2;
	CQuery qEvFindFirst, qEvFindNext, qEvFindLast, qEvFindPrev, qEvFindUnread, qEvAddSrt, qEvDelSrt, qEvDelSrt2, qEvMetaSplit, qEvMetaMerge;
	CQuery qEvGetById, qEvUpdateId, qEvSetJson;
	int AddEventSrt(MEVENT, MCONTACT, int64_t ts);
	int DeleteEventMain(MEVENT);
	int DeleteEventSrt(MCONTACT, int64_t ts);
	int DeleteEventSrt2(MEVENT);

	// settings
	void InitSettings();
	CQuery qSettModules, qSettWrite, qSettDel, qSettEnum, qSettChanges;
	int DeleteContactSettingWorker(MCONTACT contactID, LPCSTR szModule, LPCSTR szSetting);

	void CheckConversion();
	void DBFlush(bool bForce = false);
	sqlite3_stmt* InitQuery(const char *szQuery, CQuery &stmt);

public:
	CDbxSQLite(const wchar_t *pwszFileName, bool bReadOnly, bool bShared);
	~CDbxSQLite();

	int Create();
	int Check();
	int Load();

	STDMETHODIMP_(BOOL)     IsRelational(void) override;
	STDMETHODIMP_(void)     SetCacheSafetyMode(BOOL) override;

	STDMETHODIMP_(int)      GetContactCount(void) override;
	STDMETHODIMP_(int)      DeleteContact(MCONTACT contactID) override;
	STDMETHODIMP_(MCONTACT) AddContact(void) override;
	STDMETHODIMP_(BOOL)     IsDbContact(MCONTACT contactID) override;
	STDMETHODIMP_(int)      GetContactSize(void) override;

	STDMETHODIMP_(int)      GetEventCount(MCONTACT contactID) override;
	STDMETHODIMP_(MEVENT)   AddEvent(MCONTACT contactID, const DBEVENTINFO *dbe) override;
	STDMETHODIMP_(BOOL)     DeleteEvent(MEVENT hDbEvent) override;
	STDMETHODIMP_(BOOL)     EditEvent(MEVENT hDbEvent, const DBEVENTINFO *dbe) override;
	STDMETHODIMP_(int)      SetEventJson(MEVENT hDbEvent, const char *szSetting, DBVARIANT *dbv) override;
	STDMETHODIMP_(int)      GetBlobSize(MEVENT hDbEvent) override;
	STDMETHODIMP_(BOOL)     GetEvent(MEVENT hDbEvent, DBEVENTINFO *dbe) override;
	STDMETHODIMP_(BOOL)     MarkEventRead(MCONTACT contactID, MEVENT hDbEvent) override;
	STDMETHODIMP_(MCONTACT) GetEventContact(MEVENT hDbEvent) override;
	STDMETHODIMP_(MEVENT)   FindFirstEvent(MCONTACT contactID) override;
	STDMETHODIMP_(MEVENT)   FindFirstUnreadEvent(MCONTACT contactID) override;
	STDMETHODIMP_(MEVENT)   FindLastEvent(MCONTACT contactID) override;
	STDMETHODIMP_(MEVENT)   FindNextEvent(MCONTACT contactID, MEVENT hDbEvent) override;
	STDMETHODIMP_(MEVENT)   FindPrevEvent(MCONTACT contactID, MEVENT hDbEvent) override;

	STDMETHODIMP_(MEVENT)   GetEventById(LPCSTR szModule, LPCSTR szId) override;
	STDMETHODIMP_(int)      UpdateEventId(MEVENT hDbEvent, LPCSTR szId) override;

	STDMETHODIMP_(BOOL)     EnumModuleNames(DBMODULEENUMPROC pFunc, void *pParam) override;

	STDMETHODIMP_(BOOL)     ReadCryptoKey(MBinBuffer&) override;
	STDMETHODIMP_(BOOL)     StoreCryptoKey() override;

	STDMETHODIMP_(CRYPTO_PROVIDER*) ReadProvider() override;
	STDMETHODIMP_(BOOL)     StoreProvider(CRYPTO_PROVIDER*) override;

	STDMETHODIMP_(BOOL)     EnableEncryption(BOOL) override;
	STDMETHODIMP_(BOOL)     ReadEncryption() override;

	STDMETHODIMP_(BOOL)     WriteContactSettingWorker(MCONTACT contactID, DBCONTACTWRITESETTING &dbcws) override;
	STDMETHODIMP_(BOOL)     DeleteContactSetting(MCONTACT contactID, LPCSTR szModule, LPCSTR szSetting) override;
	STDMETHODIMP_(BOOL)     EnumContactSettings(MCONTACT hContact, DBSETTINGENUMPROC pfnEnumProc, const char *szModule, void *param) override;

	STDMETHODIMP_(BOOL)     MetaMergeHistory(DBCachedContact *ccMeta, DBCachedContact *ccSub) override;
	STDMETHODIMP_(BOOL)     MetaSplitHistory(DBCachedContact *ccMeta, DBCachedContact *ccSub) override;

	STDMETHODIMP_(BOOL)     Compact() override;
	STDMETHODIMP_(BOOL)     Backup(LPCWSTR) override;
	STDMETHODIMP_(BOOL)     Flush() override;

	STDMETHODIMP_(DATABASELINK*) GetDriver() override;

	STDMETHODIMP_(DB::EventCursor*) EventCursor(MCONTACT hContact, MEVENT hDbEvent) override;
	STDMETHODIMP_(DB::EventCursor*) EventCursorRev(MCONTACT hContact, MEVENT hDbEvent) override;

	////////////////////////////////////////////////////////////////////////////////////////
	// database checker interface implementation

protected:
	STDMETHODIMP_(MIDatabaseChecker *) GetChecker() override
	{	return this;
	}

	STDMETHODIMP_(BOOL) Start(DBCHeckCallback *callback) override
	{
		cb = callback;
		return ERROR_SUCCESS;
	}
	
	STDMETHODIMP_(BOOL) CheckDb(int phase) override;
	STDMETHODIMP_(VOID) Destroy() override
	{}

	DBCHeckCallback *cb;
	int CheckPhase1(void);
	int CheckPhase2(void);
	int CheckPhase3(void);
	int CheckPhase4(void);
	int CheckPhase5(void);
};
