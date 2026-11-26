// Copyright © 2014 CCP ehf.
// localization.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include "localization.h"

#include "parser.h"

#if defined( __linux__ )
#include <unicode/dcfmtsym.h>
#include <unicode/locid.h>
#endif

const char* g_moduleName = "_evelocalization";
LocalizationSettings g_settings;

// -------------------------------------------------------------
// Description:
//   Constructs a language object with all information for the passed in locale
// Arguments:
//   langID - The languageID for which to retrieve locale information.
// -------------------------------------------------------------
Language::Language( LanguageID langID /*= Default_Language*/ ) : id( langID ),  quantityCategoryFunc( NULL )
{
	switch ( langID )
	{
	case LANGUAGEID_CHINESE_SIMPLIFIED:
	case LANGUAGEID_JAPANESE:
		quantityCategoryFunc = GetType1QuantityCategory;
		break;
	case LANGUAGEID_RUSSIAN:
		quantityCategoryFunc = GetType3QuantityCategory;
		break;
	default:
		quantityCategoryFunc = GetType2QuantityCategory;
		break;
	};

    try
    {
        locale = std::locale( LanguageIDToLocaleName( langID ) );
    }
    catch ( std::runtime_error )
    {
        CCP_LOGWARN( "Localization failed to initialize locale for %s. Falling back to system locale", LanguageIDToLocaleName( langID ) );
    }

	if( !SetNumberSeparators( langID ) )
	{
		//Default fallback ( English )
        decimalSep = L'.';
        thousandSep = L',';
	}

}

// -------------------------------------------------------------
// Description:
//   Sets decimalSep and thousandSep to match system region, only returns
// Arguments:
//   langID - the LanguageID.
// Return value:
//   True if decimal and thousands separator is found for langID, false otherwise.
// -------------------------------------------------------------
bool Language::SetNumberSeparators(LanguageID langID)
{
#if _WIN32

	// MSDN says: maximum 4 characters for LOCALE_SDECIMAL and LOCALE_STHOUSAND, including terminating NULL
	const int len = 4;
	wchar_t temp[len];
	// Note that we are only supporting a single character as grouping and decimal symbol,
	// and thus we check that we get more than just the terminating NULL returned.
	if ( GetLocaleInfoW( langID, LOCALE_SDECIMAL, temp, len ) <= 1 )
	{
		CCP_LOGWARN( "Localization failed to get decimal char. Falling back to default setting." );
		return false;
	}
	decimalSep = temp[0];
	if ( GetLocaleInfoW( langID, LOCALE_STHOUSAND, temp, len ) <= 1)
	{
		CCP_LOGWARN( "Localization failed to get thousands char. Falling back to default setting." );
		return false;
	}
	thousandSep = temp[0];

	return true;

#elif __APPLE__

    CFLocaleRef locale = NULL;

	if( langID == LANGUAGEID_SYSTEM_DEFAULT )
	{
		//Get system default locale
        locale = CFLocaleCopyCurrent();
	}
	else
	{
		const char* localName = LanguageIDToLocaleName( langID );
        CFStringRef localId = CFStringCreateWithCString( kCFAllocatorDefault, localName,kCFStringEncodingUTF8 );
		if( !localId )
		{
            CCP_LOGWARN( "Localization failed to initialize localeID for %s. Falling back to default setting.", localName );
			return false;
		}

        locale = CFLocaleCreate( kCFAllocatorDefault, localId );
        CFRelease( localId );
		if( !locale )
		{
            CCP_LOGWARN( "Localization failed to create CFLocale %s. Falling back to default setting.", localName );
			return false;
		}
	}

    CFStringRef decimalSeparatorRef = ( CFStringRef )CFLocaleGetValue( locale, kCFLocaleDecimalSeparator );
    CFStringRef groupingSeparatorRef = ( CFStringRef )CFLocaleGetValue( locale, kCFLocaleGroupingSeparator );

    char temp[sizeof( wchar_t ) * 2];
	bool retVal = true;
    
    CFStringEncoding encoding;
    if( sizeof( wchar_t ) == 2 )
    {
        encoding = kCFStringEncodingUTF16LE;
    }
    else
    {
        encoding = kCFStringEncodingUTF32LE;
    }

    if( !CFStringGetCString( decimalSeparatorRef, temp, sizeof( temp ), encoding ) || !CFStringGetLength( decimalSeparatorRef ) )
	{
        CCP_LOGWARN( "Localization failed to get decimal char. Falling back to default setting." );
        retVal = false;
	}
	else
	{
		decimalSep = *reinterpret_cast<wchar_t*>( temp );
	}

    if( !CFStringGetCString( groupingSeparatorRef, temp, sizeof( temp ), encoding ) || !CFStringGetLength( groupingSeparatorRef ) )
	{
        CCP_LOGWARN( "Localization failed to get thousands char. Falling back to default setting." );
        retVal = false;
	}
	else
	{
        thousandSep = *reinterpret_cast<wchar_t*>( temp );
        // On macOS 11 French separator is a narrow non-breakable whitespace, that
        // we don't have a glyph for in our font. So we use a "normal" non-breakable
        // whitespace instead.
        if( thousandSep == L'\x202f')
        {
            thousandSep = L'\xa0';
        }
	}

    CFRelease( locale );

	return retVal;

#elif defined( __linux__ )

	// Use ICU rather than std::locale/localeconv, as the glibc implementation depends
	// on which locales happen to be installed on the host system.
	const icu::Locale locale = ( langID == LANGUAGEID_SYSTEM_DEFAULT ) ? icu::Locale::getDefault() : icu::Locale( LanguageIDToLocaleName( langID ) );

	UErrorCode status = U_ZERO_ERROR;
	icu::DecimalFormatSymbols symbols( locale, status );
	if( U_FAILURE( status ) )
	{
		CCP_LOGWARN( "Localization failed to get number symbols for %s (%s). Falling back to default setting.", locale.getName(), u_errorName( status ) );
		return false;
	}

	const icu::UnicodeString& decimal = symbols.getConstSymbol( icu::DecimalFormatSymbols::kDecimalSeparatorSymbol );
	const icu::UnicodeString& grouping = symbols.getConstSymbol( icu::DecimalFormatSymbols::kGroupingSeparatorSymbol );

	// Note that we are only supporting a single character as grouping and decimal symbol.
	if( decimal.isEmpty() )
	{
		CCP_LOGWARN( "Localization failed to get decimal char. Falling back to default setting." );
		return false;
	}
	if( grouping.isEmpty() )
	{
		CCP_LOGWARN( "Localization failed to get thousands char. Falling back to default setting." );
		return false;
	}

	decimalSep = static_cast<wchar_t>( decimal.char32At( 0 ) );
	thousandSep = static_cast<wchar_t>( grouping.char32At( 0 ) );

	// Some locales (e.g. French) use a narrow non-breaking space as the grouping separator,
	// which we don't have a glyph for in our font. Use a "normal" non-breaking space instead,
	// matching the macOS implementation.
	if( thousandSep == L'\x202f' )
	{
		thousandSep = L'\xa0';
	}

	return true;

#else

	return false;

#endif

}


// -------------------------------------------------------------
// Description:
//   Load a single token from its dictionary representation into a Token structure.
// Arguments:
//   dict - PyObject containing the dictionary with the token information.
//   token - Token instance in which we store the token information.
// Return value:
//   True if the token could be loaded, False otherwise.
// -------------------------------------------------------------
bool LoadToken( PyObject* dict, Token& token )
{
	Py_ssize_t p = 0;
	PyObject* key;
	PyObject* value;

	if ( ! PyDict_Check ( dict ) )
	{
		PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - LoadToken expects a dictionary." );
		return false;
	}

	while ( PyDict_Next( dict, &p, &key, &value ) )
	{
		if ( ! PyUnicode_Check( key ) )
		{
			PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - token must be keyed on a string" );
			return false;
		}
		const char* keyText = PyUnicode_AsUTF8( key );

		if ( 0 == strcmp( keyText, "args" ) )
		{
			if ( ! PyLong_Check( value ) )
			{
				PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - args must be number value." );
				return false;
			}
			token.flags = PyLong_AsLong( value );
		}
		else if ( 0 == strcmp( keyText, "variableType" ) && Py_None != value )
		{
			if ( ! PyLong_Check( value ) )
			{
				PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - variableType must be a number." );
				return false;
			}
			token.variableType = (VariableType) PyLong_AsLong( value );
		}
		else if ( 0 == strcmp( keyText, "variableName" ) && Py_None != value )
		{
			if ( ! PyUnicode_Check( value ) )
			{
				PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - variableName must be a string." );
				return false;
			}
			token.variableName = std::string( PyUnicode_AsUTF8( value ) );
		}
		else if ( 0 == strcmp( keyText, "propertyName" ) && Py_None != value )
		{
			if ( ! PyUnicode_Check( value ) )
			{
				PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - propertyName must be a string." );
				return false;
			}
			token.propertyName = std::string(PyUnicode_AsUTF8( value ) );
		}
		else if ( 0 == strcmp( keyText, "kwargs" ) )
		{
			if ( ! PyDict_Check( value ) )
			{
				PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - kwargs must be a dictionary." );
				return false;
			}
			PyObject* k;
			PyObject* v;
			Py_ssize_t s = 0;
			while ( PyDict_Next( value, &s, &k, &v ) )
			{
				if ( ! PyUnicode_Check( v ) && ! PyLong_Check( v ) )
				{
					PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - kwarg value must be string or number." );
					return false;
				}

				PyObject* keyAsAscii = PyUnicode_AsASCIIString( k );

				if ( !keyAsAscii )
				{
					PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - kwarg key must be ascii string." );
					return false;
				}
				
				std::string key( PyBytes_AsString( keyAsAscii ) );
				Py_DecRef( keyAsAscii );

				if ( ! token.kwargs )
				{
					token.kwargs = CCP_NEW( "eveLocalization/KeywordArgs" ) KeywordArgs();
				}

				Py_XINCREF( v );
				token.kwargs->insert( KeywordArgs::value_type( key, v ) );
			}
		}
		else if ( 0 == strcmp( keyText, "conditionalValues" ) )
		{
			if ( ! PyList_Check( value ) )
			{
				PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - conditionalValues must be a list." );
				return false;
			}

			Py_ssize_t len = PyList_Size( value );

			if ( len > 3 )
			{
				PyErr_SetString( PyExc_ValueError, "Localization::ParseToken - too many conditionalValues to unpack" );
				return false;
			}

			for ( Py_ssize_t i = 0; i < len; ++i )
			{
				PyObject* item = PyList_GetItem( value, i );
				if ( ! PyUnicode_Check ( item ) )
				{
					PyErr_SetString( PyExc_TypeError, "Localization::ParseToken - conditionalValues must be unicode strings" );
					return false;
				}
                std::wstring result( PyUnicodeToWString( item ) );
				token.conditionalValues[i] = result;
			}
		}
	}

	// -- BEGIN NO TOKENIZER UPDATES POSSIBLE WORKAROUND --
	// Temporary work around until we update the pickle generator again
	if ( token.variableType == VARIABLETYPE_DATETIME || token.variableType == VARIABLETYPE_FORMATTEDTIME )
	{
		// inject the formatting string
		if ( ! token.kwargs )
		{
			token.kwargs = CCP_NEW( "eveLocalization/KeywordArgs" ) KeywordArgs();
		}
		if ( token.kwargs->find( "format" ) == token.kwargs->cend() )
		{
			token.kwargs->insert( KeywordArgs::value_type( "format", PyUnicode_FromString( "%Y.%m.%d %H:%M" ) ) );
		}
	}

	// timeinterval needs special handling as it's the only property handler with a default property that does not return the input value
	if ( token.variableType == VARIABLETYPE_TIMEINTERVAL )
	{
		// inject the default property name
		if ( token.propertyName.empty() )
		{
			token.propertyName = "shortForm";
		}
	}

	// see if we need leading zeroes
	if ( token.kwargs && token.kwargs->find( "leadingZeroes" ) != token.kwargs->cend() )
	{
		token.flags |= TOKENFLAG_LEADINGZEROES;
	}

	// Quantity kwarg superseeds a quantity flag because the quantity comes from kwargs
	if ( token.kwargs && token.kwargs->find( "quantity" ) != token.kwargs->cend() )
	{
		if ( ( token.flags & TOKENFLAG_QUANTITY ) == TOKENFLAG_QUANTITY )
		{
			token.flags ^= TOKENFLAG_QUANTITY;
		}
	}
	// -- END WORKAROUND --

	return true;
}

// -------------------------------------------------------------
// Description:
//   Loads tokens from a python dictionary into a MessageData structure.
// Arguments:
//   dict - Dictionary in the form of {messageID: tokenDictionary, ...}.
//   messageData - reference to a MessageData instance which will be populated with the parsed tokens.
// -------------------------------------------------------------
bool LoadTokens( PyObject* dict, MessageData& messageData )
{
	Py_ssize_t p = 0;
	PyObject* key;
	PyObject* value;

	p = PyDict_Size( dict );
	if ( p == 0 )
	{
		// We probably want to log a warning here as this case should never happen
		return true;
	}

	p = 0;

	while ( PyDict_Next( dict, &p, &key, &value ) )
	{
		if ( ! ( PyUnicode_Check( key ) ) )
		{
			PyErr_SetString( PyExc_TypeError, "Localization::ParseTokens - token dictionary must be keyed on unicode string." );
			return false;
		}

		Token* token = CCP_NEW( "LoadTokens/token" ) Token;
		if ( ! LoadToken( value, *token ) )
		{
			CCP_DELETE token;

			// error message was set from within ParseToken
			return false;
		}

        token->tagName = PyUnicodeToWString( key );

		// temporarily keep support for linkify
		// TODO: Does this really have to happen on load time? <snorri>
		if ( token->tagName.find( L"linkify" ) != std::wstring::npos )
		{
			token->flags |= TOKENFLAG_LINKIFY;
		}

		if ( token->tagName.find( L"linkinfo" ) != std::wstring::npos )
		{
			token->flags |= TOKENFLAG_LINKINFO;
		}

		if ( ! messageData.tokens )
		{
			messageData.tokens = CCP_NEW( "eveLocalization/Tokens" ) TokenContainer();
		}

		messageData.tokens->insert( TokenContainer::value_type( token->tagName, token ) );
	}

	return true;
}

#ifndef _WIN32
int _stricmp( const char* a, const char* b )
{
    int ca, cb;
    do
    {
        ca = *a++;
        cb = *b++;
        ca = tolower( ca );
        cb = tolower( cb );
    }
    while( ca == cb && ca != 0 );
    return ca - cb;
}
#endif

// -------------------------------------------------------------
// Description:
//   Convert python language codes into a LanguageID
// Arguments:
//   code - string containing the python language code.
// Return value:
//   The LanguageID
// -------------------------------------------------------------
LanguageID CodeToLanguageID( const char* code )
{
	if ( 0 == _stricmp( code, "en-us" ) )   return LANGUAGEID_ENGLISH_US;
	if ( 0 == _stricmp( code, "en" ) )      return LANGUAGEID_ENGLISH_US;
	if ( 0 == _stricmp( code, "ja" ) )      return LANGUAGEID_JAPANESE;
	if ( 0 == _stricmp( code, "ko" ) )      return LANGUAGEID_KOREAN;
	if ( 0 == _stricmp( code, "zh" ) )      return LANGUAGEID_CHINESE_SIMPLIFIED;
	if ( 0 == _stricmp( code, "de" ) )      return LANGUAGEID_GERMAN;
	if ( 0 == _stricmp( code, "fr" ) )		return LANGUAGEID_FRENCH;
	if ( 0 == _stricmp( code, "it" ) )		return LANGUAGEID_ITALIAN;
	if ( 0 == _stricmp( code, "es" ) )		return LANGUAGEID_SPANISH;
	if ( 0 == _stricmp( code, "ru" ) )      return LANGUAGEID_RUSSIAN;
	if ( 0 == _stricmp( code, "" ) )        return LANGUAGEID_SYSTEM_DEFAULT;


	// default to english
	assert ( 0 && "There is a missing language code mapping in the localization module, please update enum and mapping." );
	return LANGUAGEID_ENGLISH_US;
}

// -------------------------------------------------------------
// Description:
//   Convert a LanguageID to its default Python representation.
// Arguments:
//   languageID - the LanguageID to convert.
// Return value:
//   String containing the python language code.
// -------------------------------------------------------------
const char* LanguageIDToCode( LanguageID languageID )
{
	switch( languageID )
	{
		case LANGUAGEID_CHINESE_SIMPLIFIED:
			return "zh";
		case LANGUAGEID_GERMAN:
			return "de";
		case LANGUAGEID_RUSSIAN:
			return "ru";
		case LANGUAGEID_FRENCH:
			return "fr";
		case LANGUAGEID_ITALIAN:
			return "it";
		case LANGUAGEID_SPANISH:
			return "es";
		case LANGUAGEID_JAPANESE:
			return "ja";
		case LANGUAGEID_KOREAN:
		    return "ko";
		case LANGUAGEID_SYSTEM_DEFAULT:
			return "";
		case LANGUAGEID_ENGLISH_US:
		default:
			return "en-us";
	};
}

// -------------------------------------------------------------
// Description:
//   Converts a LanguageID to its locale name.
//   For a list of locale names see: http://msdn.microsoft.com/en-us/library/39cwe7zf.aspx
// Arguments:
//   languageID - LanguageID for which we want to get the locale name
// Return value:
//   String containing the locale name for the passed in LanguageID.
// -------------------------------------------------------------
const char* LanguageIDToLocaleName( LanguageID languageID )
{
#ifdef _WIN32
	switch( languageID )
	{
		case LANGUAGEID_CHINESE_SIMPLIFIED:
			return "chinese-simplified";
		case LANGUAGEID_GERMAN:
			return "german";
		case LANGUAGEID_RUSSIAN:
			return "russian";
		case LANGUAGEID_FRENCH:
			return "french";
		case LANGUAGEID_ITALIAN:
			return "italian";
		case LANGUAGEID_SPANISH:
			return "spanish";
		case LANGUAGEID_JAPANESE:
			return "japanese";
		case LANGUAGEID_KOREAN:
		    return "korean";
		case LANGUAGEID_ENGLISH_US:
		default:
			return "english";
	};
#else
switch( languageID )
{
    case LANGUAGEID_CHINESE_SIMPLIFIED:
        return "zh_CN";
    case LANGUAGEID_GERMAN:
        return "de_DE";
    case LANGUAGEID_RUSSIAN:
        return "ru_RU";
    case LANGUAGEID_FRENCH:
        return "fr_FR";
    case LANGUAGEID_ITALIAN:
        return "it_IT";
    case LANGUAGEID_SPANISH:
        return "es_ES";
    case LANGUAGEID_JAPANESE:
        return "ja_JP";
    case LANGUAGEID_KOREAN:
        return "ko_KR";
    case LANGUAGEID_ENGLISH_US:
    default:
        return "en_US";
};
#endif
};


std::wstring PyUnicodeToWString( PyObject* unicode )
{
	wchar_t* tmp = PyUnicode_AsWideCharString( unicode, nullptr );
	std::wstring ss( tmp );
	PyMem_Free( tmp );
	return ss;
}

#ifdef __APPLE__
CFStringRef ToStringRef( const char* string )
{
    return CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const uint8_t*>( string ),
        strlen( string ) * sizeof( char ),
        kCFStringEncodingASCII,
        false );
}

CFStringRef ToStringRef( const std::string& string )
{
    return CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const uint8_t*>( string.c_str() ),
        string.length() * sizeof( char ),
        kCFStringEncodingASCII,
        false );
}

CFStringRef ToStringRef( const wchar_t* string, size_t length )
{
    CFStringEncoding encoding = ( CFByteOrderLittleEndian == CFByteOrderGetCurrent() ) ? kCFStringEncodingUTF32LE : kCFStringEncodingUTF32BE;
    
    return CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const uint8_t*>( string ),
        length * sizeof( wchar_t ),
        encoding,
        false );
}

CFStringRef ToStringRef( const std::wstring& string )
{
    CFStringEncoding encoding = ( CFByteOrderLittleEndian == CFByteOrderGetCurrent() ) ? kCFStringEncodingUTF32LE : kCFStringEncodingUTF32BE;
    
    return CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const uint8_t*>( string.c_str() ),
        string.size() * sizeof( wchar_t ),
        encoding,
        false );
}
#endif


//-----------------------------------------------------------------------------
// The globals
//-----------------------------------------------------------------------------
static struct PyModuleDef ModuleDef =
{
	PyModuleDef_HEAD_INIT,
	CCP_STRINGIZE( CCP_CONCATENATE( _evelocalization, CCP_BUILD_FLAVOR ) ),
	"",
	-1,
	NULL
};

static PyObject* StartDLL()
{
	BeClasses->RegisterClasses( BlueRegistration::GetClassRegs() );
	
	PyObject* module = PyModule_Create( &ModuleDef );
	
	PyMethodDef pmd[] = { 
							{ 
								"GetMessageByID", 
								( PyCFunction ) PyGetMessageByID, 
								METH_VARARGS | METH_KEYWORDS, 
								"C implementation of Cerberus' _GetByMessageID." 
							},
							{ 
								"FormatNumeric", 
								( PyCFunction ) PyFormatNumeric, 
								METH_VARARGS | METH_KEYWORDS, 
								"C implementation of FormatNumeric." 
							},
							{
								"Parse",
								( PyCFunction ) PyParse,
								METH_VARARGS | METH_KEYWORDS,
								"C implementation of the cerberus parser."
							},
						};
	for ( size_t i = 0; i < ( sizeof( pmd ) / sizeof( pmd[0] ) ); ++i )
	{
		BlueRegistration::GetFuncRegs().push_back( pmd[i] );
	}

	BlueRegisterToModule( module, 
						  BlueRegistration::GetClassRegs(), 
						  BlueRegistration::GetFuncRegs(), 
						  BlueRegistration::GetEnumRegs() );
	
	//Init 3rd party libs here.

	return module;
}

#ifdef _WIN32
BOOL APIENTRY DllMain( HINSTANCE instance, DWORD reason, LPVOID )
{
	if ( reason == DLL_PROCESS_ATTACH )
	{
		DisableThreadLibraryCalls( instance );
	}
	else if ( reason == DLL_PROCESS_DETACH )
	{
		;
	}
    return TRUE;
}
#endif

//-----------------------------------------------------------------------------
// init - python dll module entry function
//-----------------------------------------------------------------------------
extern "C"
#ifdef _WIN32
__declspec( dllexport )
#else
__attribute__((visibility ("default")))
#endif
PyObject*
CCP_CONCATENATE( PyInit__evelocalization, CCP_BUILD_FLAVOR )()
{
	return StartDLL();
}
