#undef  _UNICODE
#define _UNICODE
#undef  UNICODE
#define UNICODE

#include "ReflectiveLoader.h"
#include <stdio.h>
#include <objbase.h>
#include <activeds.h>
#include <sddl.h>

#pragma comment(lib, "ADSIid.lib")
#pragma comment(lib, "ActiveDS.Lib")

// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;


int IS_BUFFER_ENOUGH(UINT maxAlloc, LPWSTR pszTarget, LPCWSTR pszSource, int toCopy = -1) {
	if (toCopy == -1) {
		toCopy = wcslen(pszSource);
	}

	return maxAlloc - (wcslen(pszTarget) + toCopy + 1);
}

HRESULT FindComputers(IDirectorySearch *pContainerToSearch,	// IDirectorySearch pointer to Partitions container.
	LPOLESTR szFilter,										// Filter for finding specific crossrefs. NULL returns all attributeSchema objects.
	LPOLESTR *pszPropertiesToReturn)						// Properties to return for crossRef objects found. NULL returns all set properties.
{
	if (!pContainerToSearch)
		return E_POINTER;

	// Create search filter
	LPOLESTR pszSearchFilter = new OLECHAR[MAX_PATH * 2];
	if (!pszSearchFilter)
		return E_OUTOFMEMORY;
	wchar_t szFormat[] = L"(&(objectCategory=computer)(objectClass=computer)%s)";

	// Check the buffer first
	if (IS_BUFFER_ENOUGH(MAX_PATH * 2, szFormat, szFilter) > 0)
	{
		// Add the filter.
		swprintf_s(pszSearchFilter, MAX_PATH * 2, szFormat, szFilter);
	}
	else
	{
		wprintf(L"[!] The filter is too large for buffer, aborting...");
		delete[] pszSearchFilter;
		return FALSE;
	}

	// Specify subtree search
	ADS_SEARCHPREF_INFO SearchPrefs;
	SearchPrefs.dwSearchPref = ADS_SEARCHPREF_SEARCH_SCOPE;
	SearchPrefs.vValue.dwType = ADSTYPE_INTEGER;
	SearchPrefs.vValue.Integer = ADS_SCOPE_SUBTREE;
	DWORD dwNumPrefs = 1;

	// COL for iterations
	LPOLESTR pszColumn = NULL;
	ADS_SEARCH_COLUMN col;
	HRESULT hr;

	// Interface Pointers
	IADs *pObj = NULL;
	IADs *pIADs = NULL;

	// Handle used for searching
	ADS_SEARCH_HANDLE hSearch = NULL;

	// Set the search preference
	hr = pContainerToSearch->SetSearchPreference(&SearchPrefs, dwNumPrefs);
	if (FAILED(hr))
	{
		delete[] pszSearchFilter;
		return hr;
	}

	LPOLESTR pszBool = NULL;
	DWORD dwBool;
	PSID pObjectSID = NULL;
	LPOLESTR szSID = NULL;
	LPOLESTR szDSGUID = new WCHAR[39];
	LPGUID pObjectGUID = NULL;
	FILETIME filetime;
	SYSTEMTIME systemtime;
	DATE date;
	VARIANT varDate;
	LARGE_INTEGER liValue;
	LPOLESTR *pszPropertyList = NULL;

	int iCount = 0;
	DWORD x = 0L;

	if (!pszPropertiesToReturn)
	{
		// Return all properties.
		hr = pContainerToSearch->ExecuteSearch(pszSearchFilter,
			NULL,
			-1L,
			&hSearch);
	}
	else
	{
		// Specified subset.
		pszPropertyList = pszPropertiesToReturn;

		// Return specified properties
		hr = pContainerToSearch->ExecuteSearch(pszSearchFilter,
			pszPropertyList,
			sizeof(pszPropertyList) / sizeof(LPOLESTR),
			&hSearch);
	}

	if (SUCCEEDED(hr))
	{
		// Call IDirectorySearch::GetNextRow() to retrieve the next row of data
		hr = pContainerToSearch->GetFirstRow(hSearch);
		if (SUCCEEDED(hr))
		{
			while (hr != S_ADS_NOMORE_ROWS)
			{
				// Keep track of count.
				iCount++;
					
				wprintf(L"--------------------------------------------------------------------\n");
				
				// Loop through the array of passed column names, print the data for each column
				while (pContainerToSearch->GetNextColumnName(hSearch, &pszColumn) != S_ADS_NOMORE_COLUMNS)
				{
					hr = pContainerToSearch->GetColumn(hSearch, pszColumn, &col);
					if (SUCCEEDED(hr))
					{
						// Print the data for the column and free the column
						// Get the data for this column
						wprintf(L"[+] %s:\n", col.pszAttrName);
						switch (col.dwADsType)
						{
						case ADSTYPE_DN_STRING:
							for (x = 0; x< col.dwNumValues; x++)
							{
								wprintf(L"    %s\r\n", col.pADsValues[x].DNString);
							}
							break;
						case ADSTYPE_CASE_EXACT_STRING:
						case ADSTYPE_CASE_IGNORE_STRING:
						case ADSTYPE_PRINTABLE_STRING:
						case ADSTYPE_NUMERIC_STRING:
						case ADSTYPE_TYPEDNAME:
						case ADSTYPE_FAXNUMBER:
						case ADSTYPE_PATH:
						case ADSTYPE_OBJECT_CLASS:
							for (x = 0; x< col.dwNumValues; x++)
							{
								wprintf(L"    %s\r\n", col.pADsValues[x].CaseIgnoreString);
							}
							break;
						case ADSTYPE_BOOLEAN:
							for (x = 0; x< col.dwNumValues; x++)
							{
								dwBool = col.pADsValues[x].Boolean;
								pszBool = dwBool ? L"TRUE" : L"FALSE";
								wprintf(L"    %s\r\n", pszBool);
							}
							break;
						case ADSTYPE_INTEGER:
							for (x = 0; x< col.dwNumValues; x++)
							{
								wprintf(L"    %d\r\n", col.pADsValues[x].Integer);
							}
							break;
						case ADSTYPE_OCTET_STRING:
							if (_wcsicmp(col.pszAttrName, L"objectSID") == 0)
							{
								for (x = 0; x< col.dwNumValues; x++)
								{
									pObjectSID = (PSID)(col.pADsValues[x].OctetString.lpValue);
									// Convert SID to string.
									ConvertSidToStringSid(pObjectSID, &szSID);
									wprintf(L"    %s\r\n", szSID);
									LocalFree(szSID);
								}
							}
							else if ((_wcsicmp(col.pszAttrName, L"objectGUID") == 0))
							{
								for (x = 0; x< col.dwNumValues; x++)
								{
									// Cast to LPGUID
									pObjectGUID = (LPGUID)(col.pADsValues[x].OctetString.lpValue);
									// Convert GUID to string.
									::StringFromGUID2(*pObjectGUID, szDSGUID, 39);
									// Print the GUID
									wprintf(L"    %s\r\n", szDSGUID);
								}
							}
							else
								wprintf(L"    Value of type Octet String. No Conversion.\n");
							break;
						case ADSTYPE_UTC_TIME:
							for (x = 0; x< col.dwNumValues; x++)
							{
								systemtime = col.pADsValues[x].UTCTime;
								if (SystemTimeToVariantTime(&systemtime,
									&date) != 0)
								{
									// Pack in variant.vt
									varDate.vt = VT_DATE;
									varDate.date = date;
									VariantChangeType(&varDate, &varDate, VARIANT_NOVALUEPROP, VT_BSTR);
									wprintf(L"    %s\r\n", varDate.bstrVal);
									VariantClear(&varDate);
								}
								else
									wprintf(L"[!] Could not convert UTC-Time.\n");
							}
							break;
						case ADSTYPE_LARGE_INTEGER:
							for (x = 0; x< col.dwNumValues; x++)
							{
								liValue = col.pADsValues[x].LargeInteger;
								filetime.dwLowDateTime = liValue.LowPart;
								filetime.dwHighDateTime = liValue.HighPart;
								if ((filetime.dwHighDateTime == 0) && (filetime.dwLowDateTime == 0))
								{
									wprintf(L"    No value set.\n");
								}
								else
								{
									// Check for properties of type LargeInteger that represent time
									// if TRUE, then convert to variant time.
									if ((0 == wcscmp(L"accountExpires", col.pszAttrName)) |
										(0 == wcscmp(L"badPasswordTime", col.pszAttrName)) ||
										(0 == wcscmp(L"lastLogon", col.pszAttrName)) ||
										(0 == wcscmp(L"lastLogoff", col.pszAttrName)) ||
										(0 == wcscmp(L"lockoutTime", col.pszAttrName)) ||
										(0 == wcscmp(L"pwdLastSet", col.pszAttrName))
										)
									{
										// Handle special case for Never Expires where low part is -1
										if (filetime.dwLowDateTime == -1)
										{
											wprintf(L"    Never Expires.\n");
										}
										else
										{
											if (FileTimeToLocalFileTime(&filetime, &filetime) != 0)
											{
												if (FileTimeToSystemTime(&filetime,
													&systemtime) != 0)
												{
													if (SystemTimeToVariantTime(&systemtime,
														&date) != 0)
													{
														// Pack in variant.vt
														varDate.vt = VT_DATE;
														varDate.date = date;
														VariantChangeType(&varDate, &varDate, VARIANT_NOVALUEPROP, VT_BSTR);
														wprintf(L"    %s\r\n", varDate.bstrVal);
														VariantClear(&varDate);
													}
													else
													{
														wprintf(L"    FileTimeToVariantTime failed\n");
													}
												}
												else
												{
													wprintf(L"    FileTimeToSystemTime failed\n");
												}

											}
											else
											{
												wprintf(L"    FileTimeToLocalFileTime failed\n");
											}
										}
									}
									else
									{
										// Print the LargeInteger.
										wprintf(L"    high: %d low: %d\r\n", filetime.dwHighDateTime, filetime.dwLowDateTime);
									}
								}
							}
							break;
						case ADSTYPE_NT_SECURITY_DESCRIPTOR:
							for (x = 0; x< col.dwNumValues; x++)
							{
								wprintf(L"    Security descriptor.\n");
							}
							break;
						default:
							wprintf(L"[!] Unknown type %d.\n", col.dwADsType);
						}

						pContainerToSearch->FreeColumn(&col);
					}
					CoTaskMemFree(pszColumn);
				}

				// Get the next row
				hr = pContainerToSearch->GetNextRow(hSearch);
			}
		}
		// Close the search handle to clean up
		pContainerToSearch->CloseSearchHandle(hSearch);
	}
	if (SUCCEEDED(hr) && 0 == iCount)
		hr = S_FALSE;

	wprintf(L"--------------------------------------------------------------------\n");

	delete[] pszSearchFilter;
	return hr;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved)
{
	BOOL bReturnValue = TRUE;
	LPWSTR pwszParams = (LPWSTR)calloc(strlen((LPSTR)lpReserved) + 1, sizeof(WCHAR));
	size_t convertedChars = 0;
	size_t newsize = strlen((LPSTR)lpReserved) + 1;

	switch (dwReason)
	{
	case DLL_QUERY_HMODULE:
		if (lpReserved != NULL)
			*(HMODULE *)lpReserved = hAppInstance;
		break;
	case DLL_PROCESS_ATTACH:
		hAppInstance = hinstDLL;

		if (lpReserved != NULL) {

			// Handle the command line arguments.
			int maxAlloc = MAX_PATH * 2;
			LPOLESTR pszBuffer = new OLECHAR[maxAlloc];
			mbstowcs_s(&convertedChars, pwszParams, newsize, (LPSTR)lpReserved, _TRUNCATE);
			wcscpy_s(pszBuffer, maxAlloc, pwszParams);

			// Initialize COM
			CoInitialize(NULL);
			HRESULT hr = S_OK;

			// Get rootDSE and the current user's domain container DN.
			IADs *pObject = NULL;
			IDirectorySearch *pContainerToSearch = NULL;
			LPOLESTR szPath = new OLECHAR[MAX_PATH];
			VARIANT var;
			hr = ADsOpenObject(L"LDAP://rootDSE",
				NULL,
				NULL,
				ADS_SECURE_AUTHENTICATION, // Use Secure Authentication
				IID_IADs,
				(void**)&pObject);
			if (FAILED(hr))
			{
				wprintf(L"[!] Could not execute query. Could not bind to LDAP://rootDSE.\n");
				if (pObject)
					pObject->Release();
				delete[] pszBuffer;
				delete[] szPath;
				CoUninitialize();

				// Flush STDOUT
				fflush(stdout);

				// We're done, so let's exit
				ExitProcess(0);
			}
			if (SUCCEEDED(hr))
			{
				hr = pObject->Get(L"defaultNamingContext", &var);
				if (SUCCEEDED(hr))
				{
					// Build path to the domain container.
					wcscpy_s(szPath, MAX_PATH, L"LDAP://");
					if (IS_BUFFER_ENOUGH(MAX_PATH, szPath, var.bstrVal) > 0)
					{
						wcscat_s(szPath, MAX_PATH, var.bstrVal);
					}
					else
					{
						wprintf(L"[!] Buffer is too small for the domain DN");
						delete[] pszBuffer;
						delete[] szPath;
						CoUninitialize();

						// Flush STDOUT
						fflush(stdout);

						// We're done, so let's exit
						ExitProcess(0);
					}

					hr = ADsOpenObject(szPath,
						NULL,
						NULL,
						ADS_SECURE_AUTHENTICATION, // Use Secure Authentication
						IID_IDirectorySearch,
						(void**)&pContainerToSearch);

					if (SUCCEEDED(hr))
					{
						hr = FindComputers(pContainerToSearch, // IDirectorySearch pointer to Partitions container.
							pszBuffer,
							NULL	//Return all properties
						);
						if (SUCCEEDED(hr))
						{
							if (S_FALSE == hr)
								wprintf(L"[!] No computer object could be found.\n");
						}
						else if (0x8007203e == hr)
							wprintf(L"[!] Could not execute query. An invalid filter was specified.\n");
						else
							wprintf(L"[!] Query failed to run. HRESULT: %x\n", hr);
					}
					else
					{
						wprintf(L"[!] Could not execute query. Could not bind to the container.\n");
					}
					if (pContainerToSearch)
						pContainerToSearch->Release();
				}
				VariantClear(&var);
			}
			if (pObject)
				pObject->Release();

			delete[] pszBuffer;
			delete[] szPath;

			// Uninitialize COM
			CoUninitialize();
		}

		// Flush STDOUT
		fflush(stdout);

		// We're done, so let's exit
		ExitProcess(0);
		break;
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return bReturnValue;
}
