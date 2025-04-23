/*
Copyright (c) 2005 Victor Pavlychko (nullbyte@sotline.net.ua)
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

int g_iPixelY;

wchar_t *weekDays[7] = { LPGENW("Sunday"), LPGENW("Monday"), LPGENW("Tuesday"), LPGENW("Wednesday"), LPGENW("Thursday"), LPGENW("Friday"), LPGENW("Saturday") };

wchar_t *months[12] =
{
	LPGENW("January"), LPGENW("February"), LPGENW("March"), LPGENW("April"), LPGENW("May"), LPGENW("June"),
	LPGENW("July"), LPGENW("August"), LPGENW("September"), LPGENW("October"), LPGENW("November"), LPGENW("December")
};

///////////////////////////////////////////////////////////////////////////////
// color table

struct
{
	const wchar_t *pwszName;
	uint32_t iValue;
}
static builtinColors[] = {
	{ L"black",  RGB(0, 0, 0) },
	{ L"navy",   RGB(0, 0, 128) },
	{ L"blue",   RGB(0, 0, 255) },
	{ L"green",  RGB(0, 128, 0) },
	{ L"lime",   RGB(0, 255, 0) },
	{ L"red",    RGB(255, 0, 0) },
	{ L"maroon", RGB(128, 0, 0) },
	{ L"purple", RGB(128, 0, 128) },
	{ L"pink",   RGB(255, 0, 255) },
	{ L"olive",  RGB(128, 128, 0) },
	{ L"yellow", RGB(255, 255, 0) },
	{ L"cyan",   RGB(0, 128, 128) },
	{ L"aqua",   RGB(0, 255, 255) },
	{ L"gray",   RGB(128, 128, 128) },
	{ L"white",  RGB(255, 255, 255) },
	{ L"silver", RGB(192, 192, 192) },
};

static uint32_t color2html(COLORREF clr)
{
	return (((clr & 0xFF) << 16) | (clr & 0xFF00) | ((clr & 0xFF0000) >> 16));
}

static int str2color(const CMStringW &str)
{
	for (auto &it : builtinColors)
		if (str == it.pwszName)
			return it.iValue;

	// 6 hex digits in the RGB format
	if (str.GetLength() != 6)
		return -1;

	for (int i = 0; i < 6; i++)
		if (!is_hex_digit(str[i]))
			return -1;

	return color2html(wcstoul(str, 0, 16));
}

///////////////////////////////////////////////////////////////////////////////
// HTML generator

static wchar_t* font2html(LOGFONTA &lf, wchar_t *dest)
{
	mir_snwprintf(dest, 100, L"font-family: %S; font-size: %dpt; font-weight: %s %s", 
		lf.lfFaceName, abs((signed char)lf.lfHeight) * 74 / g_iPixelY,
		lf.lfWeight >= FW_BOLD ? L"bold" : L"normal",
		lf.lfItalic ? L"; font-style: italic;" : L"");
	return dest;
}

static void AppendImage(CMStringW &buf, const CMStringW &wszUrl, const CMStringW &wszDescr, ItemData *pItem, UINT uMaxHeight)
{
	if (g_plugin.bShowPreview) {
		pItem->pOwner->webPage.load_image(wszUrl, pItem);

		int iHeight = uMaxHeight;
		if (auto *pImage = pItem->pOwner->webPage.find_image(wszUrl)) {
			if (pImage->GetHeight() < uMaxHeight)
				iHeight = pImage->GetHeight();

			buf.AppendFormat(L"<img style=\"height: %dpx;\" src=\"%s\"/><br>", iHeight, wszUrl.c_str());
		}
		else buf.AppendFormat(L"<img src=\"%s\"/><br>", wszUrl.c_str());
	}
	else buf.AppendFormat(L"<a class=\"link\" href=\"%s\">%s</a>", wszUrl.c_str(), wszDescr.c_str());
}

static void AppendString(CMStringW &buf, const wchar_t *p, ItemData *pItem)
{
	bool wasSpace = false;

	for (; *p; p++) {
		if (*p == ' ') {
			if (wasSpace)
				buf.Append(L"&nbsp;");
			else {
				buf.AppendChar(' ');
				wasSpace = true;
			}
			continue;
		}

		wasSpace = false;
		if (*p == '\r' && p[1] == '\n') {
			buf.Append(L"<br>");
			p++;
		}
		else if (*p == '\n') buf.Append(L"<br>");
		else if (*p == '\"') buf.Append(L"&quot;");
		else if (*p == '&') buf.Append(L"&amp;");
		else if (*p == '>') buf.Append(L"&gt;");
		else if (*p == '<') buf.Append(L"&lt;");
		else if (*p == '%') buf.Append(L"&percnt;");
		else if (*p == '[') {
			p++;
			if (*p == 'c') {
				int colorId = -1;
				if (p[2] == ']') {
					colorId = _wtoi(p + 1);
					p += 2;
				}
				else if (p[3] == ']') {
					colorId = _wtoi(p + 1);
					p += 3;
				}

				switch (colorId) {
				case 0: buf.Append(L"</font>"); continue;
				case 1: buf.Append(L"<font class=\"nick\">"); continue;
				case 2: case 3: case 4: case 5: case 6:
					buf.AppendFormat(L"<font color=#%06X>", color2html(g_plugin.clCustom[colorId-2]));
					continue;
				}
			}

			wchar_t *pEnd = L"";
			if (*p == '/') {
				pEnd = L"/";
				p++;
			}
			if (*p == 'b' && p[1] == ']') {
				buf.AppendFormat(L"<%sb>", pEnd);
				p++;
			}
			else if (*p == 'i' && p[1] == ']') {
				buf.AppendFormat(L"<%si>", pEnd);
				p++;
			}
			else if (*p == 'u' && p[1] == ']') {
				buf.AppendFormat(L"<%su>", pEnd);
				p++;
			}
			else if (*p == 's' && p[1] == ']') {
				buf.AppendFormat(L"<%ss>", pEnd);
				p++;
			}
			else if (!wcsncmp(p, L"img=", 4)) {
				p += 4;

				int iMaxHeight = 0;
				const wchar_t * pHeightBegin = nullptr;
				if (pHeightBegin = wcsstr(p, L" height=")) {
					auto* p1 = pHeightBegin + 8;
					if (auto* p2 = wcschr(p1, ']')) {
						CMStringW wszHeight(p1, int(p2 - p1));
						iMaxHeight = _wtoi(wszHeight);
					}
				}

				if (auto *p1 = wcschr(p, ']')) {
					CMStringW wszUrl(p, int((pHeightBegin ? pHeightBegin : p1) - p));
					p1++;

					if (auto *p2 = wcsstr(p1, L"[/img]")) {
						CMStringW wszDescr(p1, int(p2 - p1));
						AppendImage(buf, wszUrl, wszDescr, pItem, iMaxHeight ? iMaxHeight : g_iPreviewHeight);
						p = p2 + 5;
					}
				}
				else p--;
			}
			else if (!wcsncmp(p, L"img]", 4)) {
				p += 4;

				if (auto *p1 = wcsstr(p, L"[/img]")) {
					CMStringW wszUrl(p, int(p1 - p));
					AppendImage(buf, wszUrl, L"", pItem, g_iPreviewHeight);
					p = p1 + 5;
				}
				else p--;
			}
			else if (!wcsncmp(p, L"url=", 4)) {
				p += 4;

				if (auto *p1 = wcschr(p, ']')) {
					CMStringW wszUrl(p, int(p1 - p));
					p1++;

					if (auto *p2 = wcsstr(p1, L"[/url]")) {
						CMStringW wszDescr(p1, int(p2 - p1));
						buf.AppendFormat(L"<a class=\"link\" href=\"%s\">", wszUrl.c_str());
						AppendString(buf, wszDescr, pItem);
						buf.Append(L"</a>");
						p = p2 + 5;
					}
				}
				else p--;
			}
			else if (!wcsncmp(p, L"url]", 4)) {
				p += 4;

				if (auto *p1 = wcsstr(p, L"[/url]")) {
					CMStringW wszUrl(p, int(p1 - p));
					buf.AppendFormat(L"<a class=\"link\" href=\"%s\">%s</a>", wszUrl.c_str(), wszUrl.c_str());
					p = p1 + 5;
				}
				else p--;
			}
			else if (!wcsncmp(p, L"color=", 6)) {
				p += 6;

				if (auto *p1 = wcschr(p, ']')) {
					int iColor = str2color(CMStringW(p, int(p1 - p)));
					if (iColor != -1)
						buf.AppendFormat(L"<font color=#%06X>", color2html(iColor));
					else
						buf.Append(L"<font>");
					p = p1;
				}
				else p--;
			}
			else if (*pEnd && !wcsncmp(p, L"color]", 6)) {
				p += 5;
				buf.AppendFormat(L"</font>");
			}
			else if (!wcsncmp(p, L"code]", 5)) {
				p += 4;
				buf.AppendFormat(*pEnd ? L"</pre>" : L"<pre>");
			}
			else if (!wcsncmp(p, L"quote]", 6)) {
				p += 5;
				buf.AppendFormat(*pEnd ? L"</div>" : L"<div class=\"quote\">" );
			}
			else {
				buf.AppendChar('[');
				if (*pEnd == '/')
					p--;
				p--;
			}
		}
		else buf.AppendChar(*p);
	}
}

CMStringW ItemData::formatHtml(const wchar_t *pwszStr)
{
	CMStringW str;
	str.Append(L"<html><head>");
	str.Append(L"<style type=\"text/css\">\n");

	int fontID, colorID;
	getFontColor(fontID, colorID);
	auto &F = g_fontTable[fontID];

	wchar_t szFont[100];
	str.AppendFormat(L"body {margin: 0px; text-align: left; %s; color: NSText; overflow: auto;}\n", font2html(F.lf, szFont));
	str.AppendFormat(L".nick {color: #%06X }\n", color2html(g_colorTable[dbe.bSent ? COLOR_OUTNICK : COLOR_INNICK].cl));
	str.AppendFormat(L".link {color: #%06X }\n", color2html(g_colorTable[COLOR_LINK].cl));
	str.AppendFormat(L".quote {border-left: 4px solid #%06X; padding-left: 8px; }\n", color2html(g_colorTable[COLOR_QUOTE].cl));

	str.Append(L"</style></head><body class=\"body\">\n");

	if (qtext) {
		str.Append(L"<div class=\"quote\">");
		AppendString(str, qtext, this);
		str.Append(L"</div>\n");
	}

	CMStringW wszOrigText((pwszStr) ? pwszStr : formatString());

	if (dbe.flags & DBEF_JSON) {
		wszOrigText.Append(L"\r\n");
			
		auto &json = dbe.getJson();
		for (auto &it : json["r"])
			wszOrigText.AppendFormat(L"%s: %d ", Utf2T(it.name()).get(), it.as_int());
	}

	SMADD_BATCHPARSE sp = {};
	SMADD_BATCHPARSERES *spRes = nullptr;
	if (g_plugin.bHasSmileys) {
		sp.Protocolname = Proto_GetBaseAccountName(dbe.hContact);
		sp.flag = SAFL_PATH | SAFL_UNICODE;
		sp.str.w = wszOrigText;
		sp.hContact = dbe.hContact;
		spRes = (SMADD_BATCHPARSERES *)CallService(MS_SMILEYADD_BATCHPARSE, 0, (LPARAM)&sp);
	}

	CMStringW szBody(wszOrigText);
	UrlAutodetect(szBody);
	AppendString(str, szBody, this);
	if (spRes) {
		int iOffset = 0;
		for (int i = 0; i < (int)sp.numSmileys; i++) {
			auto &smiley = spRes[i];
			if (!mir_wstrlen(smiley.filepath))
				continue;

			CMStringW wszFound(wszOrigText.Mid(smiley.startChar, smiley.size));
			int idx = str.Find(wszFound, iOffset);
			if (idx == -1)
				continue;

			str.Delete(idx, smiley.size);

			CMStringW wszNew(FORMAT, L"<img class=\"img\" src=\"file://%s\" title=\"%s\" alt=\"%s\" />", smiley.filepath, wszFound.c_str(), wszFound.c_str());
			str.Insert(idx, wszNew);
			iOffset = idx + wszNew.GetLength();
		}

		CallService(MS_SMILEYADD_BATCHFREE, 0, (LPARAM)spRes);
	}

	str.Append(L"</body></html>");
	
	// Netlib_LogfW(0, str);
	return str;
}

///////////////////////////////////////////////////////////////////////////////
// RTF generator

static void AppendUnicodeToBuffer(CMStringA &buf, const wchar_t *p)
{
	for (; *p; p++) {
		if (*p == '\r' && p[1] == '\n') {
			buf.Append("\\par ");
			p++;
		}
		else if (*p == '\n') {
			buf.Append("\\par ");
		}
		else if (*p == '\t') {
			buf.Append("\\tab ");
		}
		else if (*p == '\\' || *p == '{' || *p == '}') {
			buf.AppendChar('\\');
			buf.AppendChar((char)*p);
		}
		else if (*p == '[') {
			p++;
			if (*p == 'c') {
				if (p[2] == ']') {
					buf.AppendFormat("\\cf%c ", p[1]);
					p += 2;
					continue;
				}
				if (p[3] == ']') {
					buf.AppendFormat("\\cf%d ", _wtoi(p + 1));
					p += 3;
					continue;
				}
			}

			char *pEnd = "";
			if (*p == '/') {
				pEnd = "0";
				p++;
			}
			if (*p == 'b' && p[1] == ']') {
				buf.AppendFormat("\\b%s ", pEnd);
				p++;
			}
			else if (*p == 'i' && p[1] == ']') {
				buf.AppendFormat("\\i%s ", pEnd);
				p++;
			}
			else if (*p == 'u' && p[1] == ']') {
				buf.AppendFormat("\\ul%s ", pEnd);
				p++;
			}
			else if (*p == 's' && p[1] == ']') {
				buf.AppendFormat("\\strike%s ", pEnd);
				p++;
			}
			else {
				buf.AppendChar('[');
				if (*pEnd == '0')
					p--;
				p--;
			}
		}
		else if (*p < 128) {
			buf.AppendChar((char)*p);
		}
		else {
			buf.AppendFormat("\\u%d ?", *p);
		}
	}
}

CMStringA NewstoryListData::GatherSelectedRtf()
{
	CMStringA buf;
	buf.Append("{\\rtf1\\ansi\\deff0{\\fonttbl ");
	for (int i=0; i < FONT_COUNT; i++)
		buf.AppendFormat("{\\f%d\\fnil\\fcharset0 %s;}", i, g_fontTable[i].lf.lfFaceName);

	buf.AppendFormat("}{\\colortbl \\red%u\\green%u\\blue%u;\\red%u\\green%u\\blue%u;", 0, 0, 0, 0, 0, 0);

	for (auto cl : g_plugin.clCustom) {
		COLORREF cr = (cl == -1) ? 0 : cl;
		buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(cr), GetGValue(cr), GetBValue(cr));
	}

	for (int i = 0; i < COLOR_COUNT; i++) {
		COLORREF cr = g_colorTable[i].cl;
		buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(cr), GetGValue(cr), GetBValue(cr));
	}

	for (int i = 0; i < FONT_COUNT; i++) {
		COLORREF cr = g_fontTable[i].cl;
		buf.AppendFormat("\\red%u\\green%u\\blue%u;", GetRValue(cr), GetGValue(cr), GetBValue(cr));
	}

	buf.Append("}");

	int eventCount = totalCount;
	for (int i = 0; i < eventCount; i++) {
		ItemData *p = GetItem(i);
		if (!p->m_bSelected)
			continue;

		int fontID, colorID;
		p->getFontColor(fontID, colorID);

		buf.AppendFormat("{\\uc1\\pard \\cb%d\\cf%d\\f%d\\b0\\i0\\fs%d ", COLOR_BACK + 7, colorID+COLOR_COUNT+7, fontID, GetFontHeight(g_fontTable[fontID].lf));
		CMStringW wszText(p->formatString());
		wszText.Replace(L"[c0]", CMStringW(FORMAT, L"[c%d]", colorID + COLOR_COUNT + 7));
		wszText.Replace(L"[c1]", CMStringW(FORMAT, L"[c%d]", 7 + (p->dbe.bSent ? COLOR_OUTNICK : COLOR_INNICK)));
		AppendUnicodeToBuffer(buf, wszText);
		buf.Append("\\par }");
	}

	buf.Append("}");
	return buf;
}

///////////////////////////////////////////////////////////////////////////////
// Template formatting for the control

CMStringW TplFormatString(int tpl, MCONTACT hContact, ItemData *item)
{
	if (tpl < 0 || tpl >= TPL_COUNT)
		return CMStringW();

	auto &T = templates[tpl];
	wchar_t *pValue = TranslateW((T.value) ? T.value : T.defvalue);

	TemplateVars vars;
	for (auto &it : T.vf)
		if (it)
			it(&vars, hContact, item);

	CMStringW buf;
	for (wchar_t *p = pValue; *p; p++) {
		if (*p == '%') {
			wchar_t *var = vars.GetVar((p[1] & 0xff));
			if (var)
				buf.Append(var);

			p++;
		}
		else buf.AppendChar(*p);
	}

	return buf;
}

///////////////////////////////////////////////////////////////////////////////
// Template formatting for options dialog

CMStringW ItemData::formatStringEx(wchar_t *sztpl)
{
	CMStringW buf;
	int tpl = getTemplate();
	if (tpl < 0 || tpl >= TPL_COUNT || !sztpl)
		return buf;

	TemplateVars vars;

	auto &T = templates[tpl];
	for (auto &it : T.vf)
		if (it)
			it(&vars, dbe.hContact, this);

	for (wchar_t *p = sztpl; *p; p++) {
		if (*p == '%') {
			wchar_t *var = vars.GetVar((p[1] & 0xff));
			if (var)
				buf.Append(var);
			p++;
		}
		else buf.AppendChar(*p);
	}

	return buf;
}

///////////////////////////////////////////////////////////////////////////////
// TemplateVars members

TemplateVars::TemplateVars()
{
	memset(&vars, 0, sizeof(vars));
}

TemplateVars::~TemplateVars()
{
	for (auto &V : vars)
		if (V.val && V.del)
			mir_free(V.val);
}

// Loading variables
void vfGlobal(TemplateVars *vars, MCONTACT hContact, ItemData *)
{
	//  %%: simply % character
	vars->SetVar('%', L"%", false);

	//  %n: line break
	vars->SetVar('n', L"\x0d\x0a", false);

	// %S: my nick (not for messages)
	char* proto = Proto_GetBaseAccountName(hContact);
	ptrW nick(Contact::GetInfo(CNF_DISPLAY, 0, proto));
	vars->SetVar('S', nick, true);
}

void vfContact(TemplateVars *vars, MCONTACT hContact, ItemData *pItem)
{
	// %N: buddy's nick (not for messages)
	wchar_t *nick = (hContact == 0) ? TranslateT("System history") : Clist_GetContactDisplayName(hContact, 0);
	vars->SetNick(nick, pItem);

	wchar_t buf[20];
	// %c: event count
	mir_snwprintf(buf, L"%d", db_event_count(hContact));
	vars->SetVar('c', buf, true);
}

void vfSystem(TemplateVars *vars, MCONTACT hContact, ItemData *pItem)
{
	// %N: buddy's nick (not for messages)
	vars->SetNick(TranslateT("System event"), pItem);

	// %c: event count
	wchar_t  buf[20];
	mir_snwprintf(buf, L"%d", db_event_count(hContact));
	vars->SetVar('c', buf, true);
}

void vfEvent(TemplateVars *vars, MCONTACT, ItemData *pItem)
{
	wchar_t buf[100];

	//  %N: Nickname
	if (!pItem->m_bIsResult && pItem->dbe.bSent) {
		if (!pItem->wszNick) {
			char *proto = Proto_GetBaseAccountName(pItem->dbe.hContact);
			ptrW nick(Contact::GetInfo(CNF_DISPLAY, 0, proto));
			vars->SetNick(nick, pItem);
		}
		else vars->SetNick(pItem->wszNick, pItem);
	}
	else {
		wchar_t *nick = (pItem->wszNick) ? pItem->wszNick : Clist_GetContactDisplayName(pItem->dbe.hContact, 0);
		vars->SetNick(nick, pItem);
	}

	// %D: direction symbol
	if (pItem->dbe.bSent)
		vars->SetVar('D', L"<<", false);
	else
		vars->SetVar('D', L">>", false);

	//  %t: timestamp
	SYSTEMTIME st;
	if (!TimeZone_GetSystemTime(nullptr, pItem->dbe.getUnixtime(), &st, 0)) {
		int iLocale = Langpack_GetDefaultLocale();

		GetDateFormatW(iLocale, 0, &st, L"dd.MM.yyyy, ", buf, _countof(buf));
		GetTimeFormatW(iLocale, 0, &st, L"HH:mm", buf + 12, _countof(buf));
		vars->SetVar('t', buf, true);

		//  %h: hour (24 hour format, 0-23)
		GetTimeFormatW(iLocale, 0, &st, L"HH", buf, _countof(buf));
		vars->SetVar('h', buf, true);

		//  %a: hour (12 hour format)
		GetTimeFormatW(iLocale, 0, &st, L"hh", buf, _countof(buf));
		vars->SetVar('a', buf, true);

		//  %m: minute
		GetTimeFormatW(iLocale, 0, &st, L"mm", buf, _countof(buf));
		vars->SetVar('m', buf, true);

		//  %s: second
		GetTimeFormatW(iLocale, 0, &st, L"ss", buf, _countof(buf));
		vars->SetVar('s', buf, true);

		//  %o: month
		GetDateFormatW(iLocale, 0, &st, L"MM", buf, _countof(buf));
		vars->SetVar('o', buf, true);

		//  %d: day of month
		GetDateFormatW(iLocale, 0, &st, L"dd", buf, _countof(buf));
		vars->SetVar('d', buf, true);

		//  %y: year
		GetDateFormatW(iLocale, 0, &st, L"yyyy", buf, _countof(buf));
		vars->SetVar('y', buf, true);

		//  %w: day of week (Sunday, Monday... translatable)
		vars->SetVar('w', TranslateW(weekDays[st.wDayOfWeek]), false);

		//  %p: AM/PM symbol
		vars->SetVar('p', (st.wHour > 11) ? L"PM" : L"AM", false);

		//  %O: Name of month, translatable
		vars->SetVar('O', TranslateW(months[st.wMonth-1]), false);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// %M: the message string itself

void vfMessage(TemplateVars *vars, MCONTACT, ItemData *item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfFile(TemplateVars *vars, MCONTACT, ItemData *item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfSign(TemplateVars *vars, MCONTACT, ItemData *item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfAuth(TemplateVars *vars, MCONTACT, ItemData *item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfAdded(TemplateVars *vars, MCONTACT, ItemData *item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfPresence(TemplateVars* vars, MCONTACT, ItemData* item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfDeleted(TemplateVars *vars, MCONTACT, ItemData *item)
{
	vars->SetVar('M', item->getWBuf(), false);
}

void vfOther(TemplateVars *vars, MCONTACT, ItemData *item)
{
	auto *pText = item->getWBuf();
	vars->SetVar('M', mir_wstrlen(pText) == 0 ? TranslateT("Unknown event") : pText, false);
}

/////////////////////////////////////////////////////////////////////////////////////////

void TemplateVars::SetNick(wchar_t *v, ItemData *pItem)
{
	CMStringW wszNick;
	if (pItem)
		wszNick.Format(L"[c1]%s[c0]", v);
	else
		wszNick = v;

	auto &V = vars['N'];
	if (V.del)
		mir_free(V.val);
	V.val = wszNick.Detach();
	V.del = true;
}

void TemplateVars::SetVar(uint8_t id, wchar_t *v, bool d)
{
	auto &V = vars[id];
	if (V.del)
		mir_free(V.val);

	V.val = (d) ? mir_wstrdup(v) : v;
	V.del = d;
}

/////////////////////////////////////////////////////////////////////////////////////////

HICON TemplateInfo::getIcon() const
{
	return (iIcon < 0) ? Skin_LoadIcon(-iIcon) : g_plugin.getIcon(iIcon);
}

TemplateInfo templates[TPL_COUNT] =
{
	{ "tpl/interface/title", LPGENW("Interface"), IDI_NEWSTORY, LPGENW("Window title"),
		LPGENW("%N - history [%c messages total]"), 0, 0,
		{ vfGlobal, vfContact, 0, 0, 0 } },

	{ "tpl/msglog/msg", LPGENW("Message log"), IDI_SENDMSG, LPGENW("Messages"),
		L"[b]%N, %t:[/b]\r\n%M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfMessage, 0 } },
	{ "tpl/msglog/msg_head", LPGENW("Message log"), IDI_SENDMSG, LPGENW("Group head"),
		L"[b]%N, %t:[/b] %M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfMessage, 0 } },
	{ "tpl/msglog/msg_grp", LPGENW("Message log"), IDI_SENDMSG, LPGENW("Grouped messages"),
		L"[b]%h:%m:%s:[/b] %M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfMessage, 0 } },
	{ "tpl/msglog/file", LPGENW("Message log"), -SKINICON_EVENT_FILE, LPGENW("Files"),
		L"[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfFile, 0 } },
	{ "tpl/msglog/status", LPGENW("Message log"), IDI_SIGNIN, LPGENW("Status changes"),
		L"[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfSign, 0 } },
	{ "tpl/msglog/presense", LPGENW("Message log"), IDI_UNKNOWN, LPGENW("Presence requests"),
		L"%B[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfPresence, 0 } },
	{ "tpl/msglog/other", LPGENW("Message log"), IDI_UNKNOWN, LPGENW("Other events"),
		L"%B[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfOther, 0 } },

	{ "tpl/msglog/authrq", LPGENW("Message log"), IDI_UNKNOWN, LPGENW("Authorization requests"),
		L"[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfEvent, vfSystem, vfAuth, 0 } },
	{ "tpl/msglog/added", LPGENW("Message log"), IDI_UNKNOWN, LPGENW("'You were added' events"),
		L"[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfEvent, vfSystem, vfAdded, 0 } },
	{ "tpl/msglog/deleted", LPGENW("Message log"), IDI_UNKNOWN, LPGENW("'You were deleted' events"),
		L"[b]%N, %t:[/b]%n%M", 0, 0,
		{ vfGlobal, vfEvent, vfSystem, vfDeleted, 0 } },

	{ "tpl/copy/msg", LPGENW("Clipboard"), IDI_SENDMSG, LPGENW("Messages"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfMessage, 0 } },
	{ "tpl/copy/file", LPGENW("Clipboard"), -SKINICON_EVENT_FILE, LPGENW("Files"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfFile, 0 } },
	{ "tpl/copy/status", LPGENW("Clipboard"), IDI_SIGNIN, LPGENW("Status changes"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfSign, 0 } },
	{ "tpl/copy/presence", LPGENW("Clipboard"), IDI_UNKNOWN, LPGENW("Presence requests"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfPresence, 0 } },
	{ "tpl/copy/other", LPGENW("Clipboard"), IDI_UNKNOWN, LPGENW("Other events"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfContact, vfEvent, vfOther, 0 } },

	{ "tpl/copy/authrq", LPGENW("Clipboard"), IDI_UNKNOWN, LPGENW("Authorization requests"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfEvent, vfSystem, vfAuth, 0 } },
	{ "tpl/copy/added", LPGENW("Clipboard"), IDI_UNKNOWN, LPGENW("'You were added' events"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfEvent, vfSystem, vfAdded, 0 } },
	{ "tpl/copy/deleted", LPGENW("Clipboard"), IDI_UNKNOWN, LPGENW("'You were deleted' events"),
		L"%N, %t:\x0d\x0a%M%n", 0, 0,
		{ vfGlobal, vfEvent, vfSystem, vfDeleted, 0 } }
};

void LoadTemplates()
{
	HDC hdc = GetDC(nullptr);
	g_iPixelY = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(nullptr, hdc);	

	for (auto &it : templates)
		replaceStrW(it.value, g_plugin.getWStringA(it.setting));
}

void SaveTemplates()
{
	for (auto &it : templates) {
		if (it.value) {
			if (mir_wstrcmp(it.value, it.defvalue))
				g_plugin.setWString(it.setting, it.value);
			else
				g_plugin.delSetting(it.setting);
		}
		else g_plugin.delSetting(it.setting);
	}
}
