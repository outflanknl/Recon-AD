#undef  _UNICODE
#define _UNICODE
#undef  UNICODE
#define UNICODE

#include "ReflectiveLoader.h"
#include <stdio.h>
#include <atlbase.h>
#include <atlstr.h>
#include <activeds.h>
#include <assert.h>

#pragma comment(lib, "ADSIid.lib")
#pragma comment(lib, "ActiveDS.Lib")

#define FETCH_NUM 100

// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;


HRESULT PrintGroupObjectMembers(IADsGroup * pADsGroup)
{
	HRESULT         hr = S_OK;					// COM Result Code
	IADsMembers *   pADsMembers = NULL;			// Pointer to Members of the IADsGroup
	BOOL            fContinue = TRUE;			// Looping Variable
	IEnumVARIANT *  pEnumVariant = NULL;		// Pointer to the Enum variant
	IUnknown *      pUnknown = NULL;			// IUnknown for getting the ENUM initially
	VARIANT         VariantArray[FETCH_NUM];	// Variant array for temp holding returned data
	ULONG           ulElementsFetched = NULL;	// Number of elements retrieved

	// Get an interface pointer to the IADsCollection of members.
	hr = pADsGroup->Members(&pADsMembers);

	if (SUCCEEDED(hr))
	{

		// Query the IADsCollection of members for a new ENUM Interface.
		// Be aware that the enum comes back as an IUnknown *
		hr = pADsMembers->get__NewEnum(&pUnknown);

		if (SUCCEEDED(hr))
		{

			// Call the QueryInterface method for the IUnknown * for a IEnumVARIANT interface.
			hr = pUnknown->QueryInterface(IID_IEnumVARIANT, (void **)&pEnumVariant);

			if (SUCCEEDED(hr))
			{

				// While no errors or end of data...
				while (fContinue)
				{
					ulElementsFetched = 0;

					// Get a "batch" number of group members - number of rows that FETCH_NUM specifies
					hr = ADsEnumerateNext(pEnumVariant, FETCH_NUM, VariantArray, &ulElementsFetched);

					if (ulElementsFetched)//SUCCEEDED(hr) && hr != S_FALSE)
					{
						wprintf(L"[+] Members:\n");

						// Loop through the current batch, printing 
						// the path for each member.
						for (ULONG i = 0; i < ulElementsFetched; i++)
						{
							IDispatch * pDispatch = NULL;
							// Pointer for holding dispath of element.
							IADs      * pIADsGroupMember = NULL;
							// IADs pointer to group member.
							BSTR        bstrPath = NULL;
							// Contains the path of the object.

							// Get the dispatch pointer for the variant.
							pDispatch = VariantArray[i].pdispVal;
							//assert(HAS_BIT_STYLE(VariantArray[i].vt, VT_DISPATCH));

							// Get the IADs interface for the "member" of this group.
							hr = pDispatch->QueryInterface(IID_IADs,
								(VOID **)&pIADsGroupMember);

							if (SUCCEEDED(hr))
							{

								// Get the ADsPath property for this member.
								hr = pIADsGroupMember->get_ADsPath(&bstrPath);

								if (SUCCEEDED(hr))
								{
									// Print the ADsPath of the group member.
									//CStringW sBstr;
									//sBstr = (LPCWSTR)bstrPath;
									//sBstr.Replace(L"WinNT://", L"");

									wprintf(L"    %s\r\n", (LPCWSTR)bstrPath);
									SysFreeString(bstrPath);
								}
								pIADsGroupMember->Release();
								pIADsGroupMember = NULL;
							}
						}

						// Clear the variant array.
						memset(VariantArray, 0, sizeof(VARIANT)*FETCH_NUM);
					}
					else
						fContinue = FALSE;
				}
				pEnumVariant->Release();
				pEnumVariant = NULL;
			}
			pUnknown->Release();
			pUnknown = NULL;
		}
		pADsMembers->Release();
		pADsMembers = NULL;
	}

	// If all completed normally, all data
	// was printed, and an S_FALSE, indicating 
	// no more data, was received. If so,
	// return S_OK.
	if (hr == S_FALSE)
		hr = S_OK;

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

				LPCWSTR pwszComputer = pszBuffer;
				LPCWSTR pwszClass = L"group";
				LPCWSTR pwszUsername = NULL;
				LPCWSTR pwszPassword = NULL;

				HRESULT hr;

				// Initialize COM
				CoInitialize(NULL);

				IADsContainer * pIADsCont = NULL;

				// Build the binding string.
				CComBSTR sbstrBindingString;
				sbstrBindingString = "WinNT://";
				sbstrBindingString += pwszComputer;
				sbstrBindingString += ",computer";

				// Bind to the container.
				hr = ADsOpenObject(sbstrBindingString,
					pwszUsername,
					pwszPassword,
					ADS_SECURE_AUTHENTICATION,
					IID_IADsContainer,
					(void**)&pIADsCont);

				if (SUCCEEDED(hr))
				{
					VARIANT vFilter;
					VariantInit(&vFilter);
					LPWSTR rgpwszFilter[] = { (LPWSTR)pwszClass };

					// Build a Variant of array type, using the filter passed.
					hr = ADsBuildVarArrayStr(rgpwszFilter, 1, &vFilter);
					if (SUCCEEDED(hr))
					{
						// Set the filter for the results of the enumeration.
						hr = pIADsCont->put_Filter(vFilter);
						if (SUCCEEDED(hr))
						{
							IEnumVARIANT *pEnumVariant = NULL;

							// Build an enumerator interface. This is used 
							// to enumerate the objects contained in 
							// the IADsContainer.
							hr = ADsBuildEnumerator(pIADsCont, &pEnumVariant);

							if (SUCCEEDED(hr))
							{
								VARIANT Variant;
								ULONG ulElementsFetched;

								wprintf(L"--------------------------------------------------------------------\n");

								// Loop through and print the data.
								while (SUCCEEDED(ADsEnumerateNext(pEnumVariant,
									1,
									&Variant,
									&ulElementsFetched))
									&& (ulElementsFetched > 0))
								{
									if (VT_DISPATCH == Variant.vt)
									{
										IADs *pIADs = NULL;

										// Query the variant IDispatch *
										// for the IADs interface
										hr = Variant.pdispVal->QueryInterface(IID_IADs,
											(VOID**)&pIADs);

										if (SUCCEEDED(hr))
										{
											// Print the object data.
											CComBSTR sbstrResult;
											hr = pIADs->get_Name(&sbstrResult);
											if (SUCCEEDED(hr))
											{
												wprintf(L"[+] Group:\n");
												wprintf(L"    %s\r\n", (LPCWSTR)sbstrResult);
											}

											hr = pIADs->get_ADsPath(&sbstrResult);
											if (SUCCEEDED(hr))
											{
												//wprintf(L"[+] ADsPath:\n");
												//wprintf(L"    %s\r\n", (LPCWSTR)sbstrResult);
											}

											IADsGroup *pGroup = NULL;
											hr = ADsGetObject(sbstrResult, IID_IADsGroup, (void**)&pGroup);
											if (SUCCEEDED(hr)) {
												PrintGroupObjectMembers(pGroup);
											}

											wprintf(L"--------------------------------------------------------------------\n");

											pIADs->Release();
										}
									}

									VariantClear(&Variant);
								}

								pEnumVariant->Release();
							}

						}
					}
					VariantClear(&vFilter);

					// Uninitialize COM
					CoUninitialize();
				}
				
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
