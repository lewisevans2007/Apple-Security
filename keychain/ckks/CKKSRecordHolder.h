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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#include "keychain/securityd/SecDbItem.h"
#include "utilities/SecDb.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"

NS_ASSUME_NONNULL_BEGIN

// Helper class that includes a single encoded CKRecord
@interface CKKSCKRecordHolder : CKKSSQLDatabaseObject


- (instancetype)initWithCKRecord:(CKRecord*)record
                       contextID:(NSString*)contextID;
- (instancetype)initWithCKRecordType:(NSString*)recordType
                     encodedCKRecord:(NSData* _Nullable)encodedCKRecord
                           contextID:(NSString*)contextID
                              zoneID:(CKRecordZoneID*)zoneID;

@property (readonly) NSString* contextID;
@property CKRecordZoneID* zoneID;
@property NSString* ckRecordType;
@property (nullable, copy) NSData* encodedCKRecord;
@property (nullable, copy) CKRecord* storedCKRecord;

- (CKRecord*)CKRecordWithZoneID:(CKRecordZoneID*)zoneID;

// All of the following are virtual: you must override to use
- (NSString*)CKRecordName;
- (CKRecord*)updateCKRecord:(CKRecord*)record zoneID:(CKRecordZoneID*)zoneID;
- (void)setFromCKRecord:(CKRecord*)record;  // When you override this, make sure to call [setStoredCKRecord]
- (bool)matchesCKRecord:(CKRecord*)record;

@end

NS_ASSUME_NONNULL_END
#endif
