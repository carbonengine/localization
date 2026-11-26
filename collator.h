// Copyright © 2014 CCP ehf.
#pragma once
#ifndef Collator_H
#define Collator_H

#include "localization.h"
#include <locale>

#if __APPLE__
#import "Foundation/NSLocale.h"
#elif defined( __linux__ )
#include <memory>
#include <unicode/coll.h>
#endif

// -------------------------------------------------------------
// Description:
//   A Collator performs locale-sensitive string comparisons.
// -------------------------------------------------------------
BLUE_CLASS( Collator ) : public IRoot
{
public:
	// DOM-IGNORE-BEGIN
	// Don't use this constructor from Python, unless you explicitly set the locale afterwards.
	Collator( IRoot* lockobj = 0 ) {}
	// DOM-IGNORE-END

	Collator( const std::string& languageCode, IRoot* lockobj = 0 );

	EXPOSE_TO_BLUE();

	int Compare( const std::wstring& source, const std::wstring& target ) const;

	// -------------------------------------------------------------
	// Description:
	//   Get information about the locale that is being used.
	// Return Value:
	//   Current locale.
	// -------------------------------------------------------------
	std::string GetLocale() const { return LanguageIDToCode( m_localeID ); }

	void SetLocale( const std::string& languageCode );

private:
    // The locale that is supposed to be used for this collator
    LanguageID m_localeID;

#if __APPLE__
	NSLocale* m_locale = nullptr;
	NSString* m_localeName = nullptr;
#elif defined( __linux__ )
	// ICU collator configured to match the macOS implementation
	// (case-insensitive, numeric-aware, locale-sensitive comparison).
	std::shared_ptr<icu::Collator> m_collator;
#else
    std::locale m_locale;
#endif
};
TYPEDEF_BLUECLASS( Collator );

#endif // Collator_H
