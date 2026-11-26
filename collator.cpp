// Copyright © 2014 CCP ehf.
#include "stdafx.h"
#include "collator.h"

// -------------------------------------------------------------
// Description:
//   Constructs a collator instance for the given locale.
// Arguments:
//   languageCode - string representation of the locale as used in Python
// SeeAlso:
//   LanguageID
// -------------------------------------------------------------
Collator::Collator( const std::string& languageCode, IRoot* lockObj /* = 0 */ )
{
	SetLocale( languageCode );
}

#if defined( __linux__ )
// -------------------------------------------------------------
// Description:
//   The comparison function compares the character data stored in two different strings.
//   Returns information about whether a string is less than, greater than or equal to another string.
//   Uses ICU, as the glibc std::collate implementation depends on which locales happen to be
//   installed on the host system.
// Arguments:
//   source - the string to be compared with.
//   target - the string that is to be compared with the source string.
// Return Value:
//   -1 if source is lesser than target, 0 if equal and 1 if source is greater than target.
// -------------------------------------------------------------
int Collator::Compare( const std::wstring& source, const std::wstring& target ) const
{
	static_assert( sizeof( wchar_t ) == sizeof( UChar32 ), "Expected UTF-32 wchar_t on Linux" );
	const icu::UnicodeString icuSource = icu::UnicodeString::fromUTF32( reinterpret_cast<const UChar32*>( source.data() ), static_cast<int32_t>( source.size() ) );
	const icu::UnicodeString icuTarget = icu::UnicodeString::fromUTF32( reinterpret_cast<const UChar32*>( target.data() ), static_cast<int32_t>( target.size() ) );

	if( !m_collator )
	{
		const int8_t result = icuSource.compareCodePointOrder( icuTarget );
		return result < 0 ? -1 : ( result > 0 ? 1 : 0 );
	}

	UErrorCode status = U_ZERO_ERROR;
	const UCollationResult result = m_collator->compare( icuSource, icuTarget, status );
	if( U_FAILURE( status ) )
	{
		CCP_LOGWARN( "Collator failed to compare strings: %s", u_errorName( status ) );
		return 0;
	}
	return result == UCOL_LESS ? -1 : ( result == UCOL_EQUAL ? 0 : 1 );
}

// -------------------------------------------------------------
// Description:
//   Sets the locale for this collator.
// Arguments:
//   languageCode - string representation of the locale as used in Python
// -------------------------------------------------------------
void Collator::SetLocale( const std::string& languageCode )
{
	m_localeID = CodeToLanguageID( languageCode.c_str() );

	UErrorCode status = U_ZERO_ERROR;
	std::shared_ptr<icu::Collator> collator( icu::Collator::createInstance( icu::Locale( LanguageIDToLocaleName( m_localeID ) ), status ) );
	if( U_FAILURE( status ) || !collator )
	{
		CCP_LOGWARN( "Collator failed to create ICU collator for %s: %s", LanguageIDToLocaleName( m_localeID ), u_errorName( status ) );
		m_collator.reset();
		return;
	}

	// Match the macOS implementation: NSCaseInsensitiveSearch | NSNumericSearch
	collator->setStrength( icu::Collator::SECONDARY );
	collator->setAttribute( UCOL_NUMERIC_COLLATION, UCOL_ON, status );
	if( U_FAILURE( status ) )
	{
		CCP_LOGWARN( "Collator failed to enable numeric collation: %s", u_errorName( status ) );
	}
	m_collator = std::move( collator );
}
#elif !__APPLE__
// -------------------------------------------------------------
// Description:
//   The comparison function compares the character data stored in two different strings.
//   Returns information about whether a string is less than, greater than or equal to another string.
// Arguments:
//   source - the string to be compared with.
//   target - the string that is to be compared with the source string.
// Return Value:
//   -1 if source is lesser than target, 0 if equal and 1 if source is greater than target.
// -------------------------------------------------------------
int Collator::Compare( const std::wstring& source, const std::wstring& target ) const
{
    auto& col = std::use_facet<std::collate<wchar_t>>( m_locale );
    return col.compare(
       source.c_str(), source.c_str() + source.length(),
       target.c_str(), target.c_str() + target.length() );
}

// -------------------------------------------------------------
// Description:
//   Sets the locale for this collator.
// Arguments:
//   languageCode - string representation of the locale as used in Python
// -------------------------------------------------------------
void Collator::SetLocale( const std::string& languageCode )
{
    m_localeID = CodeToLanguageID( languageCode.c_str() );
    try
    {
        m_locale = std::locale( LanguageIDToLocaleName( m_localeID ) );
    }
    catch ( std::runtime_error )
    {
        m_locale = std::locale();
    }
}
#endif
