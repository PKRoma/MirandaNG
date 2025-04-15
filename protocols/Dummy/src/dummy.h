/*
Copyright (c) 2014-17 Robert Pösel, 2017-25 Miranda NG team

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

#pragma once

struct CDummyProto;

#define DUMMY_ID_TEMPLATE "Template"
#define DUMMY_ID_TEXT "UniqueIdText"
#define DUMMY_ID_SETTING "UniqueIdSetting"
#define DUMMY_KEY_ALLOW_SENDING "AllowSending"

struct ttemplate
{
	const char *name;
	const char *setting;
	const char *text;
	int iconId;
};

extern const ttemplate templates[];

struct CMPlugin : public ACCPROTOPLUGIN<CDummyProto>
{
	CMPlugin();

	int Load() override;
};
