/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include "corecrypto/ccsha1.h"
#include "corecrypto/ccrsa_priv.h"
#include "corecrypto/ccrng.h"
#include "corecrypto/ccn.h"
#include "stdio.h"
#include "misc.h"
#include "Utilities.h"

// corecrypto headers don't like C++ (on a deaper level then extern "C" {} can fix
// so we need a C "shim" for all our corecrypto use.

CFDataRef oaep_padding_via_c(int desired_message_length, CFDataRef dataValue);
CFDataRef oaep_padding_via_c(int desired_message_length, CFDataRef dataValue) CF_RETURNS_RETAINED
{
    size_t pBufferSize = ccn_sizeof_size(desired_message_length);
	cc_unit *paddingBuffer = malloc(pBufferSize);
    if (paddingBuffer == NULL){
        return (void*)GetNoMemoryErrorAndRetain();
    }
    
	bzero(paddingBuffer, pBufferSize); // XXX needed??

    ccrsa_oaep_encode(ccsha1_di(),
                      ccrng(NULL),
                      pBufferSize, (cc_unit*)paddingBuffer,
                      CFDataGetLength(dataValue), CFDataGetBytePtr(dataValue));
    ccn_swap(ccn_nof_size(pBufferSize), (cc_unit*)paddingBuffer);
	
    CFDataRef paddedValue = CFDataCreate(NULL, (UInt8*)paddingBuffer, desired_message_length);
    free(paddingBuffer);
    return paddedValue ? paddedValue : (void*)GetNoMemoryErrorAndRetain();
}

CFDataRef oaep_unpadding_via_c(CFDataRef encodedMessage);
CFDataRef oaep_unpadding_via_c(CFDataRef encodedMessage) CF_RETURNS_RETAINED
{
	size_t mlen = CFDataGetLength(encodedMessage);
	size_t pBufferSize = ccn_sizeof_size(mlen);
    cc_unit *paddingBuffer = malloc(pBufferSize);
	UInt8 *plainText = malloc(mlen);
    if (plainText == NULL || paddingBuffer == NULL) {
        free(plainText);
        free(paddingBuffer);
        return (void*)GetNoMemoryErrorAndRetain();
    }
	
	ccn_read_uint(ccn_nof_size(mlen), paddingBuffer, mlen, CFDataGetBytePtr(encodedMessage));
	size_t plainTextLength = mlen;
    int err = ccrsa_oaep_decode(ccsha1_di(), &plainTextLength, plainText, mlen, paddingBuffer);
	
    if (err) {
		// XXX should make a CFError or something.
        CFErrorRef error = fancy_error(CFSTR("CoreCrypto"), err, CFSTR("OAEP decode error"));
        CFRetainSafe(error);
        free(plainText);
        free(paddingBuffer);
        return (void*)error;
    }
	
    CFDataRef result = CFDataCreate(NULL, (UInt8*)plainText, plainTextLength);
    
    free(plainText);
    free(paddingBuffer);
    
    return result;
}
