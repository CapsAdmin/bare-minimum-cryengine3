/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
StringUtils.cpp
*************************************************************************/

#include "StdAfx.h"
#include "StringUtils.h"

static bool s_stringUtils_assertEnabled = true;



//--------------------------------------------------------------------------------
bool cry_strncpy(char * destination, const char * source, size_t bufferLength)
{
	bool reply = false;

	CRY_ASSERT(destination);
	CRY_ASSERT(source);

	if (bufferLength)
	{
		size_t i;
		for (i = 0; source[i] && (i + 1) < bufferLength; ++ i)
		{
			destination[i] = source[i];
		}
		destination[i] = '\0';
		reply = (source[i] == '\0');
	}

	CRY_ASSERT_MESSAGE(reply || !s_stringUtils_assertEnabled, string().Format("String '%s' is too big to fit into a buffer of length %u", source, (unsigned int) bufferLength));

	return reply;
}

//--------------------------------------------------------------------------------
bool cry_wstrncpy(wchar_t * destination, const wchar_t * source, size_t bufferLength)
{
	bool reply = false;

	CRY_ASSERT(destination);
	CRY_ASSERT(source);

	if (bufferLength)
	{
		size_t i;
		for (i = 0; source[i] && (i + 1) < bufferLength; ++ i)
		{
			destination[i] = source[i];
		}
		destination[i] = '\0';
		reply = (source[i] == '\0');
	}

	CRY_ASSERT_MESSAGE(reply || !s_stringUtils_assertEnabled, string().Format("String '%ls' is too big to fit into a buffer of length %u", source, (unsigned int) bufferLength));

	return reply;
}

//--------------------------------------------------------------------------------
void cry_sprintf(char * destination, size_t size, const char* format, ...)
{
	va_list args;
	va_start(args,format);
	int  ret = vsnprintf(destination, size, format, args);
	va_end(args);
	destination[size - 1] = 0;
	CRY_ASSERT_MESSAGE((ret < (int)size), string().Format("String '%s' was truncated to fit buffer of length %u", destination, (unsigned int) size));
}

//--------------------------------------------------------------------------------
size_t cry_copyStringUntilFindChar(char * destination, const char * source, size_t bufferLength, char until)
{
	size_t reply = 0;

	CRY_ASSERT(destination);
	CRY_ASSERT(source);

	if (bufferLength)
	{
		size_t i;
		for (i = 0; source[i] && source[i] != until && (i + 1) < bufferLength; ++ i)
		{
			destination[i] = source[i];
		}
		destination[i] = '\0';
		reply = (source[i] == until) ? (i + 1) : 0;
	}

	return reply;
}

//---------------------------------------------------------------------
void StrToWstr(const char* str, wstring& dstr)
{
	CryStackStringT<wchar_t, 64> tmp;
	tmp.resize(strlen(str));
	tmp.clear();

	while (const wchar_t c=(wchar_t)(*str++))
	{
		tmp.append(1, c);
	}

	dstr.assign(tmp.data(), tmp.length());
}