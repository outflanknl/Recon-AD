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

HRESULT FindSPNs(IDirectorySearch *pContainerToSearch,  // IDirectorySearch pointer to Partitions container.
	LPOLESTR szFilter,									// Filter for finding specific crossrefs. NULL returns all attributeSchema objects.
	LPOLESTR *pszPropertiesToReturn)					// Properties to return for crossRef objects found. NULL returns all set properties.
{
	if (!pContainerToSearch)
		return E_POINTER;
	
	// Create search filter
	LPOLESTR pszSearchFilter = new OLECHAR[MAX_PATH * 2];
	if (!pszSearchFilter)
		return E_OUTOFMEMORY;
	wchar_t szFormat[] = L"(&(objectClass=user)(objectCategory=person)%s)";

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

	FILETIME filetime;
	SYSTEMTIME systemtime;
	DATE date;
	VARIANT varDate;
	LARGE_INTEGER liValue;
	LPOLESTR *pszPropertyList = NULL;

	typedef struct _USER_INFO {
		WCHAR chName[MAX_PATH];
		WCHAR chDistinguishedName[MAX_PATH];
		WCHAR chSamAccountName[MAX_PATH];
		WCHAR chDescription[MAX_PATH];
		WCHAR chuserPrincipalName[MAX_PATH];
		WCHAR chMemberOf[250][MAX_PATH];
		WCHAR chServicePrincipalName[250][MAX_PATH];
		WCHAR chWhenCreated[MAX_PATH];
		WCHAR chWhenChanged[MAX_PATH];
		WCHAR chPwdLastSet[MAX_PATH];
		WCHAR chAccountExpires[MAX_PATH];
		WCHAR chLastLogon[MAX_PATH];
	} USER_INFO, *PUSER_INFO;

	PUSER_INFO pUserInfo = (PUSER_INFO)calloc(1, sizeof(USER_INFO));

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

				// Loop through the array of passed column names, print the data for each column
				while (pContainerToSearch->GetNextColumnName(hSearch, &pszColumn) != S_ADS_NOMORE_COLUMNS)
				{
					hr = pContainerToSearch->GetColumn(hSearch, pszColumn, &col);
					if (SUCCEEDED(hr))
					{
						// Print the data for the column and free the column
						// Get the data for this column
						switch (col.dwADsType)
						{
						case ADSTYPE_DN_STRING:
							for (x = 0; x < col.dwNumValues; x++)
							{
								if (_wcsicmp(col.pszAttrName, L"memberOf") == 0) {
									wcscpy_s(pUserInfo->chMemberOf[x], MAX_PATH, col.pADsValues[x].DNString);
								}
							}
							if (_wcsicmp(col.pszAttrName, L"distinguishedName") == 0) {
								wcscpy_s(pUserInfo->chDistinguishedName, MAX_PATH, col.pADsValues->CaseIgnoreString);
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
							for (x = 0; x < col.dwNumValues; x++)
							{
								if (_wcsicmp(col.pszAttrName, L"servicePrincipalName") == 0) {
									wcscpy_s(pUserInfo->chServicePrincipalName[x], MAX_PATH, col.pADsValues[x].CaseIgnoreString);
								}
							}
							if (_wcsicmp(col.pszAttrName, L"name") == 0) {
								wcscpy_s(pUserInfo->chName, MAX_PATH, col.pADsValues->CaseIgnoreString);
							}
							else if (_wcsicmp(col.pszAttrName, L"description") == 0) {
								wcscpy_s(pUserInfo->chDescription, MAX_PATH, col.pADsValues->CaseIgnoreString);
							}
							else if (_wcsicmp(col.pszAttrName, L"userPrincipalName") == 0) {
								wcscpy_s(pUserInfo->chuserPrincipalName, MAX_PATH, col.pADsValues->CaseIgnoreString);
							}
							else if (_wcsicmp(col.pszAttrName, L"sAMAccountName") == 0) {
								wcscpy_s(pUserInfo->chSamAccountName, MAX_PATH, col.pADsValues->CaseIgnoreString);
							}
							else {
								break;
							}
							break;
						case ADSTYPE_BOOLEAN:
						case ADSTYPE_INTEGER:
						case ADSTYPE_OCTET_STRING:
						case ADSTYPE_UTC_TIME:
							for (x = 0; x < col.dwNumValues; x++)
							{
								systemtime = col.pADsValues[x].UTCTime;
								if (SystemTimeToVariantTime(&systemtime,
									&date) != 0)
								{
									//Pack in variant.vt
									varDate.vt = VT_DATE;
									varDate.date = date;
									VariantChangeType(&varDate, &varDate, VARIANT_NOVALUEPROP, VT_BSTR);

									if (_wcsicmp(col.pszAttrName, L"whenCreated") == 0) {
										wcscpy_s(pUserInfo->chWhenCreated, MAX_PATH, varDate.bstrVal);
									}
									else if (_wcsicmp(col.pszAttrName, L"whenChanged") == 0) {
										wcscpy_s(pUserInfo->chWhenChanged, MAX_PATH, varDate.bstrVal);
									}
									else {
										VariantClear(&varDate);
									}
									VariantClear(&varDate);
								}
							}
							break;
						case ADSTYPE_LARGE_INTEGER:
							for (x = 0; x < col.dwNumValues; x++)
							{
								liValue = col.pADsValues[x].LargeInteger;
								filetime.dwLowDateTime = liValue.LowPart;
								filetime.dwHighDateTime = liValue.HighPart;

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
										if (_wcsicmp(col.pszAttrName, L"accountExpires") == 0) {
											wcscpy_s(pUserInfo->chAccountExpires, MAX_PATH, L"Never Expires");
										}

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

													if (_wcsicmp(col.pszAttrName, L"pwdLastSet") == 0) {
														wcscpy_s(pUserInfo->chPwdLastSet, MAX_PATH, varDate.bstrVal);
													}
													else if (_wcsicmp(col.pszAttrName, L"lastLogon") == 0) {
														if (_wcsicmp(varDate.bstrVal, L"1-1-1601 02:00:00") == 0) {
															wcscpy_s(pUserInfo->chLastLogon, MAX_PATH, L"Never");
														}
														else {
															wcscpy_s(pUserInfo->chLastLogon, MAX_PATH, varDate.bstrVal);
														}

													}
													else if (_wcsicmp(col.pszAttrName, L"accountExpires") == 0) {
														if (_wcsicmp(varDate.bstrVal, L"1-1-1601 02:00:00") == 0) {
															wcscpy_s(pUserInfo->chAccountExpires, MAX_PATH, L"Never Expires");
														}
														else {
															wcscpy_s(pUserInfo->chAccountExpires, MAX_PATH, varDate.bstrVal);
														}
													}

													VariantClear(&varDate);
												}
											}
										}
									}
								}
							}
							break;
						case ADSTYPE_NT_SECURITY_DESCRIPTOR:
						default:
							wprintf(L"Unknown type %d.\n", col.dwADsType);
						}

						pContainerToSearch->FreeColumn(&col);
					}
					CoTaskMemFree(pszColumn);
				}

				if (wcscmp(pUserInfo->chServicePrincipalName[0], L"") != 0) {

					wprintf(L"--------------------------------------------------------------------\n");

					wprintf(L"[+] name:\n");
					wprintf(L"    %s\r\n", pUserInfo->chName);

					wprintf(L"[+] sAMAccountName:\n");
					wprintf(L"    %s\r\n", pUserInfo->chSamAccountName);

					wprintf(L"[+] description:\n");
					wprintf(L"    %s\r\n", pUserInfo->chDescription);

					wprintf(L"[+] userPrincipalName:\n");
					wprintf(L"    %s\r\n", pUserInfo->chuserPrincipalName);

					wprintf(L"[+] distinguishedName:\n");
					wprintf(L"    %s\r\n", pUserInfo->chDistinguishedName);

					wprintf(L"[+] whenCreated:\n");
					wprintf(L"    %s\r\n", pUserInfo->chWhenCreated);

					wprintf(L"[+] whenChanged:\n");
					wprintf(L"    %s\r\n", pUserInfo->chWhenChanged);

					wprintf(L"[+] pwdLastSet:\n");
					wprintf(L"    %s\r\n", pUserInfo->chPwdLastSet);

					wprintf(L"[+] accountExpires:\n");
					wprintf(L"    %s\r\n", pUserInfo->chAccountExpires);

					wprintf(L"[+] lastLogon:\n");
					wprintf(L"    %s\r\n", pUserInfo->chLastLogon);

					wprintf(L"[+] memberOf:\n");
					for (x = 0; x < 250; x++) {
						if (wcscmp(pUserInfo->chMemberOf[x], L"") == 0) {
							break;
						}
						else {
							wprintf(L"    %s\r\n", pUserInfo->chMemberOf[x]);
						}
					}

					wprintf(L"[+] servicePrincipalName (SPNs):\n");
					for (x = 0; x < 250; x++) {
						if (wcscmp(pUserInfo->chServicePrincipalName[x], L"") == 0) {
							break;
						}
						else {
							wprintf(L"    %s\r\n", pUserInfo->chServicePrincipalName[x]);
						}
					}
				}

				RtlZeroMemory(pUserInfo, sizeof(USER_INFO));

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
						hr = FindSPNs(pContainerToSearch, // IDirectorySearch pointer to Partitions container.
							pszBuffer,
							NULL	//Return all properties
						);
						if (SUCCEEDED(hr))
						{
							if (S_FALSE == hr)
								wprintf(L"[!] No user object could be found.\n");
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
