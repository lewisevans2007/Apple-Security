/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef kc_key_helpers_h
#define kc_key_helpers_h

#include <CommonCrypto/CommonCryptor.h>

#include "kc-helpers.h"
#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"

#if TARGET_OS_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

static CFMutableDictionaryRef makeBaseKeyDictionary(void) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassKey);
    return query;
}

static CFMutableDictionaryRef createQueryKeyDictionary(SecKeychainRef kc, CFStringRef keyClass) {
    CFMutableDictionaryRef query = makeBaseKeyDictionary();

    CFMutableArrayRef searchList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)searchList, kc);
    CFDictionarySetValue(query, kSecMatchSearchList, searchList);

    CFDictionarySetValue(query, kSecAttrKeyClass, keyClass);

    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);

    CFRelease(searchList); // retained by query dict

    return query;
}

static CFMutableDictionaryRef createQueryKeyDictionaryWithLabel(SecKeychainRef kc, CFStringRef keyClass, CFStringRef label) {
    CFMutableDictionaryRef query = createQueryKeyDictionary(kc, keyClass);
    CFDictionarySetValue(query, kSecAttrLabel, label);
    return query;
}

static CFMutableDictionaryRef createAddKeyDictionaryWithApplicationLabel(SecKeychainRef kc, CFStringRef keyClass, CFStringRef label, CFStringRef applicationLabel) {
    CFMutableDictionaryRef query = makeBaseKeyDictionary();
    CFDictionaryAddValue(query, kSecUseKeychain, kc);

    CFDictionarySetValue(query, kSecAttrLabel, label);
    if(applicationLabel) {
        CFDictionarySetValue(query, kSecAttrApplicationLabel, applicationLabel);
    } else {
        CFDictionarySetValue(query, kSecAttrApplicationLabel, CFSTR("test_application")); // without setting this, it uses the current datetime.
    }

    int32_t n = 0;
    if(CFEqual(keyClass, kSecAttrKeyClassSymmetric)) {
        CFDictionarySetValue(query, kSecAttrKeyType, kSecAttrKeyTypeAES);
        n = 128;
    } else if(CFEqual(keyClass, kSecAttrKeyClassPublic) ||
              CFEqual(keyClass, kSecAttrKeyClassPrivate)) {
        CFDictionarySetValue(query, kSecAttrKeyType, kSecAttrKeyTypeRSA);
        n = 1024;
    }
    CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &n);
    CFDictionarySetValue(query, kSecAttrKeySizeInBits, num);
    CFReleaseNull(num);

    return query;
}
static CFMutableDictionaryRef createAddKeyDictionary(SecKeychainRef kc, CFStringRef keyClass, CFStringRef label) {
    return createAddKeyDictionaryWithApplicationLabel(kc, keyClass, label, NULL);
}

static SecKeyRef createCustomKeyWithApplicationLabel(const char* name, SecKeychainRef kc, CFStringRef label, CFStringRef applicationLabel) {
    CFMutableDictionaryRef query = createAddKeyDictionaryWithApplicationLabel(kc, kSecAttrKeyClassSymmetric, label, applicationLabel);

    CFErrorRef error = NULL;
    SecKeyRef item = SecKeyGenerateSymmetric(query, &error);
    ok(item != NULL, "%s: SecKeyGenerateSymmetric: %ld", name, error ? CFErrorGetCode(error) : 0);

    CFReleaseNull(query);
    return item;
}
#define createCustomKeyWithApplicationLabelTests 1

static SecKeyRef createCustomKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    return createCustomKeyWithApplicationLabel(name, kc, label, NULL);
}
#define createCustomKeyTests createCustomKeyWithApplicationLabelTests

static SecKeyRef makeKey(const char* name, SecKeychainRef kc) {
    return createCustomKey(name, kc, CFSTR("test_key"));
}
#define makeKeyTests createCustomKeyTests

static void makeCustomKeyPair(const char* name, SecKeychainRef kc, CFStringRef label, SecKeyRef* aPub, SecKeyRef* aPriv) {
    CFMutableDictionaryRef query = createAddKeyDictionary(kc, kSecAttrKeyClassPublic, label);

    SecKeyRef pub;
    SecKeyRef priv;
    ok_status(SecKeyGeneratePair(query, &pub, &priv), "%s: SecKeyGeneratePair returned a result", name);

    if(aPub) {
        *aPub = pub;
    }
    if(aPriv) {
        *aPriv = priv;
    }

    CFReleaseNull(query);
}
#define makeCustomKeyPairTests 1

static void makeKeyPair(const char* name, SecKeychainRef kc, SecKeyRef* aPub, SecKeyRef* aPriv) {
    makeCustomKeyPair(name, kc, CFSTR("test_key"), aPub, aPriv);
}
#define makeKeyPairTests makeCustomKeyPairTests

// This only works for symmetric keys; key pairs cannot ever generate a duplicate (due to setting kSecKeyLabel to the hash of the public key)
static void makeCustomDuplicateKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    CFMutableDictionaryRef query;

    query = createAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);
    CFErrorRef error = NULL;
    CFReleaseSafe(SecKeyGenerateSymmetric(query, &error));
    is(CFErrorGetCode(error), errSecDuplicateItem, "%s: SecKeyGenerateSymmetric (duplicate) errored: %ld", name, error ? CFErrorGetCode(error) : -1);

    CFReleaseNull(error);
    CFReleaseNull(query);
}
#define makeCustomDuplicateKeyTests 1

static void makeDuplicateKey(const char* name, SecKeychainRef kc) {
    makeCustomDuplicateKey(name, kc, CFSTR("test_key"));
}
#define makeDuplicateKeyTests makeCustomDuplicateKeyTests

static SecKeyRef makeCustomFreeKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    SecKeyRef symkey;

    ok_status(SecKeyGenerate(
                             NULL,
                             CSSM_ALGID_AES, 128,
                             0, /* contextHandle */
                             CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
                             CSSM_KEYATTR_EXTRACTABLE,
                             NULL, /* initialAccess */
                             &symkey), "%s: SecKeyGenerate", name);

    CFMutableDictionaryRef query = createAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, symkey);

    CFDictionarySetValue(query, kSecUseItemList, itemList);

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(query, &result), "%s: SecItemAdd", name);
    ok(result != NULL, "%s: SecItemAdd returned a result", name);
    CFReleaseNull(symkey);
    return (SecKeyRef) result;
}
#define makeCustomFreeKeyTests 3

static SecKeyRef makeFreeKey(const char* name, SecKeychainRef kc) {
    return makeCustomFreeKey(name, kc, CFSTR("test_free_key"));
}
#define makeFreeKeyTests makeCustomFreeKeyTests

static SecKeyRef makeCustomDuplicateFreeKey(const char* name, SecKeychainRef kc, CFStringRef label) {
    SecKeyRef symkey;

    ok_status(SecKeyGenerate(
                             NULL,
                             CSSM_ALGID_AES, 128,
                             0, /* contextHandle */
                             CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
                             CSSM_KEYATTR_EXTRACTABLE,
                             NULL, /* initialAccess */
                             &symkey), "%s: SecKeyGenerate", name);

    CFMutableDictionaryRef query = createAddKeyDictionary(kc, kSecAttrKeyClassSymmetric, label);

    CFMutableArrayRef itemList = (CFMutableArrayRef) CFArrayCreateMutable(kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue((CFMutableArrayRef)itemList, symkey);

    CFDictionarySetValue(query, kSecUseItemList, itemList);

    CFTypeRef result = NULL;
    is(SecItemAdd(query, &result), errSecDuplicateItem, "%s: SecItemAdd (duplicate)", name);
    CFReleaseNull(symkey);
    return (SecKeyRef) result;
}
#define makeCustomDuplicateFreeKeyTests 2

static SecKeyRef makeDuplicateFreeKey(const char* name, SecKeychainRef kc) {
    return makeCustomFreeKey(name, kc, CFSTR("test_free_key"));
}
#define makeDuplicateFreeKeyTests makeCustomDuplicateFreeKeyTests

static CFDataRef encryptOrDecryptAESCBC(CCOperation op, SecKeyRef key, CFDataRef input, CFErrorRef *error) {
    CCCryptorStatus status = kCCUnspecifiedError;
    CCCryptorRef cryptor = NULL;
    CFMutableDataRef output = NULL;
    CFDataRef outputCopy = NULL;

    CFDataRef keyBytes = NULL;
    OSStatus exportStatus = SecItemExport(key, kSecFormatRawKey, 0, NULL, &keyBytes);
    if (exportStatus != errSecSuccess) {
        if (error) {
            CFStringRef description = SecCopyErrorMessageString(exportStatus, NULL) ?: CFSTR("");
            CFAssignRetained(*error, CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, kCFErrorDomainOSStatus, exportStatus, (const void **)&kCFErrorLocalizedDescriptionKey, (const void **)&description, 1));
            CFReleaseNull(description);
        }
        goto out;
    }

    status = CCCryptorCreate(op, kCCAlgorithmAES, kCCOptionPKCS7Padding, CFDataGetBytePtr(keyBytes), (size_t)CFDataGetLength(keyBytes), NULL, &cryptor);
    if (status != kCCSuccess) {
        CFAssignRetained(*error, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL));
        goto out;
    }
    size_t outputLen = CCCryptorGetOutputLength(cryptor, (size_t)CFDataGetLength(input), true);
    output = CFDataCreateMutableWithScratch(kCFAllocatorDefault, outputLen);
    if (!output) {
        goto out;
    }
    UInt8 *outputPtr = CFDataGetMutableBytePtr(output);
    size_t chunkLen = 0;
    status = CCCryptorUpdate(cryptor, CFDataGetBytePtr(input), (size_t)CFDataGetLength(input), outputPtr, outputLen, &chunkLen);
    if (status != kCCSuccess) {
        CFAssignRetained(*error, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL));
        goto out;
    }
    size_t remainingLen = outputLen - chunkLen;
    size_t finalChunkLen = 0;
    status = CCCryptorFinal(cryptor, &outputPtr[chunkLen], remainingLen, &finalChunkLen);
    if (status != kCCSuccess) {
        CFAssignRetained(*error, CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL));
        goto out;
    }
    CFDataSetLength(output, (CFIndex)(chunkLen + finalChunkLen));

out:
    outputCopy = output ? CFDataCreateCopy(kCFAllocatorDefault, output) : NULL;
    CFReleaseNull(output);
    (void)CCCryptorRelease(cryptor);
    CFReleaseNull(keyBytes);

    return outputCopy;
}

#define checkKeyUseTests 4
static void checkKeyUse(SecKeyRef key, OSStatus expectedStatus) {
    CFStringRef plaintext = CFSTR("A short story: the string goes into the encryptor, and returns unrecognizable. The decryptor reverses.");
    CFDataRef plaintextData = CFDataCreate(NULL, (uint8_t*) CFStringGetCStringPtr(plaintext, kCFStringEncodingUTF8), CFStringGetLength(plaintext));

    /* encrypt first */
    CFErrorRef error = NULL;
    CFDataRef ciphertextData = encryptOrDecryptAESCBC(kCCEncrypt, key, plaintextData, &error);
    CFDataRef roundtripData = NULL;

    if(error) {
        CFStringRef errorStr = CFErrorCopyDescription(error);
        is(CFErrorGetCode(error), expectedStatus, "%s: Encrypting data failed: %d %s (and expected %d)", testName, (int) CFErrorGetCode(error), CFStringGetCStringPtr(errorStr, kCFStringEncodingUTF8), (int) expectedStatus);
        CFReleaseSafe(errorStr);
        CFReleaseNull(error);

        if(expectedStatus != errSecSuccess) {
            // make test numbers match and quit
            for(int i = 1; i < checkKeyUseTests; i++) {
                pass("test numbers match");
            }
            goto cleanup;
        }

    } else {
        pass("%s: transform executed", testName);
    }

    /* and now decrypt */
    roundtripData = encryptOrDecryptAESCBC(kCCDecrypt, key, ciphertextData, &error);
    is(error, NULL, "%s: checkKeyUse: encryptOrDecryptAESCBC (decrypt)", testName);

    if(error) {
        CFStringRef errorStr = CFErrorCopyDescription(error);
        fail("%s: Decrypting data failed: %d %s", testName, (int) CFErrorGetCode(error), CFStringGetCStringPtr(errorStr, kCFStringEncodingUTF8));
        CFReleaseSafe(errorStr);
        CFReleaseNull(error);
    } else {
        pass("%s: make test numbers match", testName);
    }

    eq_cf(plaintextData, roundtripData, "%s: checkKeyUse: roundtripped data is input data", testName);

 cleanup:
    CFReleaseSafe(plaintext);
    CFReleaseSafe(plaintextData);
    CFReleaseSafe(ciphertextData);
    CFReleaseSafe(roundtripData);
}



#pragma clang diagnostic pop

#else

#endif /* TARGET_OS_MAC */


#endif /* kc_key_helpers_h */
