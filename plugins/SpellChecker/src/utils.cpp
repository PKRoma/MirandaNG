/*
Copyright (C) 2006-2010 Ricardo Pescuma Domenecci

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#include "stdafx.h"

typedef void(*FoundWrongWordCallback)(const wchar_t *word, CHARRANGE pos, void *param);

typedef map<HWND, Dialog *> DialogMapType;
DialogMapType dialogs;
DialogMapType menus;

void SetUnderline(Dialog *dlg, int pos_start, int pos_end)
{
	dlg->re->SetSel(pos_start, pos_end);

	CHARFORMAT2 cf;
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_UNDERLINE | CFM_UNDERLINETYPE;
	cf.dwEffects = CFE_UNDERLINE;
	cf.bUnderlineType = opts.underline_type + CFU_UNDERLINEDOUBLE;

	if (IsWinVer8Plus())
		cf.bUnderlineColor = 0x06;
	else
		cf.bUnderlineColor = 0x05;

	dlg->re->SendMessage(EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);

	dlg->markedSomeWord = TRUE;
}

BOOL IsMyUnderline(const CHARFORMAT2 &cf)
{
	return (cf.dwEffects & CFE_UNDERLINE)
		&& (cf.bUnderlineType & 0x0F) >= CFU_UNDERLINEDOUBLE
		&& (cf.bUnderlineType & 0x0F) <= CFU_UNDERLINETHICK
		&& (cf.bUnderlineColor) == 5;
}

void SetNoUnderline(RichEdit *re, int pos_start, int pos_end)
{
	if (opts.handle_underscore) {
		for (int i = pos_start; i <= pos_end; i++) {
			re->SetSel(i, min(i + 1, pos_end));

			CHARFORMAT2 cf;
			cf.cbSize = sizeof(cf);
			re->SendMessage(EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);

			BOOL mine = IsMyUnderline(cf);
			if (mine) {
				cf.cbSize = sizeof(cf);
				cf.dwMask = CFM_UNDERLINE | CFM_UNDERLINETYPE;
				cf.dwEffects = 0;
				cf.bUnderlineType = CFU_UNDERLINE;
				cf.bUnderlineColor = 0;
				re->SendMessage(EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);
			}
		}
	}
	else {
		re->SetSel(pos_start, pos_end);

		CHARFORMAT2 cf;
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_UNDERLINE | CFM_UNDERLINETYPE;
		cf.dwEffects = 0;
		cf.bUnderlineType = CFU_UNDERLINE;
		cf.bUnderlineColor = 0;
		re->SendMessage(EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);
	}
}

void SetNoUnderline(Dialog *dlg)
{
	dlg->re->Stop();
	SetNoUnderline(dlg->re, 0, dlg->re->GetTextLength());
	dlg->markedSomeWord = FALSE;
	dlg->re->Start();
}

inline BOOL IsURL(wchar_t c)
{
	return (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| IsNumber(c)
		|| c == '.' || c == '/'
		|| c == '\\' || c == '?'
		|| c == '=' || c == '&'
		|| c == '%' || c == '-'
		|| c == '_' || c == ':'
		|| c == '@' || c == '#';
}

int FindURLEnd(Dialog *dlg, wchar_t *text, int start_pos, int *checked_until = nullptr)
{
	int num_slashes = 0;
	int num_ats = 0;
	int num_dots = 0;

	int i = start_pos;

	for (; IsURL(text[i]) || dlg->lang->isWordChar(text[i]); i++) {
		wchar_t c = text[i];
		if (c == '\\' || c == '/')
			num_slashes++;
		else if (c == '.')
			num_dots++;
		else if (c == '@')
			num_ats++;
	}

	if (checked_until != nullptr)
		*checked_until = i;

	if (num_slashes <= 0 && num_ats <= 0 && num_dots <= 0)
		return -1;

	if (num_slashes == 0 && num_ats == 0 && num_dots < 2)
		return -1;

	if (i - start_pos < 2)
		return -1;

	return i;
}


int ReplaceWord(Dialog *dlg, CHARRANGE &sel, const wchar_t *new_word)
{
	dlg->re->Stop();
	dlg->re->ResumeUndo();

	int dif = dlg->re->Replace(sel.cpMin, sel.cpMax, new_word);

	dlg->re->SuspendUndo();
	dlg->re->Start();

	return dif;
}

class TextParser
{
public:
	virtual ~TextParser() {}

	/// @return true when finished an word
	virtual bool feed(int pos, wchar_t c) = 0;
	virtual int  getFirstCharPos() = 0;
	virtual void reset() = 0;
	virtual bool deal(const wchar_t *text, bool*, CMStringW &replacement) = 0;
};

class SpellParser : public TextParser
{
	Dictionary *dict;
	int last_pos;
	BOOL found_real_char;

public:
	SpellParser(Dictionary *dict) : dict(dict)
	{
		reset();
	}

	virtual void reset()
	{
		last_pos = -1;
		found_real_char = FALSE;
	}

	virtual bool feed(int pos, wchar_t c)
	{
		// Is inside a word?
		if (dict->isWordChar(c) || IsNumber(c)) {
			if (last_pos == -1)
				last_pos = pos;

			if (c != '-' && !IsNumber(c))
				found_real_char = TRUE;

			return false;
		}

		if (!found_real_char)
			last_pos = -1;

		return (last_pos != -1);
	}

	virtual int getFirstCharPos()
	{
		return (!found_real_char) ? -1 : last_pos;
	}

	virtual bool deal(const wchar_t *text, bool *mark, CMStringW &replacement) override
	{
		// Is it correct?
		if (dict->spell(text))
			return false;

		// Has to auto-correct?
		if (opts.auto_replace_dict) {
			wchar_t *pwszWord = dict->autoSuggestOne(text);
			if (pwszWord != nullptr) {
				replacement = pwszWord;
				free(pwszWord);				
				return true;
			}
		}

		*mark = true;
		return false;
	}
};

class AutoReplaceParser : public TextParser
{
	AutoReplaceMap *ar;
	int last_pos;

public:
	AutoReplaceParser(AutoReplaceMap *ar) : ar(ar)
	{
		reset();
	}

	virtual void reset()
	{
		last_pos = -1;
	}

	virtual bool feed(int pos, wchar_t c)
	{
		// Is inside a word?
		if (ar->isWordChar(c)) {
			if (last_pos == -1)
				last_pos = pos;
			return false;
		}

		return (last_pos != -1);
	}

	virtual int getFirstCharPos()
	{
		return last_pos;
	}

	virtual bool deal(const wchar_t *text, bool*, CMStringW &replacement) override
	{
		replacement = ar->autoReplace(text);
		return !replacement.IsEmpty();
	}
};

int CheckTextLine(Dialog *dlg, int line, TextParser *parser,
						BOOL ignore_upper, BOOL ignore_with_numbers, BOOL test_urls,
						const CHARRANGE &ignored, FoundWrongWordCallback callback, void *param)
{
	int errors = 0;
	wchar_t text[1024];
	dlg->re->GetLine(line, text, _countof(text));
	int len = (int)mir_wstrlen(text);
	int first_char = dlg->re->GetFirstCharOfLine(line);

	// Now lets get the words
	int next_char_for_url = 0;
	for (int pos = 0; pos < len; pos++) {
		int url_end = pos;
		if (pos >= next_char_for_url) {
			url_end = FindURLEnd(dlg, text, pos, &next_char_for_url);
			next_char_for_url++;
		}

		if (url_end > pos) {
			BOOL ignore_url = FALSE;

			if (test_urls) {
				// All the url must be handled by the parser
				parser->reset();

				BOOL feed = FALSE;
				for (int j = pos; !feed && j <= url_end; j++)
					feed = parser->feed(j, text[j]);

				if (feed || parser->getFirstCharPos() != pos)
					ignore_url = TRUE;
			}
			else ignore_url = TRUE;

			pos = url_end;

			if (ignore_url) {
				parser->reset();
				continue;
			}
		}
		else {
			wchar_t c = text[pos];

			BOOL feed = parser->feed(pos, c);
			if (!feed) {
				if (pos >= len - 1)
					pos = len; // To check the last block
				else
					continue;
			}
		}

		int last_pos = parser->getFirstCharPos();
		parser->reset();

		if (last_pos < 0)
			continue;

		// We found a word
		CHARRANGE sel = { first_char + last_pos, first_char + pos };

		// Is in ignored range?
		if (sel.cpMin <= ignored.cpMax && sel.cpMax >= ignored.cpMin)
			continue;

		if (ignore_upper) {
			BOOL upper = TRUE;
			for (int i = last_pos; i < pos && upper; i++)
				upper = !IsCharLower(text[i]);
			if (upper)
				continue;
		}

		if (ignore_with_numbers) {
			BOOL hasNumbers = FALSE;
			for (int i = last_pos; i < pos && !hasNumbers; i++)
				hasNumbers = IsNumber(text[i]);
			if (hasNumbers)
				continue;
		}

		text[pos] = 0;

		bool mark = false;
		CMStringW replacement;
		bool replace = parser->deal(&text[last_pos], &mark, replacement);
		if (replace) {
			// Replace in rich edit
			int dif = dlg->re->Replace(sel.cpMin, sel.cpMax, replacement);
			if (dif != 0) {
				// Read line again
				dlg->re->GetLine(line, text, _countof(text));
				len = (int)mir_wstrlen(text);

				int old_first_char = first_char;
				first_char = dlg->re->GetFirstCharOfLine(line);

				pos = max(-1, pos + dif + old_first_char - first_char);
			}
		}
		else if (mark) {
			SetUnderline(dlg, sel.cpMin, sel.cpMax);

			if (callback != nullptr)
				callback(&text[last_pos], sel, param);

			errors++;
		}
	}

	return errors;
}

// Checks for errors in all text
int CheckText(Dialog *dlg, BOOL check_all, FoundWrongWordCallback callback = nullptr, void *param = nullptr)
{
	int errors = 0;

	dlg->re->Stop();

	if (dlg->re->GetTextLength() > 0) {
		int lines = dlg->re->GetLineCount();
		int line = 0;
		CHARRANGE cur_sel = { -1, -1 };

		if (!check_all) {
			// Check only the current line, one up and one down
			int current_line = dlg->re->GetLineFromChar(dlg->re->GetSel().cpMin);
			line = max(line, current_line - 1);
			lines = min(lines, current_line + 2);
			cur_sel = dlg->re->GetSel();
		}

		for (; line < lines; line++) {
			int first_char = dlg->re->GetFirstCharOfLine(line);

			SetNoUnderline(dlg->re, first_char, first_char + dlg->re->GetLineLength(line));

			if (opts.auto_replace_user) {
				AutoReplaceParser parser(dlg->lang->autoReplace);
				errors += CheckTextLine(dlg, line, &parser, FALSE, FALSE, TRUE, cur_sel, callback, param);
			}

			SpellParser parser(dlg->lang);
			errors += CheckTextLine(dlg, line, &parser, opts.ignore_uppercase, opts.ignore_with_numbers, FALSE, cur_sel, callback, param);
		}
	}

	// Fix last char
	int len = dlg->re->GetTextLength();
	SetNoUnderline(dlg->re, len, len);

	dlg->re->Start();
	return errors;
}

void ToLocaleID(wchar_t *szKLName, size_t size)
{
	wchar_t *stopped = nullptr;
	USHORT langID = (USHORT)wcstol(szKLName, &stopped, 16);

	wchar_t ini[32], end[32];
	GetLocaleInfo(MAKELCID(langID, 0), LOCALE_SISO639LANGNAME, ini, _countof(ini));
	GetLocaleInfo(MAKELCID(langID, 0), LOCALE_SISO3166CTRYNAME, end, _countof(end));

	mir_snwprintf(szKLName, size, L"%s_%s", ini, end);
}

void LoadDictFromKbdl(Dialog *dlg)
{
	wchar_t szKLName[KL_NAMELENGTH + 1];

	// Use default input language
	HKL hkl = GetKeyboardLayout(0);
	mir_snwprintf(szKLName, L"%x", (int)LOWORD(hkl));
	ToLocaleID(szKLName, _countof(szKLName));

	int d = GetClosestLanguage(szKLName);
	if (d >= 0) {
		dlg->lang = &languages[d];
		dlg->lang->load();

		if (dlg->srmm)
			ModifyIcon(dlg);
	}
}

int TimerCheck(Dialog *dlg, BOOL forceCheck = FALSE)
{
	KillTimer(dlg->hwnd, TIMER_ID);

	if (!dlg->enabled || dlg->lang == nullptr)
		return -1;

	if (!dlg->lang->isLoaded()) {
		SetTimer(dlg->hwnd, TIMER_ID, 500, nullptr);
		return -1;
	}

	// Don't check if field is read-only
	if (dlg->re->IsReadOnly())
		return -1;

	int len = dlg->re->GetTextLength();
	if (!forceCheck && len == dlg->old_text_len && !dlg->changed)
		return -1;

	dlg->old_text_len = len;
	dlg->changed = FALSE;

	return CheckText(dlg, TRUE);
}

LRESULT CALLBACK OwnerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DialogMapType::iterator dlgit = dialogs.find(hwnd);
	if (dlgit == dialogs.end())
		return -1;

	Dialog *dlg = dlgit->second;

	if (msg == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == 1624)) {
		if (opts.ask_when_sending_with_error) {
			int errors = TimerCheck(dlg, TRUE);
			if (errors > 0) {
				wchar_t text[500];
				mir_snwprintf(text, TranslateT("There are %d spelling errors. Are you sure you want to send this message?"), errors);
				if (MessageBox(hwnd, text, TranslateT("Spell Checker"), MB_ICONQUESTION | MB_YESNO) == IDNO)
					return TRUE;
			}
		}
		else if (opts.auto_replace_dict || opts.auto_replace_user) {
			// Fix all
			TimerCheck(dlg);
		}

		if (dlg->markedSomeWord)
			// Remove underline
			SetNoUnderline(dlg);

		// Schedule to re-parse
		KillTimer(dlg->hwnd, TIMER_ID);
		SetTimer(dlg->hwnd, TIMER_ID, 100, nullptr);

		dlg->changed = TRUE;
	}

	return mir_callNextSubclass(hwnd, OwnerProc, msg, wParam, lParam);
}

void ToggleEnabled(Dialog *dlg)
{
	dlg->enabled = !dlg->enabled;
	g_plugin.setByte(dlg->hContact, dlg->name, dlg->enabled);

	if (!dlg->enabled)
		SetNoUnderline(dlg);
	else {
		dlg->changed = TRUE;
		SetTimer(dlg->hwnd, TIMER_ID, 100, nullptr);
	}

	if (dlg->srmm)
		ModifyIcon(dlg);
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DialogMapType::iterator dlgit = dialogs.find(hwnd);
	if (dlgit == dialogs.end())
		return -1;

	Dialog *dlg = dlgit->second;
	if (dlg == nullptr)
		return -1;

	// Hotkey support
	MSG msgData = { hwnd, msg, wParam, lParam };
	int action = Hotkey_Check(&msgData, "Spell Checker");
	if (action == HOTKEY_ACTION_TOGGLE) {
		ToggleEnabled(dlg);
		return 1;
	}

	LRESULT ret = mir_callNextSubclass(hwnd, EditProc, msg, wParam, lParam);
	if ((dlgit = dialogs.find(hwnd)) == dialogs.end())
		return ret;

	switch (msg) {
	case WM_KEYDOWN:
		if (wParam != VK_DELETE)
			break;

	case WM_CHAR:
		if (dlg->re->IsStopped())
			break;

		if (lParam & (1 << 28))	// ALT key
			break;

		if (GetKeyState(VK_CONTROL) & 0x8000)	// CTRL key
			break;

		{
			wchar_t c = (wchar_t)wParam;
			BOOL deleting = (c == VK_BACK || c == VK_DELETE);

			// Need to do that to avoid changing the word while typing
			KillTimer(hwnd, TIMER_ID);
			SetTimer(hwnd, TIMER_ID, 1000, nullptr);

			dlg->changed = TRUE;

			if (!deleting && (lParam & 0xFF) > 1)	// Repeat rate
				break;

			if (!dlg->enabled || dlg->lang == nullptr || !dlg->lang->isLoaded())
				break;

			// Don't check if field is read-only
			if (dlg->re->IsReadOnly())
				break;

			if (!deleting && !dlg->lang->isWordChar(c))
				CheckText(dlg, FALSE);
			else {
				// Remove underline of current word
				CHARFORMAT2 cf;
				cf.cbSize = sizeof(cf);
				dlg->re->SendMessage(EM_GETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);

				if (IsMyUnderline(cf)) {
					dlg->re->Stop();

					CHARRANGE sel = dlg->re->GetSel();

					wchar_t text[1024];
					int first_char;
					GetWordCharRange(dlg, sel, text, _countof(text), first_char);

					SetNoUnderline(dlg->re, sel.cpMin, sel.cpMax);

					dlg->re->Start();
				}
			}
		}
		break;

	case EM_REPLACESEL:
	case WM_SETTEXT:
	case EM_SETTEXTEX:
	case EM_PASTESPECIAL:
	case WM_PASTE:
		if (dlg->re->IsStopped())
			break;

		KillTimer(hwnd, TIMER_ID);
		SetTimer(hwnd, TIMER_ID, 100, nullptr);

		dlg->changed = TRUE;
		break;

	case WM_TIMER:
		if (wParam == TIMER_ID)
			TimerCheck(dlg);
		break;

	case WMU_DICT_CHANGED:
		KillTimer(hwnd, TIMER_ID);
		SetTimer(hwnd, TIMER_ID, 100, nullptr);

		dlg->changed = TRUE;
		break;

	case WMU_KBDL_CHANGED:
		if (opts.auto_locale) {
			KillTimer(hwnd, TIMER_ID);
			SetTimer(hwnd, TIMER_ID, 100, nullptr);

			dlg->changed = TRUE;

			LoadDictFromKbdl(dlg);
		}
		break;

	case WM_INPUTLANGCHANGE:
		// Allow others to process this message and we get only the result
		PostMessage(hwnd, WMU_KBDL_CHANGED, 0, 0);
		break;
	}

	return ret;
}

int GetClosestLanguage(wchar_t *lang_name)
{
	// Search the language by name
	for (auto &it : languages)
		if (mir_wstrcmpi(it->language, lang_name) == 0)
			return languages.indexOf(&it);

	// Try searching by the prefix only
	wchar_t lang[128];
	mir_wstrncpy(lang, lang_name, _countof(lang));
	{
		wchar_t *p = wcschr(lang, '_');
		if (p != nullptr)
			*p = '\0';
	}

	// First check if there is a language that is only the prefix
	for (auto &it : languages)
		if (mir_wstrcmpi(it->language, lang) == 0)
			return languages.indexOf(&it);

	// Now try any suffix
	size_t len = mir_wstrlen(lang);
	for (auto &it : languages) {
		wchar_t *p = wcschr(it->language, '_');
		if (p == nullptr)
			continue;

		size_t prefix_len = p - it->language;
		if (prefix_len != len)
			continue;

		if (wcsnicmp(it->language, lang_name, len) == 0)
			return languages.indexOf(&it);
	}

	return -1;
}

void GetUserProtoLanguageSetting(Dialog *dlg, MCONTACT hContact, char *group, char *setting)
{
	ptrW wszLang(db_get_wsa(hContact, group, setting));
	if (wszLang == nullptr)
		return;

	for (auto &dict : languages) {
		if (mir_wstrcmpi(dict->localized_name, wszLang) == 0 || mir_wstrcmpi(dict->english_name, wszLang) == 0 || mir_wstrcmpi(dict->language, wszLang) == 0) {
			mir_wstrncpy(dlg->lang_name, dict->language, _countof(dlg->lang_name));
			break;
		}
	}
}

void GetUserLanguageSetting(Dialog *dlg, char *setting)
{
	char *proto = Proto_GetBaseAccountName(dlg->hContact);
	if (proto == nullptr)
		return;

	GetUserProtoLanguageSetting(dlg, dlg->hContact, proto, setting);
	if (dlg->lang_name[0] != '\0')
		return;

	GetUserProtoLanguageSetting(dlg, dlg->hContact, "UserInfo", setting);
	if (dlg->lang_name[0] != '\0')
		return;

	// If not found and is inside meta, try to get from the meta
	MCONTACT hMetaContact = db_mc_getMeta(dlg->hContact);
	if (hMetaContact != NULL) {
		GetUserProtoLanguageSetting(dlg, hMetaContact, META_PROTO, setting);
		if (dlg->lang_name[0] != '\0')
			return;

		GetUserProtoLanguageSetting(dlg, hMetaContact, "UserInfo", setting);
	}
}

void GetContactLanguage(Dialog *dlg)
{
	DBVARIANT dbv = { 0 };

	dlg->lang_name[0] = '\0';

	if (dlg->hContact == NULL) {
		if (!g_plugin.getWString(dlg->name, &dbv)) {
			mir_wstrncpy(dlg->lang_name, dbv.pwszVal, _countof(dlg->lang_name));
			db_free(&dbv);
		}
	}
	else {
		if (!g_plugin.getWString(dlg->hContact, "TalkLanguage", &dbv)) {
			mir_wstrncpy(dlg->lang_name, dbv.pwszVal, _countof(dlg->lang_name));
			db_free(&dbv);
		}

		if (dlg->lang_name[0] == '\0' && !db_get_ws(dlg->hContact, "eSpeak", "TalkLanguage", &dbv)) {
			mir_wstrncpy(dlg->lang_name, dbv.pwszVal, _countof(dlg->lang_name));
			db_free(&dbv);
		}

		// Try from metacontact
		if (dlg->lang_name[0] == '\0') {
			MCONTACT hMetaContact = db_mc_getMeta(dlg->hContact);
			if (hMetaContact != NULL) {
				if (!g_plugin.getWString(hMetaContact, "TalkLanguage", &dbv)) {
					mir_wstrncpy(dlg->lang_name, dbv.pwszVal, _countof(dlg->lang_name));
					db_free(&dbv);
				}

				if (dlg->lang_name[0] == '\0' && !db_get_ws(hMetaContact, "eSpeak", "TalkLanguage", &dbv)) {
					mir_wstrncpy(dlg->lang_name, dbv.pwszVal, _countof(dlg->lang_name));
					db_free(&dbv);
				}
			}
		}

		// Try to get from Language info
		if (dlg->lang_name[0] == '\0')
			GetUserLanguageSetting(dlg, "Language");
		if (dlg->lang_name[0] == '\0')
			GetUserLanguageSetting(dlg, "Language1");
		if (dlg->lang_name[0] == '\0')
			GetUserLanguageSetting(dlg, "Language2");
		if (dlg->lang_name[0] == '\0')
			GetUserLanguageSetting(dlg, "Language3");

		// Use default lang
		if (dlg->lang_name[0] == '\0')
			mir_wstrncpy(dlg->lang_name, opts.default_language, _countof(dlg->lang_name));
	}

	int i = GetClosestLanguage(dlg->lang_name);
	if (i < 0) {
		// Lost a dict?
		mir_wstrncpy(dlg->lang_name, opts.default_language, _countof(dlg->lang_name));
		i = GetClosestLanguage(dlg->lang_name);
	}

	if (i >= 0) {
		dlg->lang = &languages[i];
		dlg->lang->load();
	}
	else dlg->lang = nullptr;
}

void ModifyIcon(Dialog *dlg)
{
	for (int i = 0; i < languages.getCount(); i++) {
		if (&languages[i] == dlg->lang)
			Srmm_SetIconFlags(dlg->hContact, MODULENAME, i, dlg->enabled ? 0 : MBF_DISABLED);
		else
			Srmm_SetIconFlags(dlg->hContact, MODULENAME, i, MBF_HIDDEN);
	}
}

INT_PTR AddContactTextBoxService(WPARAM wParam, LPARAM)
{
	SPELLCHECKER_ITEM *sci = (SPELLCHECKER_ITEM *)wParam;
	if (sci == nullptr || sci->cbSize != sizeof(SPELLCHECKER_ITEM))
		return -1;

	return AddContactTextBox(sci->hContact, sci->hwnd, sci->window_name, FALSE, nullptr);
}

int AddContactTextBox(MCONTACT hContact, HWND hwnd, char *name, BOOL srmm, HWND hwndOwner)
{
	if (languages.getCount() <= 0)
		return 0;

	if (dialogs.find(hwnd) == dialogs.end()) {
		// Fill dialog data
		Dialog *dlg = (Dialog *)malloc(sizeof(Dialog));
		memset(dlg, 0, sizeof(Dialog));

		dlg->re = new RichEdit(hwnd);
		if (!dlg->re->IsValid()) {
			delete dlg->re;
			free(dlg);
			return 0;
		}

		dlg->hContact = hContact;
		dlg->hwnd = hwnd;
		strncpy(dlg->name, name, _countof(dlg->name));
		dlg->enabled = g_plugin.getByte(dlg->hContact, dlg->name, 1);
		dlg->srmm = srmm;

		GetContactLanguage(dlg);

		if (opts.auto_locale)
			LoadDictFromKbdl(dlg);

		mir_subclassWindow(dlg->hwnd, EditProc);
		dialogs[hwnd] = dlg;

		if (dlg->srmm && hwndOwner != nullptr) {
			dlg->hwnd_owner = hwndOwner;
			mir_subclassWindow(dlg->hwnd_owner, OwnerProc);
			dialogs[dlg->hwnd_owner] = dlg;

			ModifyIcon(dlg);
		}

		if (dlg->lang != nullptr)
			dlg->lang->load();

		SetTimer(hwnd, TIMER_ID, 1000, nullptr);
	}

	return 0;
}

#define DESTROY_MENY(_m_)	if (_m_ != NULL) { DestroyMenu(_m_); _m_ = NULL; }

void FreePopupData(Dialog *dlg)
{
	DESTROY_MENY(dlg->hLanguageSubMenu);
	DESTROY_MENY(dlg->hWrongWordsSubMenu);

	if (dlg->wrong_words != nullptr) {
		for (unsigned i = 0; i < dlg->wrong_words->size(); i++) {
			FREE((*dlg->wrong_words)[i].word);

			DESTROY_MENY((*dlg->wrong_words)[i].hMeSubMenu);
			DESTROY_MENY((*dlg->wrong_words)[i].hCorrectSubMenu);
			DESTROY_MENY((*dlg->wrong_words)[i].hReplaceSubMenu);

			(*dlg->wrong_words)[i].suggestions.clear();
		}

		delete dlg->wrong_words;
		dlg->wrong_words = nullptr;
	}
}

INT_PTR RemoveContactTextBoxService(WPARAM wParam, LPARAM)
{
	HWND hwnd = (HWND)wParam;
	if (hwnd == nullptr)
		return -1;

	return RemoveContactTextBox(hwnd);
}

int RemoveContactTextBox(HWND hwnd)
{
	DialogMapType::iterator dlgit = dialogs.find(hwnd);
	if (dlgit != dialogs.end()) {
		Dialog *dlg = dlgit->second;

		KillTimer(hwnd, TIMER_ID);

		mir_unsubclassWindow(hwnd, EditProc);
		if (dlg->hwnd_owner)
			mir_unsubclassWindow(dlg->hwnd_owner, OwnerProc);

		dialogs.erase(hwnd);
		if (dlg->hwnd_owner != nullptr)
			dialogs.erase(dlg->hwnd_owner);

		delete dlg->re;
		FreePopupData(dlg);
		free(dlg);
	}

	return 0;
}

// TODO Make this better
BOOL GetWordCharRange(Dialog *dlg, CHARRANGE &sel, wchar_t *text, size_t text_len, int &first_char)
{
	// Get line
	int line = dlg->re->GetLineFromChar(sel.cpMin);

	// Get text
	dlg->re->GetLine(line, text, text_len);
	first_char = dlg->re->GetFirstCharOfLine(line);

	// Find the word
	sel.cpMin--;
	while (sel.cpMin >= first_char && (dlg->lang->isWordChar(text[sel.cpMin - first_char])
		|| IsNumber(text[sel.cpMin - first_char])))
		sel.cpMin--;
	sel.cpMin++;

	while (text[sel.cpMax - first_char] != '\0' && (dlg->lang->isWordChar(text[sel.cpMax - first_char])
		|| IsNumber(text[sel.cpMax - first_char])))
		sel.cpMax++;

	// Has a word?
	if (sel.cpMin >= sel.cpMax)
		return FALSE;

	// See if it has only '-'s
	BOOL has_valid_char = FALSE;
	for (int i = sel.cpMin; i < sel.cpMax && !has_valid_char; i++)
		has_valid_char = (text[i - first_char] != '-');

	return has_valid_char;
}

wchar_t* GetWordUnderPoint(Dialog *dlg, POINT pt, CHARRANGE &sel)
{
	// Get text
	if (dlg->re->GetTextLength() <= 0)
		return nullptr;

	// Get pos
	sel.cpMin = sel.cpMax = dlg->re->GetCharFromPos(pt);

	// Get text
	wchar_t text[1024];
	int first_char;

	if (!GetWordCharRange(dlg, sel, text, _countof(text), first_char))
		return nullptr;

	// copy the word
	text[sel.cpMax - first_char] = '\0';
	return mir_wstrdup(&text[sel.cpMin - first_char]);
}

void AppendSubmenu(HMENU hMenu, HMENU hSubMenu, const wchar_t *name)
{
	MENUITEMINFO mii = { 0 };
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_SUBMENU | MIIM_TYPE;
	mii.fType = MFT_STRING;
	mii.hSubMenu = hSubMenu;
	mii.dwTypeData = (LPWSTR)name;
	mii.cch = (int)mir_wstrlen(name);
	InsertMenuItem(hMenu, 0, TRUE, &mii);
}

void AppendMenuItem(HMENU hMenu, int id, wchar_t *name, HICON hIcon, BOOL checked)
{
	ICONINFO iconInfo;
	GetIconInfo(hIcon, &iconInfo);

	MENUITEMINFO mii = { 0 };
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_CHECKMARKS | MIIM_TYPE | MIIM_STATE;
	mii.fType = MFT_STRING;
	mii.fState = (checked ? MFS_CHECKED : 0);
	mii.wID = id;
	mii.hbmpChecked = iconInfo.hbmColor;
	mii.hbmpUnchecked = iconInfo.hbmColor;
	mii.dwTypeData = name;
	mii.cch = (int)mir_wstrlen(name);
	InsertMenuItem(hMenu, 0, TRUE, &mii);
}

#define LANGUAGE_MENU_ID_BASE 10
#define WORD_MENU_ID_BASE 100
#define AUTOREPLACE_MENU_ID_BASE 50

void AddMenuForWord(Dialog *dlg, const wchar_t *word, CHARRANGE &pos, HMENU hMenu, BOOL in_submenu, UINT base)
{
	if (dlg->wrong_words == nullptr)
		dlg->wrong_words = new vector<WrongWordPopupMenuData>(1);
	else
		dlg->wrong_words->resize(dlg->wrong_words->size() + 1);

	WrongWordPopupMenuData &data = (*dlg->wrong_words)[dlg->wrong_words->size() - 1];
	memset(&data, 0, sizeof(WrongWordPopupMenuData));

	// Get suggestions
	data.word = _wcsdup(word);
	data.pos = pos;
	data.suggestions = dlg->lang->suggest(word);

	Suggestions &suggestions = data.suggestions;

	if (in_submenu) {
		data.hMeSubMenu = CreatePopupMenu();
		AppendSubmenu(hMenu, data.hMeSubMenu, word);
		hMenu = data.hMeSubMenu;
	}

	data.hReplaceSubMenu = CreatePopupMenu();

	int iCount = (int)suggestions.size();
	InsertMenu(data.hReplaceSubMenu, 0, MF_BYPOSITION, base + AUTOREPLACE_MENU_ID_BASE + iCount, TranslateT("Other..."));
	if (iCount > 0) {
		InsertMenu(data.hReplaceSubMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
		for (int i = iCount - 1; i >= 0; i--)
			InsertMenu(data.hReplaceSubMenu, 0, MF_BYPOSITION, base + AUTOREPLACE_MENU_ID_BASE + i, suggestions[i].c_str());
	}

	AppendSubmenu(hMenu, data.hReplaceSubMenu, TranslateT("Always replace with"));

	InsertMenu(hMenu, 0, MF_BYPOSITION, base + iCount + 1, TranslateT("Ignore all"));
	InsertMenu(hMenu, 0, MF_BYPOSITION, base + iCount, TranslateT("Add to dictionary"));

	if (iCount > 0) {
		HMENU hSubMenu;
		if (opts.cascade_corrections) {
			hSubMenu = data.hCorrectSubMenu = CreatePopupMenu();
			AppendSubmenu(hMenu, hSubMenu, TranslateT("Corrections"));
		}
		else {
			InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
			hSubMenu = hMenu;
		}

		for (int i = iCount - 1; i >= 0; i--)
			InsertMenu(hSubMenu, 0, MF_BYPOSITION, base + i, suggestions[i].c_str());
	}

	if (!in_submenu && opts.show_wrong_word) {
		InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

		wchar_t text[128];
		mir_snwprintf(text, TranslateT("Wrong word: %s"), word);
		InsertMenu(hMenu, 0, MF_BYPOSITION, 0, text);
	}
}

struct FoundWrongWordParam
{
	Dialog *dlg;
	int count;
};

void FoundWrongWord(const wchar_t *word, CHARRANGE pos, void *param)
{
	FoundWrongWordParam *p = (FoundWrongWordParam*)param;

	p->count++;

	AddMenuForWord(p->dlg, word, pos, p->dlg->hWrongWordsSubMenu, TRUE, WORD_MENU_ID_BASE * p->count);
}

void AddItemsToMenu(Dialog *dlg, HMENU hMenu, POINT pt, HWND hwndOwner)
{
	FreePopupData(dlg);
	if (opts.use_flags) {
		dlg->hwnd_menu_owner = hwndOwner;
		menus[hwndOwner] = dlg;
	}

	// Make menu
	if (GetMenuItemCount(hMenu) > 0)
		InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

	if (languages.getCount() > 0 && dlg->enabled) {
		dlg->hLanguageSubMenu = CreatePopupMenu();

		if (dlg->hwnd_menu_owner != nullptr)
			mir_subclassWindow(dlg->hwnd_menu_owner, MenuWndProc);

		// First add languages
		for (int i = 0; i < languages.getCount(); i++)
			AppendMenu(dlg->hLanguageSubMenu, MF_STRING | (&languages[i] == dlg->lang ? MF_CHECKED : 0),
			LANGUAGE_MENU_ID_BASE + i, languages[i].full_name);

		AppendSubmenu(hMenu, dlg->hLanguageSubMenu, TranslateT("Language"));
	}

	InsertMenu(hMenu, 0, MF_BYPOSITION, 1, TranslateT("Enable spell checking"));
	CheckMenuItem(hMenu, 1, MF_BYCOMMAND | (dlg->enabled ? MF_CHECKED : MF_UNCHECKED));

	// Get text
	if (dlg->lang != nullptr && dlg->enabled) {
		if (opts.show_all_corrections) {
			dlg->hWrongWordsSubMenu = CreatePopupMenu();

			FoundWrongWordParam p = { dlg, 0 };
			CheckText(dlg, TRUE, FoundWrongWord, &p);

			if (p.count > 0)
				AppendSubmenu(hMenu, dlg->hWrongWordsSubMenu, TranslateT("Wrong words"));
		}
		else {
			CHARRANGE sel;
			ptrW word(GetWordUnderPoint(dlg, pt, sel));
			if (word != nullptr && !dlg->lang->spell(word)) {
				InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
				AddMenuForWord(dlg, word, sel, hMenu, FALSE, WORD_MENU_ID_BASE);
			}
		}
	}
}

static void AddWordToDictCallback(BOOL canceled, Dictionary *dict,
	const wchar_t *find, const wchar_t *replace, BOOL useVariables,
	const wchar_t *, void *param)
{
	if (canceled)
		return;

	dict->autoReplace->add(find, replace, useVariables);

	HWND hwndParent = (HWND)param;
	if (hwndParent != nullptr)
		PostMessage(hwndParent, WMU_DICT_CHANGED, 0, 0);
}

BOOL HandleMenuSelection(Dialog *dlg, int selection)
{
	BOOL ret = FALSE;

	if (selection == 1) {
		ToggleEnabled(dlg);
		ret = TRUE;
	}
	else if (selection >= LANGUAGE_MENU_ID_BASE && selection < LANGUAGE_MENU_ID_BASE + languages.getCount()) {
		SetNoUnderline(dlg);

		if (dlg->hContact == 0)
			g_plugin.setWString(dlg->name, languages[selection - LANGUAGE_MENU_ID_BASE].language);
		else
			g_plugin.setWString(dlg->hContact, "TalkLanguage", languages[selection - LANGUAGE_MENU_ID_BASE].language);

		GetContactLanguage(dlg);

		if (dlg->srmm)
			ModifyIcon(dlg);

		ret = TRUE;
	}
	else if (selection > 0 && dlg->wrong_words != nullptr && selection >= WORD_MENU_ID_BASE && selection < (dlg->wrong_words->size() + 1) * WORD_MENU_ID_BASE) {
		int pos = selection / WORD_MENU_ID_BASE;
		selection -= pos * WORD_MENU_ID_BASE;
		pos--; // 0 based
		WrongWordPopupMenuData &data = (*dlg->wrong_words)[pos];

		int iCount = (int)data.suggestions.size();
		if (selection < iCount) {
			// TODO Assert that text hasn't changed
			ReplaceWord(dlg, data.pos, data.suggestions[selection].c_str());
			ret = TRUE;
		}
		else if (selection == iCount) {
			dlg->lang->addWord(data.word);
			ret = TRUE;
		}
		else if (selection == iCount + 1) {
			dlg->lang->ignoreWord(data.word);
			ret = TRUE;
		}
		else if (selection >= AUTOREPLACE_MENU_ID_BASE && selection < AUTOREPLACE_MENU_ID_BASE + iCount + 1) {
			selection -= AUTOREPLACE_MENU_ID_BASE;
			if (selection == iCount) {
				ShowAutoReplaceDialog(dlg->hwnd_owner != nullptr ? dlg->hwnd_owner : dlg->hwnd, FALSE,
					dlg->lang, data.word, nullptr, FALSE,
					TRUE, &AddWordToDictCallback, dlg->hwnd);
			}
			else {
				// TODO Assert that text hasn't changed
				ReplaceWord(dlg, data.pos, data.suggestions[selection].c_str());
				dlg->lang->autoReplace->add(data.word, data.suggestions[selection].c_str());
				ret = TRUE;
			}
		}
	}

	if (ret) {
		KillTimer(dlg->hwnd, TIMER_ID);
		SetTimer(dlg->hwnd, TIMER_ID, 100, nullptr);

		dlg->changed = TRUE;
	}

	FreePopupData(dlg);
	return ret;
}

int MsgWindowPopup(WPARAM, LPARAM lParam)
{
	MessageWindowPopupData *mwpd = (MessageWindowPopupData *)lParam;
	if (mwpd == nullptr || mwpd->uFlags != MSG_WINDOWPOPUP_INPUT)
		return 0;

	DialogMapType::iterator dlgit = dialogs.find(mwpd->hwnd);
	if (dlgit == dialogs.end())
		return -1;

	Dialog *dlg = dlgit->second;

	POINT pt = mwpd->pt;
	ScreenToClient(dlg->hwnd, &pt);

	if (mwpd->uType == MSG_WINDOWPOPUP_SHOWING)
		AddItemsToMenu(dlg, mwpd->hMenu, pt, dlg->hwnd_owner);
	else if (mwpd->uType == MSG_WINDOWPOPUP_SELECTED)
		HandleMenuSelection(dlg, mwpd->selection);

	return 0;
}

INT_PTR ShowPopupMenuService(WPARAM wParam, LPARAM)
{
	SPELLCHECKER_POPUPMENU *scp = (SPELLCHECKER_POPUPMENU *)wParam;
	if (scp == nullptr || scp->cbSize != sizeof(SPELLCHECKER_POPUPMENU))
		return -1;

	return ShowPopupMenu(scp->hwnd, scp->hMenu, scp->pt, scp->hwndOwner == nullptr ? scp->hwnd : scp->hwndOwner);
}

int ShowPopupMenu(HWND hwnd, HMENU hMenu, POINT pt, HWND hwndOwner)
{
	DialogMapType::iterator dlgit = dialogs.find(hwnd);
	if (dlgit == dialogs.end())
		return -1;

	Dialog *dlg = dlgit->second;

	if (pt.x == 0xFFFF && pt.y == 0xFFFF) {
		CHARRANGE sel;
		SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);

		// Get current cursor pos
		SendMessage(hwnd, EM_POSFROMCHAR, (WPARAM)&pt, (LPARAM)sel.cpMax);
	}
	else ScreenToClient(hwnd, &pt);

	BOOL create_menu = (hMenu == nullptr);
	if (create_menu)
		hMenu = CreatePopupMenu();

	// Make menu
	AddItemsToMenu(dlg, hMenu, pt, hwndOwner);

	// Show menu
	ClientToScreen(hwnd, &pt);
	int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwndOwner, nullptr);

	// Do action
	if (HandleMenuSelection(dlg, selection))
		selection = 0;

	if (create_menu)
		DestroyMenu(hMenu);

	return selection;
}

int MsgWindowEvent(WPARAM uType, LPARAM lParam)
{
	auto *pDlg = (CSrmmBaseDialog *)lParam;

	if (uType == MSG_WINDOW_EVT_OPEN)
		AddContactTextBox(pDlg->m_hContact, pDlg->GetInput(), "DefaultSRMM", TRUE, pDlg->GetHwnd());
	else if (uType == MSG_WINDOW_EVT_CLOSING)
		RemoveContactTextBox(pDlg->GetInput());

	return 0;
}


int IconPressed(WPARAM hContact, LPARAM lParam)
{
	StatusIconClickData *sicd = (StatusIconClickData *)lParam;
	if (sicd == nullptr || mir_strcmp(sicd->szModule, MODULENAME) != 0)
		return 0;

	if (hContact == NULL)
		return 0;

	// Find the dialog
	Dialog *dlg = nullptr;
	for (DialogMapType::iterator it = dialogs.begin(); it != dialogs.end(); it++) {
		Dialog *p = it->second;
		if (p->srmm && p->hContact == hContact) {
			dlg = p;
			break;
		}
	}

	if (dlg == nullptr)
		return 0;

	if ((sicd->flags & MBCF_RIGHTBUTTON) == 0) {
		FreePopupData(dlg);

		// Show the menu
		HMENU hMenu = CreatePopupMenu();

		if (languages.getCount() > 0) {
			if (opts.use_flags) {
				menus[dlg->hwnd] = dlg;
				dlg->hwnd_menu_owner = dlg->hwnd;
				mir_subclassWindow(dlg->hwnd_menu_owner, MenuWndProc);
			}

			// First add languages
			for (int i = 0; i < languages.getCount(); i++)
				AppendMenu(hMenu, MF_STRING | (&languages[i] == dlg->lang ? MF_CHECKED : 0), LANGUAGE_MENU_ID_BASE + i, languages[i].full_name);

			InsertMenu(hMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
		}

		InsertMenu(hMenu, 0, MF_BYPOSITION, 1, TranslateT("Enable spell checking"));
		CheckMenuItem(hMenu, 1, MF_BYCOMMAND | (dlg->enabled ? MF_CHECKED : MF_UNCHECKED));

		// Show menu
		int selection = TrackPopupMenu(hMenu, TPM_RETURNCMD, sicd->clickLocation.x, sicd->clickLocation.y, 0, dlg->hwnd, nullptr);
		HandleMenuSelection(dlg, selection);
		DestroyMenu(hMenu);
	}
	else // Enable / disable
		HandleMenuSelection(dlg, 1);

	return 0;
}

LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DialogMapType::iterator dlgit = menus.find(hwnd);
	if (dlgit == menus.end())
		return -1;

	switch (msg) {
	case WM_INITMENUPOPUP:
		{
			HMENU hMenu = (HMENU)wParam;

			int count = GetMenuItemCount(hMenu);
			for (int i = 0; i < count; i++) {
				unsigned id = GetMenuItemID(hMenu, i);
				if (id < LANGUAGE_MENU_ID_BASE || id >= LANGUAGE_MENU_ID_BASE + (unsigned)languages.getCount())
					continue;

				MENUITEMINFO mii = { 0 };
				mii.cbSize = sizeof(mii);
				mii.fMask = MIIM_STATE;
				GetMenuItemInfo(hMenu, id, FALSE, &mii);

				// Make ownerdraw
				ModifyMenu(hMenu, id, mii.fState | MF_BYCOMMAND | MF_OWNERDRAW, id, nullptr);
			}
		}
		break;

	case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			if (lpdis->CtlType != ODT_MENU || lpdis->itemID < LANGUAGE_MENU_ID_BASE || lpdis->itemID >= LANGUAGE_MENU_ID_BASE + (unsigned)languages.getCount())
				break;

			int pos = lpdis->itemID - LANGUAGE_MENU_ID_BASE;

			Dictionary *dict = &languages[pos];

			COLORREF clrfore = SetTextColor(lpdis->hDC,
				GetSysColor(lpdis->itemState & ODS_SELECTED ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT));
			COLORREF clrback = SetBkColor(lpdis->hDC,
				GetSysColor(lpdis->itemState & ODS_SELECTED ? COLOR_HIGHLIGHT : COLOR_MENU));

			FillRect(lpdis->hDC, &lpdis->rcItem, GetSysColorBrush(lpdis->itemState & ODS_SELECTED ? COLOR_HIGHLIGHT : COLOR_MENU));

			RECT rc = lpdis->rcItem;
			rc.left += 2;

			// Checked?
			rc.right = rc.left + bmpChecked.bmWidth;

			if (lpdis->itemState & ODS_CHECKED) {
				rc.top = (lpdis->rcItem.bottom + lpdis->rcItem.top - bmpChecked.bmHeight) / 2;
				rc.bottom = rc.top + bmpChecked.bmHeight;

				HDC hdcTemp = CreateCompatibleDC(lpdis->hDC);
				HBITMAP oldBmp = (HBITMAP)SelectObject(hdcTemp, hCheckedBmp);

				BitBlt(lpdis->hDC, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcTemp, 0, 0, SRCCOPY);

				SelectObject(hdcTemp, oldBmp);
				DeleteDC(hdcTemp);
			}

			rc.left += bmpChecked.bmWidth + 2;

			// Draw icon
			if (dict->hIcolib) {
				HICON hFlag = IcoLib_GetIconByHandle(dict->hIcolib);

				rc.top = (lpdis->rcItem.bottom + lpdis->rcItem.top - ICON_SIZE) / 2;
				DrawIconEx(lpdis->hDC, rc.left, rc.top, hFlag, 16, 16, 0, nullptr, DI_NORMAL);

				IcoLib_ReleaseIcon(hFlag);

				rc.left += ICON_SIZE + 4;
			}

			// Draw text
			RECT rc_text = { 0, 0, 0xFFFF, 0xFFFF };
			DrawText(lpdis->hDC, dict->full_name, -1, &rc_text, DT_END_ELLIPSIS | DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT);

			rc.right = lpdis->rcItem.right - 2;
			rc.top = (lpdis->rcItem.bottom + lpdis->rcItem.top - (rc_text.bottom - rc_text.top)) / 2;
			rc.bottom = rc.top + rc_text.bottom - rc_text.top;
			DrawText(lpdis->hDC, dict->full_name, -1, &rc, DT_END_ELLIPSIS | DT_NOPREFIX | DT_LEFT | DT_TOP | DT_SINGLELINE);

			// Restore old colors
			SetTextColor(lpdis->hDC, clrfore);
			SetBkColor(lpdis->hDC, clrback);
		}
		return TRUE;

	case WM_MEASUREITEM:
		LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
		if (lpmis->CtlType != ODT_MENU || lpmis->itemID < LANGUAGE_MENU_ID_BASE || lpmis->itemID >= LANGUAGE_MENU_ID_BASE + (unsigned)languages.getCount())
			break;

		int pos = lpmis->itemID - LANGUAGE_MENU_ID_BASE;

		Dictionary *dict = &languages[pos];

		HDC hdc = GetDC(hwnd);

		NONCLIENTMETRICS info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0);
		HFONT hFont = CreateFontIndirect(&info.lfMenuFont);
		HFONT hFontOld = (HFONT)SelectObject(hdc, hFont);

		RECT rc = { 0, 0, 0xFFFF, 0xFFFF };

		DrawText(hdc, dict->full_name, -1, &rc, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_TOP | DT_CALCRECT);

		lpmis->itemHeight = max(ICON_SIZE, max(bmpChecked.bmHeight, rc.bottom));
		lpmis->itemWidth = 2 + bmpChecked.bmWidth + 2 + ICON_SIZE + 4 + rc.right + 2;

		SelectObject(hdc, hFontOld);
		DeleteObject(hFont);
		ReleaseDC(hwnd, hdc);

		return TRUE;
	}

	return mir_callNextSubclass(hwnd, MenuWndProc, msg, wParam, lParam);
}

wchar_t* lstrtrim(wchar_t *str)
{
	int len = (int)mir_wstrlen(str);

	int i;
	for (i = len - 1; i >= 0 && (str[i] == ' ' || str[i] == '\t'); --i);
	if (i < len - 1) {
		++i;
		str[i] = '\0';
		len = i;
	}

	for (i = 0; i < len && (str[i] == ' ' || str[i] == '\t'); ++i);
	if (i > 0)
		memmove(str, &str[i], (len - i + 1) * sizeof(wchar_t));

	return str;
}
