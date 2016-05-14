/*
 * reslist.c - get nameservers from windows *
 *
 * ircd-ratbox related changes are as follows
 *
 * Copyright (C) 2008 Aaron Sethman <androsyn@ratbox.org>
 * Copyright (C) 2008-2012 ircd-ratbox development team
 *
 * pretty much all of this was yanked from c-ares ares_init.c here is the original
 * header from there --
 *
 * Id: ares_init.c,v 1.72 2008-05-15 00:00:19 yangtse Exp $
 * Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2007-2008 by Daniel Stenberg
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 *
 */

#ifdef _WIN32
#include <rb_lib.h>

#include <windows.h>
#include <iphlpapi.h>

const char *get_windows_nameservers(void);

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif /* INADDR_NONE */

#define IS_NT()	       ((int)GetVersion() > 0)
#define WIN_NS_9X      "System\\CurrentControlSet\\Services\\VxD\\MSTCP"
#define WIN_NS_NT_KEY  "System\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define NAMESERVER     "NameServer"
#define DHCPNAMESERVER "DhcpNameServer"
#define DATABASEPATH   "DatabasePath"
#define WIN_PATH_HOSTS	"\\hosts"

static int
get_iphlpapi_dns_info(char *ret_buf, size_t ret_size)
{
	FIXED_INFO *fi = alloca(sizeof(*fi));
	DWORD size = sizeof(*fi);
	typedef DWORD(WINAPI * get_net_param_func) (FIXED_INFO *, DWORD *);
	get_net_param_func xxGetNetworkParams;	/* available only on Win-98/2000+ */
	HMODULE handle;
	IP_ADDR_STRING *ipAddr;
	int i, count = 0;
	int debug = 0;
	size_t ip_size = sizeof("255.255.255.255,") - 1;
	size_t left = ret_size;
	char *ret = ret_buf;
	HRESULT res;

	if(!fi)
		return (0);

	handle = LoadLibrary("iphlpapi.dll");
	if(!handle)
		return (0);

	xxGetNetworkParams = (get_net_param_func) GetProcAddress(handle, "GetNetworkParams");
	if(!xxGetNetworkParams)
		goto quit;

	res = (*xxGetNetworkParams) (fi, &size);
	if((res != ERROR_BUFFER_OVERFLOW) && (res != ERROR_SUCCESS))
		goto quit;

	fi = alloca(size);
	if(!fi || (*xxGetNetworkParams) (fi, &size) != ERROR_SUCCESS)
		goto quit;

	if(debug)
	{
		printf("Host Name: %s\n", fi->HostName);
		printf("Domain Name: %s\n", fi->DomainName);
		printf("DNS Servers:\n" "    %s (primary)\n", fi->DnsServerList.IpAddress.String);
	}
	if(strlen(fi->DnsServerList.IpAddress.String) > 0 &&
	   inet_addr(fi->DnsServerList.IpAddress.String) != INADDR_NONE && left > ip_size)
	{
		ret += sprintf(ret, "%s,", fi->DnsServerList.IpAddress.String);
		left -= ret - ret_buf;
		count++;
	}

	for(i = 0, ipAddr = fi->DnsServerList.Next; ipAddr && left > ip_size;
	    ipAddr = ipAddr->Next, i++)
	{
		if(inet_addr(ipAddr->IpAddress.String) != INADDR_NONE)
		{
			ret += sprintf(ret, "%s,", ipAddr->IpAddress.String);
			left -= ret - ret_buf;
			count++;
		}
		if(debug)
			printf("    %s (secondary %d)\n", ipAddr->IpAddress.String, i + 1);
	}

      quit:
	if(handle)
		FreeLibrary(handle);

	if(debug && left <= ip_size)
		printf("Too many nameservers. Truncating to %d addressess", count);
	if(ret > ret_buf)
		ret[-1] = '\0';
	return (count);
}

/*
 * Warning: returns a dynamically allocated buffer, the user MUST
 * use free() / rb_free() if the function returns 1
 */
static int
get_res_nt(HKEY hKey, const char *subkey, char **obuf)
{
	/* Test for the size we need */
	DWORD size = 0;
	int result;

	result = RegQueryValueEx(hKey, subkey, 0, NULL, NULL, &size);
	if((result != ERROR_SUCCESS && result != ERROR_MORE_DATA) || !size)
		return 0;
	*obuf = rb_malloc(size + 1);
	if(!*obuf)
		return 0;

	if(RegQueryValueEx(hKey, subkey, 0, NULL, (LPBYTE) * obuf, &size) != ERROR_SUCCESS)
	{
		rb_free(*obuf);
		return 0;
	}
	if(size == 1)
	{
		rb_free(*obuf);
		return 0;
	}
	return 1;
}

static int
get_res_interfaces_nt(HKEY hKey, const char *subkey, char **obuf)
{
	char enumbuf[39];	/* GUIDs are 38 chars + 1 for NULL */
	DWORD enum_size = 39;
	int idx = 0;
	HKEY hVal;

	while(RegEnumKeyEx(hKey, idx++, enumbuf, &enum_size, 0,
			   NULL, NULL, NULL) != ERROR_NO_MORE_ITEMS)
	{
		int rc;

		enum_size = 39;
		if(RegOpenKeyEx(hKey, enumbuf, 0, KEY_QUERY_VALUE, &hVal) != ERROR_SUCCESS)
			continue;
		rc = get_res_nt(hVal, subkey, obuf);
		RegCloseKey(hVal);
		if(rc)
			return 1;
	}
	return 0;
}

const char *
get_windows_nameservers(void)
{
	/*
	   NameServer info via IPHLPAPI (IP helper API):
	   GetNetworkParams() should be the trusted source for this.
	   Available in Win-98/2000 and later. If that fail, fall-back to
	   registry information.

	   NameServer Registry:

	   On Windows 9X, the DNS server can be found in:
	   HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\VxD\MSTCP\NameServer

	   On Windows NT/2000/XP/2003:
	   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\NameServer
	   or
	   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\DhcpNameServer
	   or
	   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\{AdapterID}\
	   NameServer
	   or
	   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\{AdapterID}\
	   DhcpNameServer
	 */
	static char namelist[512];
	HKEY mykey;
	HKEY subkey;
	DWORD data_type;
	DWORD bytes;
	DWORD result;
	char *line = NULL;
	memset(&namelist, 0, sizeof(namelist));
	if(get_iphlpapi_dns_info(namelist, sizeof(namelist)) > 0)
	{
		return namelist;
	}

	if(IS_NT())
	{
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0,
				KEY_READ, &mykey) == ERROR_SUCCESS)
		{
			RegOpenKeyEx(mykey, "Interfaces", 0,
				     KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &subkey);
			if(get_res_nt(mykey, NAMESERVER, &line))
			{
				rb_strlcpy(namelist, line, sizeof(namelist));
				return namelist;
			}
			else if(get_res_nt(mykey, DHCPNAMESERVER, &line))
			{
				rb_strlcpy(namelist, line, sizeof(namelist));
				rb_free(line);
			}
			/* Try the interfaces */
			else if(get_res_interfaces_nt(subkey, NAMESERVER, &line))
			{
				rb_strlcpy(namelist, line, sizeof(namelist));
				rb_free(line);
			}
			else if(get_res_interfaces_nt(subkey, DHCPNAMESERVER, &line))
			{
				rb_strlcpy(namelist, line, sizeof(namelist));
				rb_free(line);
			}
			RegCloseKey(subkey);
			RegCloseKey(mykey);
		}
	}
	else
	{
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_NS_9X, 0,
				KEY_READ, &mykey) == ERROR_SUCCESS)
		{
			if((result = RegQueryValueEx(mykey, NAMESERVER, NULL, &data_type,
						     NULL, &bytes)) == ERROR_SUCCESS ||
			   result == ERROR_MORE_DATA)
			{
				if(bytes)
				{
					line = (char *)rb_malloc(bytes + 1);
					if(RegQueryValueEx(mykey, NAMESERVER, NULL, &data_type,
							   (unsigned char *)line, &bytes) ==
					   ERROR_SUCCESS)
					{
						rb_strlcpy(namelist, line, sizeof(namelist));
					}
					free(line);
				}
			}
		}
		RegCloseKey(mykey);
	}
	if(strlen(namelist) > 0)
		return namelist;
	return NULL;
}


#endif
