#undef  _UNICODE
#define _UNICODE
#undef  UNICODE
#define UNICODE

#include "ReflectiveLoader.h"
#include <winsock2.h>
#include <Windows.h>
#include <stdio.h>
#include <DsGetDC.h>
#include <lm.h>
#include <lmapibuf.h>
#include <Objbase.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Netapi32.lib")

#define DS_NOTIFY_AFTER_SITE_RECORDS 0x02

// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved)
{
	BOOL bReturnValue = TRUE;

	switch (dwReason)
	{
	case DLL_QUERY_HMODULE:
		if (lpReserved != NULL)
			*(HMODULE *)lpReserved = hAppInstance;
		break;
	case DLL_PROCESS_ATTACH:
		hAppInstance = hinstDLL;

		// Get a Domain Controller for the Domain this computer is on.
		DWORD dwRet;
		PDOMAIN_CONTROLLER_INFO pdcInfo;

		dwRet = DsGetDcName(NULL, NULL, NULL, NULL, 0, &pdcInfo);
		if (ERROR_SUCCESS == dwRet)
		{	
			// Open the enumeration.
			HANDLE hGetDc;
			dwRet = DsGetDcOpen(pdcInfo->DomainName,
				DS_NOTIFY_AFTER_SITE_RECORDS,
				NULL,
				NULL,
				NULL,
				0,
				&hGetDc);
			if (ERROR_SUCCESS == dwRet)
			{
				LPTSTR pszDnsHostName;
				GUID guid;
				CoCreateGuid(&guid);

				OLECHAR* guidString;
				StringFromCLSID(pdcInfo->DomainGuid, &guidString);

				wprintf(L"--------------------------------------------------------------------\n");

				wprintf(L"[+] DomainName:\n");
				wprintf(L"    %ls\n", pdcInfo->DomainName);

				wprintf(L"[+] DomainGuid:\n");
				wprintf(L"    %ls\n", guidString);

				wprintf(L"[+] DnsForestName:\n");
				wprintf(L"    %ls\n", pdcInfo->DnsForestName);

				wprintf(L"[+] DcSiteName:\n");
				wprintf(L"    %ls\n", pdcInfo->DcSiteName);

				wprintf(L"[+] ClientSiteName:\n");
				wprintf(L"    %ls\n", pdcInfo->ClientSiteName);

				wprintf(L"[+] DomainControllerName (PDC):\n");
				wprintf(L"    %ls\n", pdcInfo->DomainControllerName);

				wprintf(L"[+] DomainControllerAddress (PDC):\n");
				wprintf(L"    %ls\n", pdcInfo->DomainControllerAddress);

				CoTaskMemFree(guidString);

				// Enumerate Domain password policy.
				DWORD dwLevel = 0;
				USER_MODALS_INFO_0 *pBuf0 = NULL;
				USER_MODALS_INFO_3 *pBuf3 = NULL;
				NET_API_STATUS nStatus;

				// Call the NetUserModalsGet function; specify level 0.
				nStatus = NetUserModalsGet(pdcInfo->DomainControllerName,
					dwLevel,
					(LPBYTE *)&pBuf0);

				// If the call succeeds, print the global information.
				if (nStatus == NERR_Success)
				{
					if (pBuf0 != NULL)
					{
						wprintf(L"[+] Default Domain Password Policy:\n");

						wprintf(L"    Password history length: %d\n", pBuf0->usrmod0_password_hist_len);
						wprintf(L"    Maximum password age (d): %d\n", pBuf0->usrmod0_max_passwd_age / 86400);
						wprintf(L"    Minimum password age (d): %d\n", pBuf0->usrmod0_min_passwd_age / 86400);
						wprintf(L"    Minimum password length: %d\n", pBuf0->usrmod0_min_passwd_len);
					}
				}

				// Free the allocated memory.
				if (pBuf0 != NULL)
					NetApiBufferFree(pBuf0);

				// Call the NetUserModalsGet function; specify level 3.
				dwLevel = 3;
				nStatus = NetUserModalsGet(pdcInfo->DomainControllerName,
					dwLevel,
					(LPBYTE *)&pBuf3);

				// If the call succeeds, print the global information.
				if (nStatus == NERR_Success)
				{
					if (pBuf3 != NULL)
					{
						wprintf(L"[+] Account Lockout Policy:\n");

						wprintf(L"    Account lockout threshold: %d\n", pBuf3->usrmod3_lockout_threshold);
						wprintf(L"    Account lockout duration (m): %d\n", pBuf3->usrmod3_lockout_duration / 60);
						wprintf(L"    Account lockout observation window (m): %d\n", pBuf3->usrmod3_lockout_duration / 60);
					}
				}

				// Free the allocated memory.
				if (pBuf3 != NULL)
					NetApiBufferFree(pBuf3);

				// Enumerate each Domain Controller and print its name.
				wprintf(L"[+] NextDc DnsHostName:\n");

				while (TRUE)
				{
					ULONG ulSocketCount;
					LPSOCKET_ADDRESS rgSocketAddresses;

					dwRet = DsGetDcNext(
						hGetDc,
						&ulSocketCount,
						&rgSocketAddresses,
						&pszDnsHostName);

					if (ERROR_SUCCESS == dwRet)
					{
						wprintf(L"    %ls\n", pszDnsHostName);

						// Free the allocated string.
						NetApiBufferFree(pszDnsHostName);

						// Free the socket address array.
						LocalFree(rgSocketAddresses);
					}
					else if (ERROR_NO_MORE_ITEMS == dwRet)
					{
						// The end of the list has been reached.
						break;
					}
					else if (ERROR_FILEMARK_DETECTED == dwRet)
					{
						/*
						DS_NOTIFY_AFTER_SITE_RECORDS was specified in
						DsGetDcOpen and the end of the site-specific
						records was reached.
						*/
						wprintf(L"[+] End of site-specific Domain Controllers.\n");
						continue;
					}
					else
					{
						// Some other error occurred.
						break;
					}
				}

				wprintf(L"--------------------------------------------------------------------\n");

				// Close the enumeration.
				DsGetDcClose(hGetDc);
			}

			// Free the DOMAIN_CONTROLLER_INFO structure. 
			NetApiBufferFree(pdcInfo);
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
