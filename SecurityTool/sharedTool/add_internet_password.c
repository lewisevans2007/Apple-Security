/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <string.h>
#include <getopt.h>
#include <stdlib.h>

#include <Security/SecItem.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>

#include "SecurityTool/sharedTool/tool_errors.h"

#include "SecurityCommands.h"

#include "tool_auth_helpers.h"

typedef uint32_t SecProtocolType;
typedef uint32_t SecAuthenticationType;

static int
do_addinternetpassword(const char *keychainName, const char *serverName,
                       const char *securityDomain, const char *accountName, const char *path,
                       UInt16 port, SecProtocolType protocol,
                       SecAuthenticationType authenticationType, const void *passwordData)
{
	OSStatus result;
    CFDictionaryRef attributes = NULL;
    const void *keys[9], *values[9];
    CFIndex ix = 0;
    
    keys[ix] = kSecClass;
    values[ix++] = kSecClassInternetPassword;
    
	if (serverName) {
		keys[ix] = kSecAttrServer;
		values[ix++] = CFStringCreateWithCStringNoCopy(NULL, serverName,
                                                       kCFStringEncodingUTF8, kCFAllocatorNull);
	}
	if (securityDomain) {
		keys[ix] = kSecAttrSecurityDomain;
		values[ix++] = CFStringCreateWithCStringNoCopy(NULL, securityDomain,
                                                       kCFStringEncodingUTF8, kCFAllocatorNull);
	}
	if (accountName) {
		keys[ix] = kSecAttrAccount;
		values[ix++] = CFStringCreateWithCStringNoCopy(NULL, accountName,
                                                       kCFStringEncodingUTF8, kCFAllocatorNull);
	}
	if (path) {
		keys[ix] = kSecAttrPath;
		values[ix++] = CFStringCreateWithCStringNoCopy(NULL, path,
                                                       kCFStringEncodingUTF8, kCFAllocatorNull);
	}
    keys[ix] = kSecAttrPort;
    values[ix++] = CFNumberCreate(NULL, kCFNumberSInt16Type, &port);
    /* Protocol is a 4 char code, perhaps we should use a string rep instead. */
    keys[ix] = kSecAttrProtocol;
    values[ix++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &protocol);
    keys[ix] = kSecAttrAuthenticationType;
    values[ix++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &authenticationType);
    
    if (passwordData)
    {
        keys[ix] = kSecValueData;
        values[ix++] = CFDataCreateWithBytesNoCopy(NULL, passwordData,
                                                   strlen(passwordData), kCFAllocatorNull);
    }
    
    attributes = CFDictionaryCreate(NULL, keys, values, ix,
                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
	/* Release the values we just added to the dictionary. */
	/* @@@ For performance reasons we could choose to make the dictionaries
     CFRetain callback a no op and let the dictionary deal with the
     releasing. */
    for (; ix-- > 0;) {
        CFRelease(values[ix]);
    }
    
	result = SecItemAdd(attributes, NULL);
    
    if (attributes) {
        CFRelease(attributes);
    }

	if (result)
	{
		sec_perror("SecItemAdd", result);
	}
    
	return result;
}


int keychain_add_internet_password(int argc, char * const *argv)
{
	char *serverName = NULL, *securityDomain = NULL, *accountName = NULL, *path = NULL, *passwordData  = NULL;
    UInt16 port = 0;
    SecProtocolType protocol = 0;
    SecAuthenticationType authenticationType = 0;
	int ch, result = 0;
	const char *keychainName = NULL;
    bool authSucceeded = false;
    /*
     -s Use servername\n"
     "    -e Use securitydomain\n"
     "    -a Use accountname\n"
     "    -p Use path\n"
     "    -o Use port \n"
     "    -r Use protocol \n"
     "    -c Use SecAuthenticationType  \n"
     "    -w Use passwordData  \n"
     */
	while ((ch = getopt(argc, argv, "s:d:a:p:P:r:t:w:hyY:")) != -1)
	{
		switch  (ch)
		{
            case 's':
                serverName = optarg;
                break;
            case 'd':
                securityDomain = optarg;
                break;
            case 'a':
                accountName = optarg;
                break;
            case 'p':
                path = optarg;
                break;
            case 'P':
                port = atoi(optarg);
                break;
            case 'r':
                memcpy(&protocol,optarg,4);
                //protocol = (SecProtocolType)optarg;
                break;
            case 't':
                memcpy(&authenticationType,optarg,4);
                break;
            case 'w':
                passwordData = optarg;
                break;
            case 'y':
                if (!promptForAndCheckPassphrase()) {
                    return 1;
                }
                authSucceeded = true;
                break;
            case 'Y':
                if (!checkPassphrase(optarg, 0) ) {
                    return 1;
                }
                authSucceeded = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
		}
	}
    
    if (!authSucceeded && authRequired()) {
        return 1;
    }

	argc -= optind;
	argv += optind;
    
	if (argc == 1)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto loser;
		}
	}
	else if (argc != 0)
		return SHOW_USAGE_MESSAGE;
    
	result = do_addinternetpassword(keychainName, serverName, securityDomain,
                                    accountName, path, port, protocol,authenticationType, passwordData);
    
loser:
	return result;
}
