// Copyright © 2014 CCP ehf.
#pragma once
#ifndef WrapPointList_H
#define WrapPointList_H

#include "localization.h"

#if defined( __linux__ )
#include <unicode/locid.h>
#include <unicode/unistr.h>
#endif

#ifdef _WIN32
// -------------------------------------------------------------
// Description:
//   Provides extended information from the script analysis.
// -------------------------------------------------------------
struct ItemRun
{
	SCRIPT_ANALYSIS		analysis; // http://msdn.microsoft.com/en-us/library/dd368797.aspx
	int					charPos;
	int					len;      // length of run in WCHARs
};
#endif

// -------------------------------------------------------------
// Description:
//   A Collator performs locale-sensitive string comparisons.
// -------------------------------------------------------------
BLUE_CLASS( WrapPointList) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

#ifdef _WIN32
	WrapPointList( IRoot* lockobj = 0 );
	~WrapPointList();

	const SCRIPT_LOGATTR& operator[]( const size_t index ) const
	{
		return m_wrapPointList[index];
	}
	size_t Size() const
	{
		return m_wrapPointListCount;
	}
#elif defined(__APPLE__)
    CFStringRef GetCoreString() const
    {
        return m_coreString;
    }
    CFLocaleRef GetCoreLocale() const
    {
        return m_coreLocale;
    }
#elif defined( __linux__ )
	const icu::UnicodeString& GetText() const
	{
		return m_text;
	}
	const icu::Locale& GetLocale() const
	{
		return m_locale;
	}
#endif

	friend PyObject* Py__init__(PyObject *self, PyObject *args);

private:
#ifdef _WIN32
	void MergeSimpleScripts();
	void BuildItemRunList();

	bool TextAnalyze( WCHAR* wstr, size_t wlen, SCRIPT_CONTROL* scriptControl, SCRIPT_STATE* scriptState );

	SCRIPT_LOGATTR* m_wrapPointList;
	size_t m_wrapPointListCount;

	// The itemRun list that will be converted into the WrapPointList
	int      m_itemRunCount;
	ItemRun* m_itemRunList;

	// List of script items, will be converted into an ItemRun list
	SCRIPT_ITEM* m_tempItemList;
	int m_tempItemCount;
	int m_tempItemAllocLen;
#elif defined(__APPLE__)
    AutoReleaseCF<CFStringRef> m_coreString;
    AutoReleaseCF<CFLocaleRef> m_coreLocale;
#elif defined( __linux__ )
	icu::UnicodeString m_text;
	icu::Locale m_locale;
#endif
};
TYPEDEF_BLUECLASS( WrapPointList );

#endif // WrapPointList_H
