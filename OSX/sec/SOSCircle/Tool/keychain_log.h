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

/*
 * keychain_log.h
 * 
 * security tool subcommand (log) for sysdiagnose/ckcdiagnose information.
 *
 */


#include <SecurityTool/security_tool_commands.h>

SECURITY_COMMAND(
                 "synclog", keychain_log,
                 "[options]\n"
                 "    -s     sysdiagnose dump\n"
                 "    -i     info (current status)\n"
                 "    -D     [itemName]  dump contents of KVS\n"
                 "    -L     list all known view and their status\n"
                 "\n",
                 "iCloud Keychain Logging")