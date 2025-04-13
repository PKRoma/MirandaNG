#ifndef _CLOUD_SERVICE_H_
#define _CLOUD_SERVICE_H_

enum OnConflict
{
	NONE,
	RENAME,
	REPLACE,
};

#define PS_UPLOAD "/Upload"

class CCloudService : public PROTO<CCloudService>
{
protected:
	HPLUGIN m_pPlugin;

	// utils
	std::string PreparePath(const std::string &path) const;

	virtual char* HttpStatusToError(int status = 0);
	virtual void HttpResponseToError(MHttpResponse *response);
	virtual void HandleHttpError(MHttpResponse *response);
	virtual void HandleJsonError(JSONNode &node) = 0;

	// events
	void OnModulesLoaded() override;
	MWindow OnCreateAccMgrUI(MWindow) override;

	JSONNode GetJsonResponse(MHttpResponse *response);

	INT_PTR __cdecl UploadMenuCommand(WPARAM, LPARAM);

	virtual void Upload(FileTransferParam *ftp) = 0;

public:
	std::map<MCONTACT, HWND> InterceptedContacts;

	CCloudService(const char *protoName, const wchar_t *userName, HPLUGIN);
	virtual ~CCloudService();

	INT_PTR GetCaps(int type, MCONTACT) override;

	int FileCancel(MCONTACT hContact, HANDLE hTransfer) override;
	HANDLE SendFile(MCONTACT hContact, const wchar_t *msg, wchar_t **ppszFiles) override;
		
	HPLUGIN GetId() const;
	virtual const char* GetModuleName() const = 0;

	virtual int GetIconId() const = 0;

	virtual bool IsLoggedIn() = 0;
	virtual void Login(HWND owner = nullptr) = 0;
	virtual void Logout() = 0;

	void OpenUploadDialog(MCONTACT hContact);

	static int UnInit(PROTO_INTERFACE *);
	static UINT Upload(CCloudService *service, FileTransferParam *ftp);
};

#endif //_CLOUD_SERVICE_H_
