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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Security/Security.h>
#import <Security/SecCertificatePriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-identity-helpers.h"

#include <Security/Security.h>
#include <stdlib.h>

#define BLOCKS 7000

static long concurrentBlocks = 64;

static void tests(void) {

    SecKeychainRef kc = getPopulatedTestKeychain();

    SecIdentityRef identity = NULL;
    SecCertificateRef certRef = NULL;
    SecKeyRef keyRef = NULL;

    identity = copyFirstIdentity(kc);
    ok_status(SecIdentityCopyCertificate(identity, &certRef), "%s: SecIdentityCopyCertificate", testName);
    ok_status(SecIdentityCopyPrivateKey(identity, &keyRef), "%s: SecIdentityCopyPrivateKey", testName);

    CFReleaseNull(identity);
    CFReleaseNull(keyRef);

    static dispatch_once_t onceToken = 0;
    static dispatch_queue_t release_queue = NULL;
    dispatch_once(&onceToken, ^{
        release_queue = dispatch_queue_create("com.apple.security.identity-search-queue", DISPATCH_QUEUE_CONCURRENT);
    });
    dispatch_group_t g = dispatch_group_create();

    __block int iteration = 0;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(concurrentBlocks);

    dispatch_block_t identityBlock = ^{
        SecIdentityRef blockId = NULL;
        SecKeyRef blockKeyRef = NULL;

        ok_status(SecIdentityCreateWithCertificate(kc, certRef, &blockId), "%s: SecIdentityCreateWithCertificate", testName);
        ok_status(SecIdentityCopyPrivateKey(blockId, &blockKeyRef), "%s: SecIdentityCopyPrivateKey", testName);

        CFReleaseNull(blockKeyRef);
        CFReleaseNull(blockId);

        dispatch_semaphore_signal(semaphore);
    };

    // Send this to background queue, so that when we wait, it doesn't block main queue

    dispatch_group_async(g, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        for (iteration = 0; iteration < BLOCKS; iteration++) {
            // Wait until one of our concurrentBlocks "slots" are available
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

            dispatch_group_async(g, release_queue, identityBlock);
        }
    });

    dispatch_group_wait(g, DISPATCH_TIME_FOREVER);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);
}

int kc_20_identity_find_stress(int argc, char *const *argv)
{
    plan_tests(2*BLOCKS + 6);
    initializeKeychainTests(__FUNCTION__);

    tests();

    deleteTestFiles();
    return 0;
}
