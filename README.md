# Recon-AD, an AD recon tool based on ADSI and reflective DLL’s
New monitoring and defense optics are being applied within Microsoft operating systems and security products. This should help defenders in detecting malicious behavior within their environments. While PowerShell has long been very popular for post exploitation, now it’s something attackers try to avoid. .NET is the current hype for offensive tradecraft, but Microsoft is rapidly developing new measures by adding optics to catch malicious behavior on this platform. 

As a proof of concept, we developed an C/C++ Active Directory reconnaissance tool based on ADSI and reflective DLLs which can be used within Cobalt Strike. The tool is called “Recon-AD” and at this moment consist of seven Reflective DLLs and a corresponding aggressor script. This tool should help you moving away from PowerShell and .NET when enumerating Active Directory and help you stay under the radar from the latest monitoring and defense technologies being applied within modern environments.

More info about the used techniques can be found on the following Blog: 
https://outflank.nl/blog/2019/10/20/red-team-tactics-active-directory-recon-using-adsi-and-reflective-dlls/

## The following functionality is included in the toolkit:

```
Recon-AD-Domain: to enumerate Domain information (Domain name, GUID, site name, password policy, DC list e.g.).
Recon-AD-Users: to query for user objects and corresponding attributes.
Recon-AD-Groups: to query for group objects and corresponding attributes.
Recon-AD-Computers: to query for computer objects and corresponding attributes.
Recon-AD-SPNs: to query for user objects with Service Principal Names (SPN) configured and display useful attributes.
Recon-AD-AllLocalGroups: to query a computer for all local groups and group-members.
Recon-AD-LocalGroups: to query a computer for specific local groups and group-members (default Administrators group).
```

## Usage:

```
Download the Outflank-Recon-AD folder and load the Recon-AD.cna script within the Cobalt Strike Script Manager.
Use the Beacon help command to display syntax information.
```

```
This project is written in C/C++
You can use Visual Studio to compile the reflective dll's from source.
```

## Credits
Author: Cornelis de Plaa (@Cneelis) / Outflank

Shout out to: Stan Hegt (@StanHacked) and all my other great collegues at Outflank